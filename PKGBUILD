# Unofficial repackage of the official grok-bot_<version>_amd64.deb
# (electron-builder Debian package). Pinned to a known-good release for
# predictable builds.
#
# To bump to a new release, edit pkgver, _commit and sha256sums below, then
# makepkg -si. Find the latest version via the linux-x64 update feed (its
# download URL carries the release commit):
#   https://api2.cursor.sh/updates/api/update/linux-x64/sand/0.0.0/<uuid>/stable
# and download the linux .deb from:
#   https://downloads.cursor.com/grokbot/stable/<commit>/linux/x64/grok-bot_<version>_amd64.deb
pkgname=grok-bot-bin
pkgver=0.39.0
_commit=d8bc9c753edddb313047c9c69b480b7f8f321087
pkgrel=1
pkgdesc="Grok Bot desktop agent"
arch=('x86_64')
url="https://cursor.com"
license=('custom:unknown')
depends=(
  'gtk3'
  'libnotify'
  'nss'
  'libxss'
  'libxtst'
  'xdg-utils'
  'at-spi2-core'
  'util-linux-libs'
  'libsecret'
)
optdepends=('libappindicator-gtk3: system tray icon support')
source=("https://downloads.cursor.com/grokbot/stable/${_commit}/linux/x64/grok-bot_${pkgver}_amd64.deb")
noextract=("grok-bot_${pkgver}_amd64.deb")
options=('!debug')
sha256sums=('ae498adaf71f9ff1734a786163c5807fa50165acb5aa102bb68f43ead96de4fe')

package() {
  # Extract the data payload straight out of the .deb (ar archive)
  bsdtar -xOf "${srcdir}/grok-bot_${pkgver}_amd64.deb" data.tar.xz \
    | bsdtar -xJf - -C "${pkgdir}"

  # The deb's postinst registers /usr/bin/<name> via update-alternatives;
  # ship a plain wrapper instead of a symlink.
  # Upstream renamed the binary sand -> grok-bot in 0.24.0 and the .deb to
  # grok-bot_<version>_amd64.deb in 0.30.0.
  # No Chromium flags: 0.24.0 runs fine fully sandboxed here (the 0.16.0-era
  # SIGILL renderer crashes are fixed upstream; verified 2026-08-22 on Omarchy).
  # If the computer-view pane ever black-screens again, re-add:
  #   --disable-dev-shm-usage --no-sandbox --disable-gpu-sandbox
  if [[ -f "${pkgdir}/opt/Grok Bot/grok-bot" ]]; then
    _bin=grok-bot
  else
    _bin=sand
  fi

  install -dm755 "${pkgdir}/usr/bin"
  cat > "${pkgdir}/usr/bin/${_bin}" <<EOF
#!/bin/sh
exec "/opt/Grok Bot/${_bin}" "\$@"
EOF
  chmod 755 "${pkgdir}/usr/bin/${_bin}"

  sed -i "s|^Exec=.*|Exec=/usr/bin/${_bin} %U|" \
    "${pkgdir}/usr/share/applications/${_bin}.desktop"

  # The deb's postinst makes chrome-sandbox SUID only when the kernel lacks
  # user namespaces; SUID unconditionally works everywhere (electron-builder
  # convention used by other Arch Electron repackages).
  chmod 4755 "${pkgdir}/opt/Grok Bot/chrome-sandbox"

  # AppArmor profile is intentionally not installed: it is an unconfined
  # profile (no-op) and Omarchy/Arch does not enforce AppArmor by default.
  # Desktop/mime/icon cache refreshes are handled by pacman hooks.

  install -Dm644 "${pkgdir}/opt/Grok Bot/LICENSE.electron.txt" \
    "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE.electron.txt"
}
