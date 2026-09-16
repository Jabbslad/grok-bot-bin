#include <errno.h>
#include <signal.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <libdbusmenu-glib/server.h>
#include <libdbusmenu-gtk/parser.h>

static GPid grok_pid;
static const char *grok_executable;

static void show_grok(GtkMenuItem *item, gpointer data) {
  gchar *argv[] = {(gchar *)grok_executable, NULL};
  GError *error = NULL;

  (void)item;
  (void)data;

  /* Grok is single-instance; launching it again presents the existing window. */
  if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_DEFAULT, NULL, NULL, NULL,
                     &error)) {
    g_warning("Could not show Grok Bot: %s", error->message);
    g_error_free(error);
  }
}

static void copy_version(GtkMenuItem *item, gpointer data) {
  (void)item;

  gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), data, -1);
}

static void open_data_folder(GtkMenuItem *item, gpointer data) {
  gchar *argv[] = {"xdg-open", (gchar *)data, NULL};
  GError *error = NULL;

  (void)item;

  if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL,
                     &error)) {
    g_warning("Could not open data folder: %s", error->message);
    g_error_free(error);
  }
}

static void quit_grok(GtkMenuItem *item, gpointer data) {
  (void)item;
  (void)data;

  kill(grok_pid, SIGTERM);
  gtk_main_quit();
}

static gboolean watch_grok(gpointer data) {
  (void)data;

  if (kill(grok_pid, 0) == -1 && errno == ESRCH) {
    gtk_main_quit();
    return G_SOURCE_REMOVE;
  }

  return G_SOURCE_CONTINUE;
}

#define TRAY_PATH "/StatusNotifierItem"
#define MENU_PATH TRAY_PATH "/Menu"
#define ICON_DIR "/usr/share/icons/hicolor/128x128/apps"

/* AppIndicator is menu-only; export Activate ourselves for left-click Show. */
static const char tray_xml[] =
  "<node><interface name='org.kde.StatusNotifierItem'>"
  "<method name='Activate'><arg type='i' direction='in'/><arg type='i' direction='in'/></method>"
  "<method name='SecondaryActivate'><arg type='i' direction='in'/><arg type='i' direction='in'/></method>"
  "<method name='ContextMenu'><arg type='i' direction='in'/><arg type='i' direction='in'/></method>"
  "<method name='Scroll'><arg type='i' direction='in'/><arg type='s' direction='in'/></method>"
  "<property name='Category' type='s' access='read'/>"
  "<property name='Id' type='s' access='read'/>"
  "<property name='Title' type='s' access='read'/>"
  "<property name='Status' type='s' access='read'/>"
  "<property name='IconName' type='s' access='read'/>"
  "<property name='IconThemePath' type='s' access='read'/>"
  "<property name='ItemIsMenu' type='b' access='read'/>"
  "<property name='Menu' type='o' access='read'/>"
  "</interface></node>";

static void tray_method(GDBusConnection *connection, const gchar *sender,
                        const gchar *path, const gchar *interface,
                        const gchar *method, GVariant *parameters,
                        GDBusMethodInvocation *invocation, gpointer data) {
  (void)connection;
  (void)sender;
  (void)path;
  (void)interface;
  (void)parameters;

  if (g_str_equal(method, "Activate") || g_str_equal(method, "SecondaryActivate"))
    show_grok(NULL, NULL);
  else if (g_str_equal(method, "ContextMenu"))
    /* Most hosts render Menu themselves; retain a GTK popup fallback. */
    gtk_menu_popup_at_pointer(GTK_MENU(data), NULL);
  /* Scrolling has no action. */
  g_dbus_method_invocation_return_value(invocation, NULL);
}

static GVariant *tray_property(GDBusConnection *connection, const gchar *sender,
                               const gchar *path, const gchar *interface,
                               const gchar *property, GError **error,
                               gpointer data) {
  (void)connection;
  (void)sender;
  (void)path;
  (void)interface;
  (void)error;
  (void)data;

  if (g_str_equal(property, "Category")) return g_variant_new_string("ApplicationStatus");
  if (g_str_equal(property, "Id")) return g_variant_new_string("grok-bot");
  if (g_str_equal(property, "Title")) return g_variant_new_string("Grok Bot");
  if (g_str_equal(property, "Status")) return g_variant_new_string("Active");
  if (g_str_equal(property, "IconName")) return g_variant_new_string("grok-bot");
  if (g_str_equal(property, "IconThemePath")) return g_variant_new_string(ICON_DIR);
  if (g_str_equal(property, "ItemIsMenu")) return g_variant_new_boolean(FALSE);
  if (g_str_equal(property, "Menu")) return g_variant_new_object_path(MENU_PATH);
  return NULL;
}

static void registered(GObject *source, GAsyncResult *result, gpointer data) {
  GError *error = NULL;
  GVariant *reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, &error);
  (void)data;

  if (reply) {
    g_variant_unref(reply);
  } else {
    g_warning("Could not register Grok Bot tray: %s", error->message);
    g_error_free(error);
  }
}

static void watcher_appeared(GDBusConnection *connection, const gchar *name,
                             const gchar *owner, gpointer data) {
  (void)name;
  (void)data;

  /* Register again if the bar / StatusNotifierWatcher restarts. */
  g_dbus_connection_call(connection, owner, "/StatusNotifierWatcher",
                         "org.kde.StatusNotifierWatcher", "RegisterStatusNotifierItem",
                         g_variant_new("(s)", TRAY_PATH), NULL,
                         G_DBUS_CALL_FLAGS_NONE, -1, NULL, registered, NULL);
}

int main(int argc, char **argv) {
  char *end = NULL;
  long parsed_pid;
  GError *error = NULL;
  GDBusConnection *connection;
  GDBusNodeInfo *node;
  DbusmenuServer *server;
  DbusmenuMenuitem *menu_root;
  guint registration;
  guint watcher;
  static const GDBusInterfaceVTable vtable = {
    .method_call = tray_method,
    .get_property = tray_property,
  };
  GtkWidget *menu;
  GtkWidget *version_item;
  GtkWidget *separator;
  GtkWidget *show_item;
  GtkWidget *open_folder_item;
  GtkWidget *quit_item;
  gchar *version_label;
  gchar *data_dir;

  if (argc != 4)
    return EXIT_FAILURE;

  parsed_pid = strtol(argv[1], &end, 10);
  if (*argv[1] == '\0' || *end != '\0' || parsed_pid <= 0)
    return EXIT_FAILURE;

  grok_pid = (GPid)parsed_pid;
  grok_executable = argv[2];

  gtk_init(&argc, &argv);

  connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
  if (!connection) {
    g_printerr("Could not connect Grok Bot tray to session bus: %s\n", error->message);
    g_error_free(error);
    return EXIT_FAILURE;
  }

  menu = gtk_menu_new();
  version_label = g_strdup_printf("Version %s", argv[3]);
  version_item = gtk_menu_item_new_with_label(version_label);
  separator = gtk_separator_menu_item_new();
  show_item = gtk_menu_item_new_with_label("Show Grok Bot");
  open_folder_item = gtk_menu_item_new_with_label("Open data folder");
  quit_item = gtk_menu_item_new_with_label("Quit Grok Bot");
  data_dir = g_build_filename(g_get_user_config_dir(), "Grok Bot", NULL);
  g_signal_connect(show_item, "activate", G_CALLBACK(show_grok), NULL);
  g_signal_connect(open_folder_item, "activate", G_CALLBACK(open_data_folder),
                   data_dir);
  g_signal_connect(quit_item, "activate", G_CALLBACK(quit_grok), NULL);
  /* Clicking the version item copies the bare version string. */
  g_signal_connect(version_item, "activate", G_CALLBACK(copy_version),
                   argv[3]);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), show_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), open_folder_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), version_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), quit_item);
  gtk_widget_show_all(menu);
  g_free(version_label);

  server = dbusmenu_server_new(MENU_PATH);
  menu_root = dbusmenu_gtk_parse_menu_structure(menu);
  dbusmenu_server_set_root(server, menu_root);
  g_object_unref(menu_root);
  node = g_dbus_node_info_new_for_xml(tray_xml, NULL);
  registration = g_dbus_connection_register_object(connection, TRAY_PATH,
    node->interfaces[0], &vtable, menu, NULL, &error);
  if (!registration) {
    g_printerr("Could not export Grok Bot tray: %s\n", error->message);
    g_error_free(error);
    return EXIT_FAILURE;
  }
  watcher = g_bus_watch_name_on_connection(connection, "org.kde.StatusNotifierWatcher",
    G_BUS_NAME_WATCHER_FLAGS_NONE, watcher_appeared, NULL, NULL, NULL);

  g_timeout_add(500, watch_grok, NULL);
  gtk_main();

  g_bus_unwatch_name(watcher);
  g_dbus_connection_unregister_object(connection, registration);
  g_dbus_node_info_unref(node);
  g_object_unref(server);
  gtk_widget_destroy(menu);
  g_free(data_dir);
  g_object_unref(connection);
  return EXIT_SUCCESS;
}
