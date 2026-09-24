# PlatoLives (v4.0)

[![Platform](https://img.shields.io/badge/platform-macOS%2013%2B%20%7C%20Windows%2010%2F11%20%7C%20Linux%20ARM64%20%26%20x86__64-orange.svg)](#)
[![Language](https://img.shields.io/badge/language-C11%20%7C%20Direct3D%2011%20%7C%20Cocoa%2FMetal-blue.svg)](#)
[![License](https://img.shields.io/badge/license-CC%20BY--NC--SA%204.0-red.svg)](LICENSE)
[![Standard](https://img.shields.io/badge/standard-CDC%20IST--III%20%2F%20CERL%20X--20%20%2F%20CDC%20721-brightgreen.svg)](#)

<img width="3658" height="1964" alt="PlatoLives Banner" src="https://github.com/user-attachments/assets/8b8e7e8c-1219-4be6-82f9-b20b1591509c" />

**PlatoLives** is a high-performance, native client for the legendary **PLATO** computer-based education and social system. Designed from the ground up for modern hardware, it features native UI presentation across **macOS (Apple Silicon & Intel)**, **Windows 10/11 (Native Direct3D 11 & Win32)**, and standalone, ultra-lightweight static CLI appliances for **Linux (ARM64 and x86_64)**.

Built as a pure C11 standard core (`libplato`) paired with platform-native graphics backends (Metal/CoreAnimation on macOS, Direct3D 11 on Windows), PlatoLives connects over standard TCP to active CYBIS and Cyber1 mainframes (`cyberserv.org:8005`), IRATA.ONLINE, and private PLATO nodes. It delivers an authentic sub-pixel gas-discharge neon plasma and color CRT experience at 60 FPS with near-zero idle CPU usage.

📖 **Documentation**: Read the official [User and Technical Manual (PDF)](docs/PlatoLives_3.5_User_and_Technical_Manual.pdf).

---

## Key Features in v4.0

### 1. Windows Native Direct3D 11 & Dark Plasma Experience (v4.0)
- **Standalone PE Binary**: Zero dependencies, single executable (`PlatoLives-windows-x86_64.exe`) with dynamic Console/Direct3D 11 mode switching.
- **Pure Dark Plasma UI Theme**:
  - DWM Immersive Dark Mode window framing with authentic Orange Plasma (`RGB(255, 110, 0)`) captions and deep charcoal backgrounds (`RGB(16, 12, 10)`).
  - Custom UAH-rendered menubars and Owner-Draw dropdown menus in full Orange Plasma aesthetic.
  - Native Windows dialogs: *Manage Connection Profiles*, *PLATO Keyboard Reference*, *Live Text Buffer*, and *About PlatoLives*.
- **Native Win32 Audio & Telemetry**: Non-blocking asynchronous bell beeper and quiescence-aware FPS/CPU HUD.

### 2. Dual Physical Display Engines (60 FPS)
- **"Real Plasma" Engine**: Simulates the 1972 Owens-Illinois neon-argon gas-discharge flat panel. Features cell ionization profiles, wide-spectrum neon glow with two-stage box blur (local $r=4$ + wide $r=12$), and smooth floating-point exponential decay.
- **"Real Color CRT" Engine (CDC 721 / IST-III)**: High-fidelity analog color CRT emulation supporting full Cyber1 color mode (Subtype 16 handshake).
  - **3-Tap Analog Electron Beam Reconstruction**: Continuous 2D beam profile modeling ($\sigma = 1.8 \dots 2.8$).
  - **Phosphor P22 Color Persistence**: Pure floating-point RGB phosphor decay with selectable durations (20 ms fast P22 up to 5000 ms storage-tube mode) extinguishing seamlessly into pitch black.
  - **Pure Chromatic Shadow Mask**: Modulated RGB sub-pixel triad matrix preserving 100% light efficiency without grid darkening or color crosstalk.
  - **Soft Scanlines**: 512 analog raster lines with natural cathode groove modulation ($18\% - 22\%$).
  - **CRT Beam Profiles**: Three selectable profiles (*Standard/Authentic 13"*, *High/Soft Glow [default]*, *Ultra/Vintage Arcade*).

### 3. Optical Geometric Distortion (Glass Curvature)
- **Calibrated 13" CRT / Tube Geometry**: Physically proportioned curvature ($k = 0.018$) tailored for the central 1:1 PLATO active area inside a 4:3 cathode-ray tube, eliminating distorted fisheye artifacts while providing an authentic glass bezel frame.
- **Selectable Curvature Types**:
  - **Cylindrical [Default]**: Curvature applied exclusively along the horizontal X axis with a flat vertical Y axis, keeping scanlines straight while curving vertical borders.
  - **Barrel (Spherical)**: Organic spherical glass curvature along both X and Y axes.
  - **None (Flat)**: 1:1 rectilinear rendering.
- Available for both **Real Color CRT** and full-screen **Real Plasma** display modes.
- **Integrated Touchscreen Coordinate Mapping**: Touch input automatically compensates for glass curvature, maintaining pixel-perfect accuracy on Cyber1 lessons and keypad games.

### 4. Interactive ANSI Console Mode (`--console`, `--c`)
- **Unified Single Binary**: Launch directly in text mode from Terminal or over SSH without WindowServer / GUI:
  `./PlatoLives --console` or `./PlatoLives --c cyberserv.org 8005`.
- **TrueColor Plasma Amber Palette**: 24-bit TrueColor (`\033[38;2;255;140;0m`) with differential double-buffering.
- **Auto-Centering & Retro Bezel**: Dynamically centers the $64 \times 32$ canvas with a dim amber frame on window resize (`SIGWINCH`).
- **Full-Width Status Bar**: Live session metadata (`User/Group/Slot`) and key shortcut reminders.
- **One-Touch Screen Copy (`Ctrl+Y` / `ESC C`)**: Saves instantly to `screen.txt`, system pasteboard, and remote client via ANSI OSC 52.

### 5. Smart Double-Tap Input Engine
- **Solves the Keyboard / Web Terminal Dilemma**: Double-tap within 500 ms activates the Shift modifier:
  - **Double Return** (or `ESC` + `Return`) $\rightarrow$ **`SHIFT-NEXT`** (save & file in PLATO Notes!).
  - **Double F4** or **Double Ctrl+S** $\rightarrow$ **`SHIFT-STOP`** (sign off / logout).
  - **Double F8**, **Double ESC**, or **Double Ctrl+B** $\rightarrow$ **`SHIFT-BACK`** (exit lesson to main index).
  - **Double F1 / Ctrl+H** $\rightarrow$ **`SHIFT-HELP`**, Double F2 $\rightarrow$ `SHIFT-LAB`, Double F3 $\rightarrow$ `SHIFT-DATA`, Double F5 $\rightarrow$ `SHIFT-EDIT`.
- **CRLF De-bounce & Immediate Text**: Standard typing remains 0 ms instantaneous.

### 6. Universal UTF-8 Semigraphics & Box-Drawing Engine
- **Canonical M1 Set**: Translates lines (`│`), double rules (`═`), arrows (`↑ → ↓ ←`), math operators (`≠ ≤ ≥ × ÷ ± ≈`), Greek letters (`α β γ δ ε π μ Σ Δ Θ`), and symbols (`◆ ○`).
- **Topological RAM Font Classifier (M2/M3)**: Dynamically maps downloaded 16-byte glyphs into Unicode box-drawing characters (`┌ ┐ └ ┘ ┼ ├ ┤ ┬ ┴ ─ │ █ ░`).
- **Unified Core**: Shared across Console Mode, Live Text Buffer window, macOS/Windows Copy/Paste, and headless automation!

### 7. Connection Profile Manager & Startup Scripts
- **Native Dialogs on macOS & Windows**: Full profile management (Create, Edit, Delete, Default preset selection).
- **Automated Startup Scripts**: Monospace script editor supporting automated authentication sequences (`wait 2s`, `key NEXT`, `send user`, `send password`).
- **Interactive Handover**: Connects, authenticates automatically, and seamlessly hands the session to the user.

### 8. Live Text Buffer Companion & Copy/Paste Engine
- **"PLATO Live Text Buffer" Window (`Cmd+Shift+T` / `Ctrl+Shift+T`)**: A dedicated companion window mirroring the active $64 \times 32$ terminal text matrix at 10 fps.
  - **Freeze-on-Selection**: Automatically pauses live streaming when the user selects text or clicks *Select All*, ensuring stable, uninterrupted selection even during fast chat or data streams.
  - **Retro Dark Theme**: High-contrast Orange Plasma typography on deep background with integrated toolbar (`[Select All]`, `[Deselect / Live]`, `[✓] Compact`, `[Copy to Clipboard]`, `[● LIVE]` status badge).
  - **Menu Integration**: Complete Edit menu alignment (*Copy All Text Verbatim*, *Copy All Text Compact*, *Copy Selected Text...*, *Copy Screen Image*, *Paste Text*).
- **Core C11 Text Matrix (`libplato`)**: Tracks all characters in memory (`text_grid[32][64]`), automatically excluding M2/M3 downloadable graphics fonts.

### 9. Headless Remote Automation & CLI Scripting (`--test-script`)
- **SSH & Headless Execution**: Runs autonomously in background and remote SSH sessions without display server requirements.
- **Expanded Command Grammar**:
  - `copy-all [compact] [file:<path> | clipboard | console]`: Dumps full-screen text to disk, clipboard, or prints directly to **standard output (`console`) in real time**.
  - `copy-area <x1> <y1> <x2> <y2> [compact] [dest]`: Extracts specific sub-regions using cell ($0..63 \times 0..31$) or PLATO pixel ($0..511$) coordinates.
  - `display <on | off>`: Orthogonal toggle to disable window compositing while keeping physical decay simulations running.
  - `renderer <plasma | crt | color | crisp | split>`: Scriptable selection of simulated optical engines.
- **In-Memory 4x Optical Screenshots**: Full $2048 \times 2048$ PNG screenshot generation with authentic phosphor decay physics.

### 10. Standalone Linux Appliance & Cross-Compilation
- **Pure C11 Decoupled Console Client**: Zero external dependencies (no X11, Wayland, GTK, SDL, ncurses).
- **Microscopic 100 KB Static Binaries**: Built via Zig (`./build_all.sh`) into 100% statically linked ELF binaries for Linux ARM64 (QNAP NAS, Raspberry Pi) and Linux Intel x86_64.

---

## Technical Specifications

| Parameter | Specification |
| :--- | :--- |
| **Supported Platforms** | macOS 13+ (Universal), Windows 10/11 (x86_64), Linux ARM64 & x86_64 |
| **Protocol** | CDC IST-III / Jack Stifle CERL X-20 / CDC 721 (ASCII & Color modes) |
| **Logical Matrix** | 512 × 512 1-bit bitboard (32 KB) & 32-bit Little-Endian BGRA matrix |
| **Text Grid** | 64 × 32 matrix with M1 UTF-8 table and M2/M3 topological semigraphics classifier |
| **Console Mode** | Native ANSI TrueColor Plasma Amber terminal with double-buffering & auto-centering |
| **Input Engine** | Smart 500ms Double-Tap state machine (Return x2 = SHIFT-NEXT, F4 x2 = SHIFT-STOP) |
| **Linux Targets** | 100% Statically linked, standalone 100 KB ELF binaries (ARM64 and x86_64) |
| **Windows Target** | Standalone PE executable, Direct3D 11 hardware acceleration, Win32 Dark Plasma UI |
| **Render Surface** | 2048 × 2048 32-bit sub-pixel BGRA supersampled surface (4x scale) |
| **Core Architecture** | Pure C11 (`libplato`), thread-safe ring buffer, non-blocking sockets |
| **UI Frameworks** | macOS: Objective-C (Cocoa / Core Animation) / Windows: Direct3D 11 & Win32 |
| **Compatibility** | Cyber1 / CYBIS (`cyberserv.org:8005`), IRATA.ONLINE (`irata.online:8005`) |

---

## Building and Running

### Multi-Platform Unified Build (macOS + Linux + Windows)
With Zig compiler installed (`brew install zig`), compile all 4 target platforms and run the full test suite in seconds:
```bash
./build_all.sh
Outputs in build/:
PlatoLives.app (macOS Universal Apple Silicon + Intel)
PlatoLives-windows-x86_64.exe (Windows 10/11 Direct3D 11 Native)
PlatoLives-linux-arm64 (Linux ARM64 Static Musl ELF)
PlatoLives-linux-x86_64 (Linux x86_64 Static Musl ELF)
Prerequisites & Native macOS Build
macOS 13.0 (Ventura) or newer
Apple Clang (via Xcode Command Line Tools: xcode-select --install)
CMake >= 3.16
code
Bash
# Clone the repository
git clone git@github.com:TheSynthMaster/PlatoLives.git
cd PlatoLives

# Build and test
mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
ctest --output-on-failure

# Launch PlatoLives GUI
./PlatoLives.app/Contents/MacOS/PlatoLives
