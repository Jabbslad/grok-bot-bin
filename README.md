# grok-bot-bin (unofficial)

Unofficial Arch Linux package for the [Grok Bot](https://cursor.com) desktop
agent. Not affiliated with or endorsed by SpaceXAI/Cursor. It repackages the
official `.deb` published by upstream — no modifications to the app itself.

Grok Bot's built-in updater does not support Linux, so this PKGBUILD resolves
the latest stable version at build time from Cursor's update API and
repackages the matching Linux `.deb`.

## Update to a new release

CI (`.github/workflows/update.yml`) checks the update API daily, bumps
`pkgver`/`_commit`/`sha256sums` for any new release, tags `v<pkgver>` and
triggers the build — so new releases are normally published automatically.

To check for / move to a new version manually (or to review what the bot
did):

1. Check the latest stable via the update API (win32 feed as oracle):
   `curl -s 'https://api2.cursor.sh/updates/api/update/win32-x64-user/sand/0.0.0/00000000-0000-0000-0000-000000000000/stable'`
2. Update `pkgver` and `_commit` in the PKGBUILD from the response
3. Download the linux `.deb` from
   `https://downloads.cursor.com/grokbot/stable/<commit>/linux/x64/Grok_Bot_<version>.deb`,
   put its sha256 into `sha256sums`, then:

```sh
makepkg -si
```

(Or just ask an agent to do this — the steps above are the full procedure.)

## Install from a GitHub release

CI (`.github/workflows/build.yml`) builds the package on every `v*` tag push
(or manual dispatch) and attaches the `.pkg.tar.zst` to the matching release:

```sh
pacman -U https://github.com/Jabbslad/grok-bot-bin/releases/download/v0.24.0/grok-bot-bin-0.24.0-1-x86_64.pkg.tar.zst
```

Releases are normally cut automatically by the daily update workflow; to do
it by hand: bump `pkgver`/`_commit`/`sha256sums` in the PKGBUILD, commit, then
`git tag v<pkgver> && git push --tags` (a manually pushed tag triggers the
build on its own — no extra dispatch needed).

The version is pinned (with a real sha256) for predictable builds — upstream
publishes the linux `.deb` without notice and does not officially support
Linux in its updater.

## Notes

- `/usr/bin/grok-bot` is a plain wrapper replacing the deb's
  `update-alternatives` symlink. No Chromium flags: 0.24.0 runs fully
  sandboxed (the 0.16.0-era renderer SIGILL crashes were fixed upstream).
- User data lives in `~/.config/Grok Bot/` and is untouched by upgrades.
