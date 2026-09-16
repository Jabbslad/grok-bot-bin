# grok-bot-bin (unofficial)

Unofficial Arch Linux package for the [Grok Bot](https://cursor.com) desktop
agent. Not affiliated with or endorsed by SpaceXAI/Cursor. It repackages the
official `.deb` published by upstream — no modifications to the app itself.

Grok Bot's built-in updater does not support Linux. CI checks Cursor's update
API daily, pins the new version and SHA256, then builds and publishes a signed
Arch package. Updates normally follow upstream stable within a day.

## Install from the jabbslad repository

For x86_64 Arch Linux. Download the public signing key:

```sh
curl -fLO https://github.com/Jabbslad/grok-bot-bin/releases/download/pacman-repo/repository-key.asc
gpg --show-keys --with-fingerprint repository-key.asc
```

Verify the fingerprint against this value before trusting the key:

```text
98E6 75CA DED4 8D04 F9BA C58D 7D9F 20A3 E0B8 6587
```

```sh
sudo pacman-key --add repository-key.asc
sudo pacman-key --lsign-key 98E675CADED48D04F9BAC58D7D9F20A3E0B86587
sudo cp -a /etc/pacman.conf "/etc/pacman.conf.backup.$(date +%Y%m%d%H%M%S)"
```

Add this section once to `/etc/pacman.conf`:

```ini
[jabbslad]
SigLevel = Required
Server = https://github.com/Jabbslad/grok-bot-bin/releases/download/pacman-repo
```

Install with a full system upgrade (avoid partial upgrades):

```sh
sudo pacman -Syu jabbslad/grok-bot-bin
```

Future `pacman -Syu` or `yay -Syu` upgrades include this package. Nothing runs
unattended on your machine. An existing local installation is the same package
and does not need to be removed; user data is preserved.

An independently maintained `grok-bot-bin` also exists in AUR. This configured
binary repository supplies the package instead. If another binary repository
provides the same name, put `[jabbslad]` before that repository to prefer this one.

## Publishing

- `.github/workflows/update.yml` checks daily, updates the PKGBUILD and tags
  `v<pkgver>`, then explicitly dispatches the build workflow.
- `.github/workflows/build.yml` builds on `v*` tags or manual dispatch, signs
  packages and the repository database, and publishes to the dedicated
  `pacman-repo` GitHub release. Version-tag builds also attach the package and
  signature to the corresponding version release.
- Packages are uploaded before the database. Older package assets are retained
  for clients with cached databases. Database/signature uploads are not atomic;
  if a refresh overlaps publication and fails signature verification, retry
  shortly afterward. Do not disable signature verification.
- Builds publish serially. Dispatch the current `main` branch to rebuild;
  do not dispatch old tags, which could publish an older database.

GitHub Actions configuration:

- Secret `REPO_SIGNING_KEY`: ASCII-armored private key dedicated to this
  repository, without a passphrase for unattended signing.
- Variable `REPO_KEY_FINGERPRINT`: full signing-key fingerprint shown above.
- `repository-key.asc`: public key only, safe to commit and distribute.

The private key must never be committed. Keep a secure backup of it and its
revocation certificate. Anyone with access to this workflow's signing secret
can publish trusted packages; limit repository write access accordingly.

For a fork, generate your own dedicated signing key, configure the secret and
variable, and replace the public key, fingerprint and repository URLs here.

## Manual installation or local build

Download a `.pkg.tar.zst` and its `.sig` from a recent [version
release](https://github.com/Jabbslad/grok-bot-bin/releases), import and trust the
key as above, then use `sudo pacman -U ./grok-bot-bin-<version>-1-x86_64.pkg.tar.zst`.
Older releases predating repository signing have no signatures.

Alternatively, run `makepkg -si` in this checkout. This repackages upstream's
binary; it does not compile the application from source.

## Notes

- `/usr/bin/grok-bot` is a plain wrapper replacing the deb's
  `update-alternatives` symlink, with no extra Chromium flags.
- User data lives in `~/.config/Grok Bot/` and is untouched by upgrades.
