#!/usr/bin/env bash
set -e

echo "================================================================="
echo ">>> PLATOLIVES - MULTI-PLATFORM BUILD PIPELINE"
echo "================================================================="

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"
mkdir -p build

# 1. BUILD NATIVA macOS (GUI + Headless + Console)
echo ""
echo "[1/4] Compilazione nativa macOS (Clang / CMake)..."
cmake -B build > /dev/null
make -C build -j$(sysctl -n hw.ncpu)
echo ">>> Esecuzione test unitari (ctest)..."
ctest --test-dir build --output-on-failure

# 2. CROSS-COMPILAZIONE LINUX ARM64 (aarch64-linux-musl)
echo ""
echo "[2/4] Cross-compilazione Linux ARM64 statica (Zig cc)..."
zig cc -target aarch64-linux-musl -std=c11 -O2 -s -Iinclude \
    src/console_main.c \
    src/console_runner.c \
    src/plato_protocol.c \
    src/plato_terminal.c \
    src/plato_transport.c \
    src/plato_graphics.c \
    src/plato_font.c \
    src/plato_framebuffer.c \
    src/plato_ringbuf.c \
    src/plato_keyboard.c \
    src/plato_profile.c \
    src/plato_script.c \
    src/plato_optical.c \
    -pthread -static \
    -o build/PlatoLives-linux-arm64

# 3. CROSS-COMPILAZIONE LINUX INTEL (x86_64-linux-musl)
echo ""
echo "[3/4] Cross-compilazione Linux Intel x86_64 statica (Zig cc)..."
zig cc -target x86_64-linux-musl -std=c11 -O2 -s -Iinclude \
    src/console_main.c \
    src/console_runner.c \
    src/plato_protocol.c \
    src/plato_terminal.c \
    src/plato_transport.c \
    src/plato_graphics.c \
    src/plato_font.c \
    src/plato_framebuffer.c \
    src/plato_ringbuf.c \
    src/plato_keyboard.c \
    src/plato_profile.c \
    src/plato_script.c \
    src/plato_optical.c \
    -pthread -static \
    -o build/PlatoLives-linux-x86_64

# 4. CROSS-COMPILAZIONE WINDOWS GUI (Direct3D 11 + Win32)
echo ""
echo "[4/4] Cross-compilazione Windows GUI x86_64 (Zig cc Win32/D3D11)..."
zig cc -target x86_64-windows-gnu -std=c11 -O2 -s -Iinclude \
    platolives-win/win_main.c \
    platolives-win/win_menu.c \
    platolives-win/win_profiles.c platolives-win/win_scripts.c \
    platolives-win/win_text_buffer.c \
    platolives-win/win_keyref.c \
    platolives-win/win_about.c \
    platolives-win/PlatoLives.rc \
    src/plato_protocol.c \
    src/plato_terminal.c \
    src/plato_transport.c \
    src/plato_graphics.c \
    src/plato_font.c \
    src/plato_framebuffer.c \
    src/plato_ringbuf.c \
    src/plato_keyboard.c \
    src/plato_profile.c \
    src/plato_script.c \
    src/plato_optical.c \
    src/console_runner.c \
    -luser32 -lgdi32 -ld3d11 -ldxgi -lws2_32 \
    -Wl,--subsystem,console \
    -o build/PlatoLives-windows-x86_64.exe

echo ""
echo "================================================================="
echo ">>> BUILD COMPLETATA CON SUCCESSO PER TUTTI I TARGET!"
echo "================================================================="
file build/PlatoLives.app/Contents/MacOS/PlatoLives
file build/PlatoLives-linux-arm64
file build/PlatoLives-linux-x86_64
file build/PlatoLives-windows-x86_64.exe
echo ""
ls -lh build/PlatoLives.app/Contents/MacOS/PlatoLives \
       build/PlatoLives-linux-arm64 \
       build/PlatoLives-linux-x86_64 \
       build/PlatoLives-windows-x86_64.exe
echo "================================================================="
