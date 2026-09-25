# PlatoLives (v4.2)

[![Platform](https://img.shields.io/badge/platform-macOS%2013%2B%20%7C%20Windows%2010%2F11%20%7C%20Linux%20ARM64%20%26%20x86__64-orange.svg)](#)
[![Language](https://img.shields.io/badge/language-C11%20%7C%20Direct3D%2011%20%7C%20Cocoa%2FMetal-blue.svg)](#)
[![License](https://img.shields.io/badge/license-CC%20BY--NC--SA%204.0-red.svg)](LICENSE)
[![Standard](https://img.shields.io/badge/standard-CDC%20IST--III%20%2F%20CERL%20X--20%20%2F%20CDC%20721-brightgreen.svg)](#)

<img width="3658" height="1964" alt="PlatoLives Banner" src="https://github.com/user-attachments/assets/8b8e7e8c-1219-4be6-82f9-b20b1591509c" />

**PlatoLives** is a high-performance, native client for the legendary **PLATO** computer-based education and social system. Designed from the ground up for modern hardware, it features native UI presentation across **macOS (Apple Silicon & Intel)**, **Windows 10/11 (Native Direct3D 11 & Win32)**, and standalone, ultra-lightweight static CLI appliances for **Linux (ARM64 and x86_64)**.

Built as a pure C11 standard core (`libplato`) paired with platform-native graphics backends (Metal/CoreAnimation on macOS, Direct3D 11 on Windows), PlatoLives connects over standard TCP to active CYBIS and Cyber1 mainframes (`cyberserv.org:8005`), IRATA.ONLINE, and private PLATO nodes. It delivers an authentic sub-pixel gas-discharge neon plasma and color CRT experience at 60 FPS with near-zero idle CPU usage.

📖 **Documentation**: Read the official [User and Technical Manual (PDF)](docs/PlatoLives_3.5_User_and_Technical_Manual.pdf).

---

## Key Features in v4.2

### 1. Unified Turing-Complete Scripting Engine & Automation
PlatoLives v4.2 introduces a pure C11, thread-safe micro-scripting language for autonomous terminal interaction, automated macro execution, and self-healing login procedures on Cyber1/CYBIS.
- **Embedded C11 Micro-Engine**: Zero external dependencies, deterministic parsing, non-blocking asynchronous execution in background threads.
- **Native Micro-Regex Engine**: Built-in regular expression support (`regex "Pattern +"`) for flexible whitespace and prompt matching without heuristics.
- **Turing-Complete Grammar**: Variables (`let`), arithmetic/relational expressions, instantaneous state checks (`find_text`), polling synchronization (`wait_text`), structured loops (`for ... next`), labels, jumps (`goto`), and conditional branches (`if ... goto`).
- **Interactive Keyboard Inhibition**: Keystrokes are safely intercepted and discarded during script runtime to prevent accidental user interference with mainframe prompts.
- **Global Emergency Abort**: Pressing `Ctrl+Shift+X` immediately terminates any running script across both macOS and Windows.
- **Visual Script Status Indicator**: An authentic Orange Plasma (`RGB(255, 110, 0)`) indicator dot blinks in the top-right corner of the active terminal area at 2.5 Hz (400ms cycle: 200ms ON / 200ms OFF) during execution. It features faithful, byte-accurate background pixel saving and restoration, leaving zero artifacts when completed.

### 2. Native "Manage Scripts" Dialog & Menu Integration (macOS & Windows)
A dedicated, fully native dark-themed management interface matching the canonical PlatoLives Orange Plasma aesthetic (`RGB(16, 12, 10)` dark background, `RGB(24, 18, 14)` panels, and `RGB(255, 110, 0)` accents):
- **Script Library Management**: Create (`+ New`), `Clone`, and `Delete` scripts with persistent storage (`~/.platolives/scripts.ini` on macOS/Linux, `%APPDATA%\PlatoLives\scripts.ini` on Windows).
- **Interactive Hotkey Recorder**: Assign custom keyboard shortcuts (with modifier support: `Ctrl`, `Alt`, `Shift`, and function keys `F1-F12`). Includes built-in reservation checks (preventing conflicts with system shortcuts).
- **Per-Script Timing Tuning**: Fine-tune character delay (`char_delay_ms`), NEXT key delay (`next_delay_ms`), and command delay (`command_delay_ms`).
- **Monospace Code Editor**: Built-in monospace editor with undo/redo, cut/copy/paste, and automatic line sanitization.
- **Dynamic Menu Bar Dispatching**: Dedicated **Scripts** dropdown menu in the top system menu bar displaying active scripts and hotkey badges with single-click trigger support.

### 3. Expanded "Manage Profiles" Window (+50% Larger UI)
- **High-Comfort Layout**: Rescaled profile manager (1080 × 960 px) providing a massive 505px vertical editing canvas for startup connection scripts.
- **Synchronized Dark Theme**: Shared Orange Plasma styling with dark Aqua controls across macOS and Windows.
- **Streamlined Keyboard Reference**: Redesigned 880px high reference panel with seamless mouse-wheel and arrow-key scrolling without distracting scrollbar tracks.

### 4. Windows Native Direct3D 11 & Dark Plasma Experience
- **Standalone PE Binary**: Zero dependencies, single executable (`PlatoLives-windows-x86_64.exe`) with dynamic Console/Direct3D 11 mode switching.
- **Pure Dark Plasma UI Theme**:
  - DWM Immersive Dark Mode window framing with authentic Orange Plasma (`RGB(255, 110, 0)`) captions and deep charcoal backgrounds (`RGB(16, 12, 10)`).
  - Custom UAH-rendered menubars and Owner-Draw dropdown menus in full Orange Plasma aesthetic.
  - Native Windows dialogs: *Manage Connection Profiles*, *Manage Scripts*, *PLATO Keyboard Reference*, *Live Text Buffer*, and *About PlatoLives*.
- **Native Win32 Audio & Telemetry**: Non-blocking asynchronous bell beeper and quiescence-aware FPS/CPU HUD.

### 5. Dual Physical Display Engines (60 FPS)
- **"Real Plasma" Engine**: Simulates the 1972 Owens-Illinois neon-argon gas-discharge flat panel. Features cell ionization profiles, wide-spectrum neon glow with two-stage box blur (local $r=4$ + wide $r=12$), and smooth floating-point exponential decay.
- **"Real Color CRT" Engine (CDC 721 / IST-III)**: High-fidelity analog color CRT emulation supporting full Cyber1 color mode (Subtype 16 handshake).
  - **3-Tap Analog Electron Beam Reconstruction**: Continuous 2D beam profile modeling ($\sigma = 1.8 \dots 2.8$).
  - **Phosphor P22 Color Persistence**: Pure floating-point RGB phosphor decay with selectable durations (20 ms fast P22 up to 5000 ms storage-tube mode) extinguishing seamlessly into pitch black.
  - **Pure Chromatic Shadow Mask**: Modulated RGB sub-pixel triad matrix preserving 100% light efficiency without grid darkening or color crosstalk.
  - **Soft Scanlines**: 512 analog raster lines with natural cathode groove modulation ($18\% - 22\%$).
  - **CRT Beam Profiles**: Three selectable profiles (*Standard/Authentic 13"*, *High/Soft Glow [default]*, *Ultra/Vintage Arcade*).

### 6. Optical Geometric Distortion (Glass Curvature)
- **Calibrated 13" CRT / Tube Geometry**: Physically proportioned curvature ($k = 0.018$) tailored for the central 1:1 PLATO active area inside a 4:3 cathode-ray tube, eliminating distorted fisheye artifacts while providing an authentic glass bezel frame.
- **Selectable Curvature Types**:
  - **Cylindrical [Default]**: Curvature applied exclusively along the horizontal X axis with a flat vertical Y axis, keeping scanlines straight while curving vertical borders.
  - **Barrel (Spherical)**: Organic spherical glass curvature along both X and Y axes.
  - **None (Flat)**: 1:1 rectilinear rendering.
- Available for both **Real Color CRT** and full-screen **Real Plasma** display modes.
- **Integrated Touchscreen Coordinate Mapping**: Touch input automatically compensates for glass curvature, maintaining pixel-perfect accuracy on Cyber1 lessons and keypad games.

### 7. Interactive ANSI Console Mode (`--console`, `--c`)
- **Unified Single Binary**: Launch directly in text mode from Terminal or over SSH without WindowServer / GUI:
  `./PlatoLives --console` or `./PlatoLives --c cyberserv.org 8005`.
- **TrueColor Plasma Amber Palette**: 24-bit TrueColor (`\033[38;2;255;140;0m`) with differential double-buffering.
- **Auto-Centering & Retro Bezel**: Dynamically centers the $64 \times 32$ canvas with a dim amber frame on window resize (`SIGWINCH`).
- **Full-Width Status Bar**: Live session metadata (`User/Group/Slot`) and key shortcut reminders.
- **One-Touch Screen Copy (`Ctrl+Y` / `ESC C`)**: Saves instantly to `screen.txt`, system pasteboard, and remote client via ANSI OSC 52.

### 8. Smart Double-Tap Input Engine
- **Solves the Keyboard / Web Terminal Dilemma**: Double-tap within 500 ms activates the Shift modifier:
  - **Double Return** (or `ESC` + `Return`) $\rightarrow$ **`SHIFT-NEXT`** (save & file in PLATO Notes!).
  - **Double F4** or **Double Ctrl+S** $\rightarrow$ **`SHIFT-STOP`** (sign off / logout).
  - **Double F8**, **Double ESC**, or **Double Ctrl+B** $\rightarrow$ **`SHIFT-BACK`** (exit lesson to main index).
  - **Double F1 / Ctrl+H** $\rightarrow$ **`SHIFT-HELP`**, Double F2 $\rightarrow$ `SHIFT-LAB`, Double F3 $\rightarrow$ `SHIFT-DATA`, Double F5 $\rightarrow$ `SHIFT-EDIT`.
- **CRLF De-bounce & Immediate Text**: Standard typing remains 0 ms instantaneous.

### 9. Universal UTF-8 Semigraphics & Box-Drawing Engine
- **Canonical M1 Set**: Translates lines (`│`), double rules (`═`), arrows (`↑ → ↓ ←`), math operators (`≠ ≤ ≥ × ÷ ± ≈`), Greek letters (`α β γ δ ε π μ Σ Δ Θ`), and symbols (`◆ ○`).
- **Topological RAM Font Classifier (M2/M3)**: Dynamically maps downloaded 16-byte glyphs into Unicode box-drawing characters (`┌ ┐ └ ┘ ┼ ├ ┤ ┬ ┴ ─ │ █ ░`).
- **Unified Core**: Shared across Console Mode, Live Text Buffer window, macOS/Windows Copy/Paste, and headless automation!

### 10. Live Text Buffer Companion & Copy/Paste Engine
- **"PLATO Live Text Buffer" Window (`Cmd+Shift+T` / `Ctrl+Shift+T`)**: A dedicated companion window mirroring the active $64 \times 32$ terminal text matrix at 10 fps.
  - **Freeze-on-Selection**: Automatically pauses live streaming when the user selects text or clicks *Select All*, ensuring stable, uninterrupted selection even during fast chat or data streams.
  - **Retro Dark Theme**: High-contrast Orange Plasma typography on deep background with integrated toolbar (`[Select All]`, `[Deselect / Live]`, `[✓] Compact`, `[Copy to Clipboard]`, `[● LIVE]` status badge).
  - **Menu Integration**: Complete Edit menu alignment (*Copy All Text Verbatim*, *Copy All Text Compact*, *Copy Selected Text...*, *Copy Screen Image*, *Paste Text*).
- **Core C11 Text Matrix (`libplato`)**: Tracks all characters in memory (`text_grid[32][64]`), automatically excluding M2/M3 downloadable graphics fonts.

### 11. Headless Remote Automation & CLI Scripting (`--test-script`)
- **SSH & Headless Execution**: Runs autonomously in background and remote SSH sessions without display server requirements.
- **Expanded Command Grammar**:
  - `copy-all [compact] [file:<path> | clipboard | console]`: Dumps full-screen text to disk, clipboard, or prints directly to **standard output (`console`) in real time**.
  - `copy-area <x1> <y1> <x2> <y2> [compact] [dest]`: Extracts specific sub-regions using cell ($0..63 \times 0..31$) or PLATO pixel ($0..511$) coordinates.
  - `display <on | off>`: Orthogonal toggle to disable window compositing while keeping physical decay simulations running.
  - `renderer <plasma | crt | color | crisp | split>`: Scriptable selection of simulated optical engines.
- **In-Memory 4x Optical Screenshots**: Full $2048 \times 2048$ PNG screenshot generation with authentic phosphor decay physics.

### 12. Standalone Linux Appliance & Cross-Compilation
- **Pure C11 Decoupled Console Client**: Zero external dependencies (no X11, Wayland, GTK, SDL, ncurses).
- **Microscopic 100 KB Static Binaries**: Built via Zig (`./build_all.sh`) into 100% statically linked ELF binaries for Linux ARM64 (QNAP NAS, Raspberry Pi) and Linux Intel x86_64.

---

## PlatoLives Scripting Language Reference

The PlatoLives Scripting Language is designed specifically for interacting with PLATO / Cyber1 terminal screens in a deterministic, robust manner.

### Syntax & Grammar

#### 1. Comments & Whitespace
Lines starting with `#` are comments. Blank lines are ignored.
```text
# This is a comment
2. Variables & State Assignment (let)
Variables can store 64-bit signed integers or double-quoted strings:
code
Text
let username = "myuser"
let group = "mygroup"
let password = "mypassword"
let attempts = 0
let found_pos = -1
3. Screen Inspection & Pattern Matching
find_text [regex] "<pattern>" into <var_x>, <var_y>
Instantaneous (0 ms) check. Inspects the current terminal text grid. If the pattern is matched, <var_x> and <var_y> receive the column (0..63) and line (0..31) coordinates. If not found, <var_x> is set to -1.
wait_text [regex] "<pattern>" [timeout <N>s | <N>ms]
Asynchronous Polling. Suspends execution until the specified text appears on the terminal (polled at 50 ms intervals). Default timeout is 5 seconds if omitted.
code
Text
# Verbatim search
find_text "Press  NEXT  to begin" into banner_x, banner_y

# Flexible regex search (matching variable whitespace)
find_text regex "Type +your +CYBIS +name" into user_x, user_y
wait_text regex "Enter +your +password" timeout 10s
4. Keyboard & Mainframe Interaction
send <string_literal | var_name>: Types the string into the terminal using the script's configured char_delay_ms pacing.
key <PLATO_KEY_NAME>: Sends a dedicated PLATO terminal function key.
Supported keys: NEXT, SHIFT-NEXT, BACK, SHIFT-BACK, STOP, SHIFT-STOP, HELP, SHIFT-HELP, LAB, SHIFT-LAB, DATA, SHIFT-DATA, EDIT, SHIFT-EDIT, ANS, TERM, SQUARE, ACCESS, ERASE, TAB, RETURN, ESC.
code
Text
send username
key NEXT
send group
key SHIFT-STOP
5. Program Flow, Branches & Loops
Labels: Defined with @name:.
Unconditional Jump: goto <label_name>
Conditional Branch: if <expr> <op> <expr> goto <label_name>
Supported operators: ==, !=, <, <=, >, >=.
Counted Loop: for <var> = <start> to <end> [step <val>] ... next <var>
Delay: wait <N>s or wait <N>ms
Termination: exit
code
Text
# Example: Retry loop with self-healing recovery
@retry:
let login_ok = -1
find_text regex "Choose +a +lesson" into login_ok, dummy_y
if login_ok >= 0 goto finished

# Send SHIFT-STOP to reset back to main login prompt
for step = 1 to 5
    key SHIFT-STOP
    wait 300ms
next step
goto retry

@finished:
exit
Canonical Cyber1 Adaptive Auto-Login Template
Every new profile and script comes pre-configured with the canonical adaptive login template:
code
Text
# ==============================================================================
# CYBER1 ADAPTIVE AUTO-LOGIN & SELF-HEALING RECOVERY
# ==============================================================================

# Credentials Configuration
let username = "user"
let group = "group"
let password = "password"

@start:

# 1. State check: identify current terminal screen
let pos_user = -1
find_text regex "Type +your +CYBIS +name" into pos_user, dummy_y
if pos_user >= 0 goto do_user

let pos_group = -1
find_text regex "Type +the +name +of +your +CYBIS +group." into pos_group, dummy_y
if pos_group >= 0 goto do_group

let pos_pwd = -1
find_text regex "Enter +your +password" into pos_pwd, dummy_y
if pos_pwd >= 0 goto do_password

let pos_banner = -1
find_text regex "Press +NEXT +to begin" into pos_banner, dummy_y
if pos_banner >= 0 goto do_next

# If no known prompt is active, trigger reset recovery
goto reset

# --- STEP 0: Press initial NEXT ---
@do_next:
key NEXT

# --- STEP 1: Enter Username ---
wait_text regex "Type +your +CYBIS +name" timeout 5s
@do_user:
send username
key NEXT

# --- STEP 2: Enter Group ---
wait_text regex "Type +the +name +of +your +CYBIS +group." timeout 5s
@do_group:
send group
key SHIFT-STOP

# --- STEP 3: Enter Password ---
wait_text regex "Enter +your +password" timeout 5s
@do_password:
send password
key NEXT

# Allow the system 2 seconds to authenticate credentials
wait 2s

# --- STEP 4: Login Verification ---
let login_ok = -1
find_text regex "Choose +a +lesson" into login_ok, dummy_y

# If "Choose a lesson" was not reached, trigger reset recovery
if login_ok < 0 goto reset

# Login succeeded
exit

# ==============================================================================
# SELF-HEALING RESET RECOVERY
# ==============================================================================
@reset:
let r_banner = -1
let r_user = -1

for reset_step = 1 to 10
    key SHIFT-STOP
    wait 400ms

    # Check if we reached username prompt
    find_text regex "Type +your +CYBIS +name" into r_user, dummy_y
    if r_user >= 0 goto do_user

    # Check if we reached initial banner
    find_text regex "Press +NEXT +to begin" into r_banner, dummy_y
    if r_banner >= 0 goto do_next
next reset_step

exit
Technical Specifications
Parameter	Specification
Current Version	v4.2
Supported Platforms	macOS 13+ (Universal), Windows 10/11 (x86_64), Linux ARM64 & x86_64
Protocol	CDC IST-III / Jack Stifle CERL X-20 / CDC 721 (ASCII & Color modes)
Logical Matrix	512 × 512 1-bit bitboard (32 KB) & 32-bit Little-Endian BGRA matrix
Text Grid	64 × 32 matrix with M1 UTF-8 table and M2/M3 topological semigraphics classifier
Scripting Engine	C11 Thread-Safe Micro-Engine with Native Micro-Regex & Keyboard Interception
Console Mode	Native ANSI TrueColor Plasma Amber terminal with double-buffering & auto-centering
Input Engine	Smart 500ms Double-Tap state machine (Return x2 = SHIFT-NEXT, F4 x2 = SHIFT-STOP)
Linux Targets	100% Statically linked, standalone 100 KB ELF binaries (ARM64 and x86_64)
Windows Target	Standalone PE executable, Direct3D 11 hardware acceleration, Win32 Dark Plasma UI
Render Surface	2048 × 2048 32-bit sub-pixel BGRA supersampled surface (4x scale)
Core Architecture	Pure C11 (libplato), thread-safe ring buffer, non-blocking sockets
UI Frameworks	macOS: Objective-C (Cocoa / Core Animation) / Windows: Direct3D 11 & Win32
Compatibility	Cyber1 / CYBIS (cyberserv.org:8005), IRATA.ONLINE (irata.online:8005)
Building and Running
Multi-Platform Unified Build (macOS + Linux + Windows)
With the Zig compiler installed (brew install zig), compile all 4 target platforms and run the full test suite in seconds:
code
Bash
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