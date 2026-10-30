#!/usr/bin/env bash
set -e

echo "================================================================="
echo ">>> PLATOLIVES - MULTI-PLATFORM BUILD PIPELINE"
echo "================================================================="

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

# Sincronizzazione automatica SSOT: genera include/plato/plato_version.h da VERSION
VERSION_STR=$(cat "$ROOT_DIR/VERSION" | tr -d ' \n\r')
cat << EOF > "$ROOT_DIR/include/plato/plato_version.h"
/* Single Source of Truth - PlatoLives Version Header */
/* Automatically generated from VERSION file - DO NOT EDIT MANUALLY */
#ifndef PLATO_VERSION_H
#define PLATO_VERSION_H

#define PLATO_VERSION_STRING "${VERSION_STR}"
#define PLATO_VERSION_WSTRING L"${VERSION_STR}"

#endif /* PLATO_VERSION_H */
EOF
mkdir -p build/stubs_arm64 build/stubs_x86_64

SDL2_INC="/opt/homebrew/include"
if [ ! -d "$SDL2_INC/SDL2" ]; then
    SDL2_INC="/opt/homebrew/opt/sdl2-compat/include"
fi

# Pulizia preventiva per forzare Full Rebuild ed evidenziare sempre tutti i warning/errori
if [ -f "build/Makefile" ]; then
    make -C build clean > /dev/null 2>&1 || true
fi
rm -rf build/PlatoLives.app build/PlatoLives-*.exe build/PlatoLives-linux-* build/PlatoLives-console-* build/linux_desktop_install

# 1. BUILD NATIVA macOS (GUI + Headless + Console)
echo ""
echo "[1/6] Compilazione nativa macOS (Clang / CMake - Full Rebuild)..."
cmake -B build > /dev/null
make -C build -j$(sysctl -n hw.ncpu)
echo ">>> Esecuzione test unitari (ctest)..."
ctest --test-dir build --output-on-failure

# Generazione stubs con SONAME ufficiale Linux
zig cc -target aarch64-linux-gnu -shared -Wl,-soname,libSDL2-2.0.so.0 platolives-linux/linux_stubs.c -o build/stubs_arm64/libSDL2.so
zig cc -target aarch64-linux-gnu -shared -Wl,-soname,libGL.so.1 platolives-linux/linux_stubs.c -o build/stubs_arm64/libGL.so

zig cc -target x86_64-linux-gnu -shared -Wl,-soname,libSDL2-2.0.so.0 platolives-linux/linux_stubs.c -o build/stubs_x86_64/libSDL2.so
zig cc -target x86_64-linux-gnu -shared -Wl,-soname,libGL.so.1 platolives-linux/linux_stubs.c -o build/stubs_x86_64/libGL.so

# 2. CROSS-COMPILAZIONE LINUX GUI ARM64 (ELF)
echo ""
echo "[2/6] Cross-compilazione Linux Desktop GUI ARM64 ELF (Zig cc)..."
zig cc -target aarch64-linux-gnu -std=c11 -O2 -s \
    -Iinclude -Iinclude/plato -I"$SDL2_INC" -I"$SDL2_INC/SDL2" \
    -Lbuild/stubs_arm64 \
    platolives-linux/linux_main.c \
    platolives-linux/linux_menu.c \
    platolives-linux/linux_gl.c \
    platolives-linux/linux_keyref.c \
    platolives-linux/linux_profiles.c \
    platolives-linux/linux_scriptref.c \
    platolives-linux/linux_scripts.c \
    platolives-linux/linux_keymap.c \
    platolives-linux/linux_livetext.c \
    platolives-linux/linux_about.c \
    platolives-linux/linux_conn.c \
    platolives-linux/linux_hud.c \
    platolives-linux/linux_audio.c \
    platolives-linux/stb_impl.c \
    src/plato_protocol.c \
    src/plato_terminal.c \
    src/plato_transport.c \
    src/plato_graphics.c \
    src/plato_font.c \
    src/plato_framebuffer.c \
    src/plato_ringbuf.c \
    src/plato_keyboard.c \
    src/plato_keymap.c \
    src/plato_profile.c \
    src/plato_script.c \
    src/plato_optical.c \
    src/plato_pdp1_font.c \
    src/plato_pdp1.c \
    src/plato_tek_font.c \
    src/z80emu.c \
    src/plato_microtutor.c \
    -lSDL2 -lm -lpthread \
    -o build/PlatoLives-linux-gui-arm64

# 3. CROSS-COMPILAZIONE LINUX GUI INTEL x86_64 (ELF)
echo ""
echo "[3/6] Cross-compilazione Linux Desktop GUI x86_64 ELF (Zig cc)..."
zig cc -target x86_64-linux-gnu -std=c11 -O2 -s \
    -Iinclude -Iinclude/plato -I"$SDL2_INC" -I"$SDL2_INC/SDL2" \
    -Lbuild/stubs_x86_64 \
    platolives-linux/linux_main.c \
    platolives-linux/linux_menu.c \
    platolives-linux/linux_gl.c \
    platolives-linux/linux_keyref.c \
    platolives-linux/linux_profiles.c \
    platolives-linux/linux_scriptref.c \
    platolives-linux/linux_scripts.c \
    platolives-linux/linux_keymap.c \
    platolives-linux/linux_livetext.c \
    platolives-linux/linux_about.c \
    platolives-linux/linux_conn.c \
    platolives-linux/linux_hud.c \
    platolives-linux/linux_audio.c \
    platolives-linux/stb_impl.c \
    src/plato_protocol.c \
    src/plato_terminal.c \
    src/plato_transport.c \
    src/plato_graphics.c \
    src/plato_font.c \
    src/plato_framebuffer.c \
    src/plato_ringbuf.c \
    src/plato_keyboard.c \
    src/plato_keymap.c \
    src/plato_profile.c \
    src/plato_script.c \
    src/plato_optical.c \
    src/plato_pdp1_font.c \
    src/plato_pdp1.c \
    src/plato_tek_font.c \
    src/z80emu.c \
    src/plato_microtutor.c \
    -lSDL2 -lm -lpthread \
    -o build/PlatoLives-linux-gui-x86_64

# 4. CROSS-COMPILAZIONE LINUX CONSOLE STATICA
echo ""
echo "[4/6] Cross-compilazione Linux Console statica ARM64 & x86_64..."
zig cc -target aarch64-linux-musl -std=c11 -O2 -s -Iinclude -Iinclude/plato \
    src/console_main.c src/console_runner.c \
    src/plato_protocol.c src/plato_terminal.c src/plato_transport.c \
    src/plato_graphics.c src/plato_font.c src/plato_framebuffer.c \
    src/plato_ringbuf.c src/plato_keyboard.c src/plato_keymap.c src/plato_profile.c \
    src/plato_script.c src/plato_optical.c src/plato_tek_font.c src/plato_pdp1_font.c src/plato_pdp1.c src/z80emu.c src/plato_microtutor.c \
    -pthread -static -o build/PlatoLives-console-linux-arm64

zig cc -target x86_64-linux-musl -std=c11 -O2 -s -Iinclude -Iinclude/plato \
    src/console_main.c src/console_runner.c \
    src/plato_protocol.c src/plato_terminal.c src/plato_transport.c \
    src/plato_graphics.c src/plato_font.c src/plato_framebuffer.c \
    src/plato_ringbuf.c src/plato_keyboard.c src/plato_keymap.c src/plato_profile.c \
    src/plato_script.c src/plato_optical.c src/plato_tek_font.c src/plato_pdp1_font.c src/plato_pdp1.c src/z80emu.c src/plato_microtutor.c \
    -pthread -static -o build/PlatoLives-console-linux-x86_64

# 5. CROSS-COMPILAZIONE WINDOWS GUI x86_64 (Direct3D 11 + Win32)
echo ""
echo "[5/6] Cross-compilazione Windows GUI x86_64 (Zig cc Win32/D3D11)..."
zig cc -target x86_64-windows-gnu -std=c11 -O2 -s -Iinclude -Iinclude/plato \
    platolives-win/win_main.c \
    platolives-win/win_menu.c \
    platolives-win/win_profiles.c platolives-win/win_scripts.c \
    platolives-win/win_keymap.c \
    platolives-win/win_text_buffer.c \
    platolives-win/win_keyref.c \
    platolives-win/win_scriptref.c \
    platolives-win/win_about.c \
    platolives-win/win_conn.c \
    platolives-win/PlatoLives.rc \
    src/plato_protocol.c \
    src/plato_terminal.c \
    src/plato_transport.c \
    src/plato_graphics.c \
    src/plato_font.c \
    src/plato_framebuffer.c \
    src/plato_ringbuf.c \
    src/plato_keyboard.c \
    src/plato_keymap.c \
    src/plato_profile.c \
    src/plato_script.c \
    src/plato_optical.c \
    src/plato_pdp1_font.c \
    src/plato_pdp1.c \
    src/plato_tek_font.c \
    src/z80emu.c \
    src/plato_microtutor.c \
    src/console_runner.c \
    -luser32 -lgdi32 -ld3d11 -ldxgi -lws2_32 -lcomdlg32 \
    -Wl,--subsystem,console \
    -o build/PlatoLives-windows-x86_64.exe

# 6. CROSS-COMPILAZIONE WINDOWS GUI ARM64 (Direct3D 11 + Win32)
echo ""
echo "[6/6] Cross-compilazione Windows GUI ARM64 (Zig cc Win32/D3D11)..."
zig cc -target aarch64-windows-gnu -std=c11 -O2 -s -Iinclude -Iinclude/plato \
    platolives-win/win_main.c \
    platolives-win/win_menu.c \
    platolives-win/win_profiles.c platolives-win/win_scripts.c \
    platolives-win/win_keymap.c \
    platolives-win/win_text_buffer.c \
    platolives-win/win_keyref.c \
    platolives-win/win_scriptref.c \
    platolives-win/win_about.c \
    platolives-win/win_conn.c \
    platolives-win/PlatoLives.rc \
    src/plato_protocol.c \
    src/plato_terminal.c \
    src/plato_transport.c \
    src/plato_graphics.c \
    src/plato_font.c \
    src/plato_framebuffer.c \
    src/plato_ringbuf.c \
    src/plato_keyboard.c \
    src/plato_keymap.c \
    src/plato_profile.c \
    src/plato_script.c \
    src/plato_optical.c \
    src/plato_pdp1_font.c \
    src/plato_pdp1.c \
    src/plato_tek_font.c \
    src/z80emu.c \
    src/plato_microtutor.c \
    src/console_runner.c \
    -luser32 -lgdi32 -ld3d11 -ldxgi -lws2_32 -lcomdlg32 \
    -Wl,--subsystem,console \
    -o build/PlatoLives-windows-arm64.exe

echo ""
echo "================================================================="

# 6. PACCHETTO DESKTOP LINUX AUTOCONSISTENTE (ARM64 + x86_64)
echo ""
echo "[*] Generazione pacchetto di installazione desktop in build/linux_desktop_install..."
mkdir -p build/linux_desktop_install

cp platolives-linux/platolives_icon.png build/linux_desktop_install/platolives.png
cp build/PlatoLives-linux-gui-arm64 build/linux_desktop_install/PlatoLives-linux-gui-arm64
cp build/PlatoLives-linux-gui-x86_64 build/linux_desktop_install/PlatoLives-linux-gui-x86_64

cat << 'EOF' > build/linux_desktop_install/install.sh
#!/bin/bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARCH="$(uname -m)"

echo "[*] Rilevata architettura di sistema: $ARCH"

if [ "$ARCH" = "aarch64" ] || [ "$ARCH" = "arm64" ]; then
    BIN_SRC="$DIR/PlatoLives-linux-gui-arm64"
elif [ "$ARCH" = "x86_64" ]; then
    BIN_SRC="$DIR/PlatoLives-linux-gui-x86_64"
else
    echo "[!] Architettura non supportata: $ARCH"
    exit 1
fi

if [ ! -f "$BIN_SRC" ]; then
    echo "[!] File binario $BIN_SRC non trovato!"
    exit 1
fi

# 1. Installazione binario utente (~/.local/bin)
mkdir -p ~/.local/bin
cp "$BIN_SRC" ~/.local/bin/platolives
chmod +x ~/.local/bin/platolives

# 2. Installazione icona ufficiale Mac
mkdir -p ~/.local/share/icons
cp "$DIR/platolives.png" ~/.local/share/icons/platolives.png

# 3. Creazione launcher .desktop per menu Applicazioni e Desktop
mkdir -p ~/.local/share/applications
cat << DESKTOP_EOF > ~/.local/share/applications/PlatoLives.desktop
[Desktop Entry]
Version=1.0
Type=Application
Name=PlatoLives
GenericName=PLATO Terminal
Comment=CDC PLATO IV / Cyber1 Network Terminal Emulator
Exec=/home/$USER/.local/bin/platolives %U
Icon=/home/$USER/.local/share/icons/platolives.png
Terminal=false
Categories=Network;Emulator;Utility;
Keywords=PLATO;Cyber1;CDC;Terminal;Retro;
StartupWMClass=PlatoLives
DESKTOP_EOF

sed -i "s/\$USER/$USER/g" ~/.local/share/applications/PlatoLives.desktop
chmod +x ~/.local/share/applications/PlatoLives.desktop

# 4. Aggiornamento cache desktop e icone
update-desktop-database ~/.local/share/applications/ 2>/dev/null || true
gtk-update-icon-cache ~/.local/share/icons/ 2>/dev/null || true

echo ""
echo "[+] Installazione completata con successo!"
echo "    - Eseguibile installato in: ~/.local/bin/platolives"
echo "    - Icona ufficiale in:      ~/.local/share/icons/platolives.png"
echo "    - Voce Menu creata in:      ~/.local/share/applications/PlatoLives.desktop"
echo "[+] PlatoLives e ora disponibile nel menu Applicazioni con la sua icona ufficiale!"
EOF

chmod +x build/linux_desktop_install/install.sh

# Generazione archivio tarball del pacchetto Linux Desktop Installer
tar -czf build/PlatoLives-linux-desktop-install.tar.gz -C build linux_desktop_install
echo "[*] Creato archivio build/PlatoLives-linux-desktop-install.tar.gz"


echo ">>> BUILD COMPLETATA CON SUCCESSO PER TUTTI I TARGET!"
echo "================================================================="
file build/PlatoLives-linux-gui-arm64
file build/PlatoLives-windows-x86_64.exe
file build/PlatoLives-windows-arm64.exe
