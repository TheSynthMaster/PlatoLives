#!/usr/bin/env bash
set -e

# 1. Pulizia file temporanei e backup non tracciati
rm -f build.log platolives-mac/mac_view.m.bak_target11 platolives-win/win_main.c.bak_theme_refresh

# 2. Creazione bundle DMG macOS
echo "=== Creazione PlatoLives-5.0.0-macOS.dmg ==="
rm -rf build/dmg_temp build/PlatoLives-5.0.0-macOS.dmg
mkdir -p build/dmg_temp
cp -R build/PlatoLives.app build/dmg_temp/
cp LICENSE build/dmg_temp/LICENSE.txt
cp README.md build/dmg_temp/README.txt
ln -s /Applications build/dmg_temp/Applications

hdiutil create -volname "PlatoLives 5.0.0" -srcfolder build/dmg_temp -ov -format UDZO build/PlatoLives-5.0.0-macOS.dmg
rm -rf build/dmg_temp
echo "[OK] DMG creato con successo in build/PlatoLives-5.0.0-macOS.dmg"

# 3. Git Add, Commit e Push
echo ""
echo "=== Git Commit & Push v5.0.0 ==="
git add -A
git commit -m 'Release v5.0.0: Tektronix 4014 DVBST Vector Engine, Connection Diagnostics, WADX Layout and Script Automation'
git tag -f -a v5.0.0 -m 'PlatoLives 5.0.0 - Tektronix 4014 DVBST, Connection Diagnostics, WADX Keymap and Network Automation'
git push origin HEAD --tags
echo "[OK] Commit e Tag v5.0.0 inviati su GitHub con successo!"
