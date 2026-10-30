#!/usr/bin/env bash
set -e

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

VERSION_STR=$(cat "$ROOT_DIR/VERSION" | tr -d ' \n\r')
DMG_NAME="PlatoLives-${VERSION_STR}-macOS.dmg"

# 1. Pulizia file temporanei e backup non tracciati
rm -f build.log platolives-mac/mac_view.m.bak_target11 platolives-win/win_main.c.bak_theme_refresh platolives_audit.txt tek_rosetta.png

# 2. Creazione bundle DMG macOS con VERSION e README
echo "=== Creazione ${DMG_NAME} ==="
rm -rf build/dmg_temp "build/${DMG_NAME}"
mkdir -p build/dmg_temp
cp -R build/PlatoLives.app build/dmg_temp/
cp LICENSE build/dmg_temp/LICENSE.txt
cp README.md build/dmg_temp/README.md
cp README.md build/dmg_temp/README.txt
cp VERSION build/dmg_temp/VERSION
cp VERSION build/dmg_temp/VERSION.txt
ln -s /Applications build/dmg_temp/Applications

hdiutil create -volname "PlatoLives ${VERSION_STR}" -srcfolder build/dmg_temp -ov -format UDZO "build/${DMG_NAME}"
rm -rf build/dmg_temp
echo "[OK] DMG creato con successo in build/${DMG_NAME}"

# 3. Git Add, Commit e Push
echo ""
echo "=== Git Commit & Push v${VERSION_STR} ==="
git add -A
git commit -m "Release v${VERSION_STR}: DEC PDP-1 Type 30 Precision CRT Display Engine & Cross-Platform Parity" || true
git tag -f -a "v${VERSION_STR}" -m "PlatoLives ${VERSION_STR} - DEC PDP-1 Type 30 Engine & Multiplatform Release"
git push origin HEAD --tags --force
echo "[OK] Commit e Tag v${VERSION_STR} inviati su GitHub con successo!"

# 4. Pubblicazione asset GitHub Release tramite gh CLI
if command -v gh >/dev/null 2>&1; then
    echo ""
    echo "=== Creazione GitHub Release tramite gh CLI ==="
    gh release create "v${VERSION_STR}" \
        "build/${DMG_NAME}" \
        build/PlatoLives-windows-x86_64.exe \
        build/PlatoLives-windows-arm64.exe \
        build/PlatoLives-linux-gui-arm64 \
        build/PlatoLives-linux-gui-x86_64 \
        build/PlatoLives-linux-desktop-install.tar.gz \
        build/PlatoLives-console-linux-arm64 \
        build/PlatoLives-console-linux-x86_64 \
        --title "PlatoLives v${VERSION_STR}" \
        --notes-file RELEASE || echo "[!] gh release create non riuscito o richiede sblocco keychain."
fi
