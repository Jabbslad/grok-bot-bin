# grok-bot-bin (unofficial)

Unofficial Arch Linux package for the [Grok Bot](https://cursor.com) desktop
agent. Not affiliated with or endorsed by SpaceXAI/Cursor. It repackages the
official `.deb` published by upstream — no modifications to the app itself.

Grok Bot's built-in updater does not support Linux, so CI here checks
Cursor's update API daily and repackages the matching Linux `.deb` —
releases track upstream stable, usually within a day.

## Install from a GitHub release

Grab the newest `.pkg.tar.zst` from the [releases
page](https://github.com/Jabbslad/grok-bot-bin/releases) and:

```sh
pacman -U grok-bot-bin-0.24.0-1-x86_64.pkg.tar.zst  # version shown as an example
```

The package is not in the AUR, so there is no auto-update: to upgrade, just
install the newer release the same way.

Publishing is fully automated: `.github/workflows/update.yml` checks the
update API daily, bumps the PKGBUILD and tags `v<pkgver>`;
`.github/workflows/build.yml` builds on every `v*` tag push (or manual
dispatch) and attaches the `.pkg.tar.zst` to the matching release.

The version is pinned (with a real sha256) for predictable builds — upstream
publishes the linux `.deb` without notice and does not officially support
Linux in its updater.

## Notes

- `/usr/bin/grok-bot` is a plain wrapper replacing the deb's
  `update-alternatives` symlink. No Chromium flags: 0.24.0 runs fully
  sandboxed (the 0.16.0-era renderer SIGILL crashes were fixed upstream).
- User data lives in `~/.config/Grok Bot/` and is untouched by upgrades.
