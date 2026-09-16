#!/usr/bin/env python3
"""Run with a compiled tray helper: python tests/test-tray.py /path/to/helper.

Requires python-gobject, GTK 3, dbus-run-session and a graphical session.
Uses a private bus, a dummy app and mock executables; does not touch Grok or
register a tray item with the user's bar.
"""
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time

if len(sys.argv) == 2:
    sys.exit(subprocess.run([
        "dbus-run-session", "--", sys.executable, __file__,
        "--private-bus", str(Path(sys.argv[1]).resolve()),
    ]).returncode)
if len(sys.argv) != 3 or sys.argv[1] != "--private-bus":
    sys.exit(__doc__)

from gi.repository import Gio, GLib

bus = Gio.bus_get_sync(Gio.BusType.SESSION)
watcher_name = "org.kde.StatusNotifierWatcher"
tray_interface = "org.kde.StatusNotifierItem"
registrations = []


def call(destination, path, interface, method, parameters=None):
    return bus.call_sync(destination, path, interface, method, parameters,
                         None, Gio.DBusCallFlags.NONE, 3000, None).unpack()


def wait_for(condition):
    deadline = time.monotonic() + 5
    while not condition():
        if time.monotonic() >= deadline:
            raise AssertionError("Timed out waiting for tray event")
        while GLib.MainContext.default().iteration(False):
            pass
        time.sleep(0.01)


def register(connection, sender, path, interface, method, parameters, invocation):
    assert method == "RegisterStatusNotifierItem"
    registrations.append((sender, parameters.unpack()[0]))
    invocation.return_value(None)


watcher_info = Gio.DBusNodeInfo.new_for_xml("""
<node><interface name="org.kde.StatusNotifierWatcher">
<method name="RegisterStatusNotifierItem"><arg type="s" direction="in"/></method>
</interface></node>
""")
bus.register_object("/StatusNotifierWatcher", watcher_info.interfaces[0], register, None, None)


def own_watcher():
    call("org.freedesktop.DBus", "/org/freedesktop/DBus",
         "org.freedesktop.DBus", "RequestName", GLib.Variant("(su)", (watcher_name, 0)))


with tempfile.TemporaryDirectory(prefix="grok-tray-test-") as directory:
    work = Path(directory)
    actions = work / "actions"
    actions.touch()
    executable = work / "grok"
    executable.write_text('#!/bin/sh\nprintf "show\\n" >> "$TRAY_TEST_ACTIONS"\n')
    executable.chmod(0o755)
    opener = work / "xdg-open"
    opener.write_text('#!/bin/sh\nprintf "folder:%s\\n" "$1" >> "$TRAY_TEST_ACTIONS"\n')
    opener.chmod(0o755)
    env = dict(os.environ, TRAY_TEST_ACTIONS=str(actions),
               PATH=str(work) + os.pathsep + os.environ["PATH"],
               XDG_CONFIG_HOME=str(work / "config"), GIO_USE_VFS="local", NO_AT_BRIDGE="1")
    app = subprocess.Popen(["sleep", "60"])
    tray = subprocess.Popen([sys.argv[2], str(app.pid), str(executable), "0.55.0"], env=env)
    try:
        # Start the watcher after the tray to also cover a late-starting bar.
        own_watcher()
        wait_for(lambda: len(registrations) == 1)
        destination, path = registrations[0]
        properties = call(destination, path, "org.freedesktop.DBus.Properties", "GetAll",
                          GLib.Variant("(s)", (tray_interface,)))[0]
        assert properties["ItemIsMenu"] is False
        assert properties["IconName"] == "grok-bot"
        assert properties["IconThemePath"] == "/usr/share/icons/hicolor/128x128/apps"

        for method, count in (("Activate", 1), ("SecondaryActivate", 2)):
            call(destination, path, tray_interface, method, GLib.Variant("(ii)", (0, 0)))
            wait_for(lambda: actions.read_text().count("show\n") == count)

        menu_path = properties["Menu"]
        _, layout = call(destination, menu_path, "com.canonical.dbusmenu", "GetLayout",
                         GLib.Variant("(iias)", (0, -1, [])))
        items = layout[2]
        labels = [item[1].get("label") for item in items]
        assert labels == ["Show Grok Bot", "Open data folder", "Version 0.55.0", None, "Quit Grok Bot"], labels
        assert items[3][1]["type"] == "separator"

        def click(item):
            call(destination, menu_path, "com.canonical.dbusmenu", "Event",
                 GLib.Variant("(isvu)", (item[0], "clicked", GLib.Variant("i", 0), 0)))

        click(items[0])
        wait_for(lambda: actions.read_text().count("show\n") == 3)
        click(items[1])
        wait_for(lambda: f"folder:{work}/config/Grok Bot\n" in actions.read_text())

        call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
             "ReleaseName", GLib.Variant("(s)", (watcher_name,)))
        own_watcher()
        wait_for(lambda: len(registrations) == 2)
        assert registrations[1] == registrations[0]

        click(items[4])
        assert app.wait(timeout=5) == -signal.SIGTERM
        assert tray.wait(timeout=5) == 0
        print("PASS: left/middle activation, icon fallback, right-click menu actions, watcher restart, Quit")

        # Closing the app independently also removes its tray helper.
        app = subprocess.Popen(["sleep", "60"])
        tray = subprocess.Popen([sys.argv[2], str(app.pid), str(executable), "0.55.0"], env=env)
        wait_for(lambda: len(registrations) == 3)
        app.terminate()
        app.wait(timeout=5)
        assert tray.wait(timeout=5) == 0
        print("PASS: tray exits when the app closes")
    finally:
        for process in (tray, app):
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)
