# PlatoLives (v5.5)

## Latest Changes (v5.5)

- **DEC PDP-1 Type 30 Display Emulation**: Introduces a point-plotting CRT display model with a 1024×1024 native coordinate space, 2× optical supersampling, and simulated P7 phosphor persistence and color shift.
- **Historic PDP-1 Fonts**: Adds two character generators: the 1961 MIT Codeword font and the 1964 DEC Type 33 font.
- **Configurable Display Response**: Offers selectable cathode refresh rates (60, 45, 30, 15, 10, and 5 Hz) and three optical presets: Historic (15s), Historic Fast Decay (5s), and Modern (2s).
- **Cross-platform Support**: PDP-1 emulation is available on macOS, Windows (x86_64 and ARM64), and Linux desktop.

[![Platform](https://img.shields.io/badge/platform-macOS%2013%2B%20%7C%20Windows%20(x86__64%20%26%20ARM64)%20%7C%20Linux%20(ARM64%20%26%20x86__64)-orange.svg)](#)
[![Language](https://img.shields.io/badge/language-C11%20%7C%20Direct3D%2011%20%7C%20Cocoa%2FMetal%20%7C%20OpenGL%203.3-blue.svg)](#)
[![License](https://img.shields.io/badge/license-CC%20BY--NC--SA%204.0-red.svg)](LICENSE)
[![Standard](https://img.shields.io/badge/standard-CDC%20IST--III%20%2F%20CERL%20X--20%20%2F%20CDC%20721%20%2F%20Tektronix%204014%20%2F%20DEC%20PDP--1-brightgreen.svg)](#)

<img width="3658" height="1964" alt="PlatoLives Banner" src="https://github.com/user-attachments/assets/8b8e7e8c-1219-4be6-82f9-b20b1591509c" />

**PlatoLives** is a modern native client for **PLATO**, the legendary computer-based education and social system that originated at the University of Illinois.

It connects to PLATO systems such as **Cyber1/CYBIS** and **IRATA.ONLINE**, while preserving something often lost in modern PLATO clients: the tactile, optical, and physical feeling of sitting in front of real vintage terminal hardware.

The display can be rendered as an authentic **orange gas-plasma display**, a **color CRT**, a **Tektronix 4014 Direct View Bistable Storage Tube (DVBST)**, or a **DEC PDP-1 Type 30 Precision CRT Display**. Each mode models the physical properties of the original hardware—cell illumination, electron beam flare, cascade phosphors, dielectric storage charge, scanlines, and subtle glass curvature. The result is not simply a terminal window displaying PLATO graphics, but an authentic reproduction of the computing hardware that originally displayed them.

At the same time, PlatoLives is a modern client. It supports copy and paste, scripting, automation, SSH/headless operation, modern Unicode text, customizable keyboard profiles, and native cross-platform execution on macOS, Windows (x86_64 & ARM64), and Linux.

In addition, PlatoLives includes **local MicroTutor emulation**, allowing it to execute standalone MicroTutor floppy disk images locally without connecting to a host, returning cleanly to an online PLATO session when finished.

📖 **Documentation:** [User and Technical Manual (PDF)](docs/PlatoLives_5.0.0_User_and_Technical_Manual.pdf)

---

# What's new in v5.5

Version 5.5 introduces emulation of the **DEC PDP-1 Type 30 Precision CRT Display**, adding a point-plotting display model alongside the existing terminal display options.

- **PDP-1 Display Engine**: Models point plotting using a 1024×1024 native coordinate space, supersampled to a 2048×2048 optical canvas, with sub-pixel beam placement.
- **P7 Phosphor Simulation**: Reproduces the blue/cyan prompt flash and its transition into a longer-lasting golden-amber glow.
- **Historic Character Generators**: Includes the 1961 MIT Codeword font and the 1964 DEC Type 33 character generator.
- **Display Controls**: Configurable refresh rates (60, 45, 30, 15, 10, and 5 Hz), plus Historic (15s), Historic Fast Decay (5s), and Modern (2s) response presets.

---

# Authentic DEC PDP-1 Type 30 CRT Display Emulation

The defining milestone of PlatoLives 5.5 is the physical emulation of the **DEC PDP-1 Type 30 Precision Display**, the display on which computer graphics history and the first video game (*Spacewar!*, 1962) were born.
+-------------------------------------------------------------------------+
| DEC PDP-1 TYPE 30 PHYSICAL PIPELINE |
| |
| [PLATO Point/Vector Stream] |
| │ |
| ▼ |
| [1024×1024 Precision Coordinate Addressing (Type 30 DACs)] |
| │ |
| ▼ |
| [Hardware Font Generator: 1961 Codeword / 1964 Type 33] |
| │ |
| ▼ |
| [Dynamic Cathode Refresh Pacer] (60, 45, 30, 15, 10, 5 Hz) |
| │ |
| ▼ |
| [2× Supersampled Optical Accumulator: 2048×2048 Canvas] |
| │ |
| ┌───────────────────────┴───────────────────────┐ |
| ▼ ▼ |
| Prompt Excitation Flash (ZnS:Ag) Long-Persistence Phosphorescence |
| Blue/Cyan Transient Flare (τ = 350 ms) ZnS:Cu Golden-Amber Shift (P7)|
| PSF Bloom (σ = 1.100f / 0.450f) Decay: 15s (Hist) / 5s / 2s |
| └───────────────────────┬───────────────────────┘ |
| ▼ |
| P7 Radar Turquoise Spectrum (#00D4AA) |
| (Plus Custom Phosphor Overrides) |
+-------------------------------------------------------------------------+
code
Code
### Point-Plotting Precision CRT Architecture

Unlike raster displays that sweep an electron beam line by line across a phosphor grid, the DEC Type 30 was a **point-plotting vector display**. The computer addressed points individually using two 10-bit digital-to-analog converters, projecting an electron beam onto a circular 16-inch cathode ray tube.

PlatoLives faithfully models this architecture:
- **Decoupled Framebuffers**: The terminal maintains an internal $512 \times 512$ PLATO logic coordinate system alongside an independent $1024 \times 1024$ PDP-1 physical landing buffer (`fb_pdp`), preventing ghosting and coordinate aliasing.
- **$2\times$ Optical Supersampling**: To capture the microscopic optical blooming of individual CRT points, the rendering pipeline rasterizes into a continuous $2048 \times 2048$ high-resolution canvas before presenting to the GPU backbuffer.
- **Point Spread Function (PSF) Bloom**: Beam intensity follows a Gaussian point spread profile. The *Historic* preset utilizes $\sigma = 1.100\text{f}$ for authentic CRT beam dispersion, while the *Modern* preset offers $\sigma = 0.450\text{f}$ for ultra-sharp dot isolation.

### Physical P7 Cascade Phosphor & Chromatic Shift

The PDP-1 Type 30 utilized a **P7 cascade phosphor**, originally designed for radar displays during World War II:
1. **Prompt Excitation Layer ($\text{ZnS:Ag}$)**: When hit by the electron beam, the base layer instantly emits an intense, short-lived blue/cyan flash ($\tau = 350\text{ ms}$).
2. **Persistent Phosphorescent Layer ($\text{ZnS:Cu}$)**: This prompt light excites a second phosphor layer that emits a long-lasting yellow-green/golden-amber glow, persisting for several seconds.

PlatoLives simulates both layers independently, reproducing the distinctive historical chromatic shift: plotted points ignite with a bright cyan-blue flare, transitioning smoothly into a warm golden-amber trail as they decay.

### Hardware Font Generators: 1961 Codeword vs 1964 Type 33

PlatoLives includes two distinct historical font implementations:
- **1961 Codeword (Spacewar!)**: The original 5x7 stroke font crafted by Steve Russell, Martin Graetz, and Wayne Wiitanen at MIT.
- **1964 Character Generator (Type 33)**: The hardware-driven character generator option introduced for production DEC PDP-1 terminals.

### Configurable Refresh & Presets

- **Dynamic Refresh Pacing**: 60 Hz, 45 Hz (historical default), 30 Hz, 15 Hz, 10 Hz, and 5 Hz.
- **Presets**:
  - `Historic (15s)`: Full 15-second natural P7 persistence and wide beam bloom.
  - `Historic Fast Decay (5s)`: Balanced persistence for high-speed terminal interaction.
  - `Modern (2s)`: Sharp, separated points with rapid settling.

---

# What makes PlatoLives different?

There are plenty of ways to connect to PLATO. PlatoLives is intended for people who want something closer to **using a physical PLATO terminal**, rather than simply accessing a PLATO server.

### Four Authentic Hardware Display Models

1. **Real Plasma**: Recreates the iconic orange gas-plasma panels developed for PLATO IV at the University of Illinois, simulating micro-cell ionization, neon glow, and bistable discharge.
2. **Real Color CRT**: Recreates the CDC color terminals, featuring RGB phosphors, electron beam blooming, scanline structures, and configurable CRT profiles.
3. **Real Tektronix 4014 (DVBST)**: Vector storage-tube experience with progressive line tracing, P31 phosphors, slow electron background noise, and full-screen flash erase.
4. **Real DEC PDP-1 Type 30 (1964)**: Precision point-plotting CRT display featuring P7 cascade phosphors, chromatic decay shift, and historic MIT font generators.

Each mode supports glass curvature simulation, optical touch compensation, and phosphor overrides (P31 Green, P40 Amber, P4 White).

### Modern hardware, native applications

PlatoLives is a native application rather than a web wrapper:

- **macOS 13+**, native Cocoa application running on Apple Silicon and Intel Macs (Metal / Core Animation)
- **Windows 10/11**, native Win32 application for **x86_64** and **ARM64** (Direct3D 11)
- **Linux ARM64 & x86_64 Desktop GUI** (OpenGL 3.3 Core Profile + SDL2 + vector TrueType typography)
- **Linux Standalone Console CLI** (Static, zero-dependency ANSI TrueColor binaries for Raspberry Pi, NAS, and remote servers)

### Keyboard Profiles & Double-Tap Emulation

PlatoLives ships with five built-in keyboard profiles and full custom remapping:

- **Standard PLATO** — the canonical PLATO keyboard layout.
- **WASD Gaming** — directional navigation with action shortcuts.
- **WASX Avatar** — layout tailored for Avatar and early dungeon crawlers.
- **WADX** — classic PLATO/TUTOR directional matrix (W=Up, A=Left, D=Right, X=Down).
- **Numpad Roguelike** — eight-way directional movement using keys 1–9, plus 13 configurable game actions for *Avatar*, *Oubliette*, *dnd*, and *Moria*.

When dedicated PLATO keys are unavailable, the double-tap input system provides immediate access:
- double Return → **SHIFT-NEXT**
- double F4 → **SHIFT-STOP**
- double F8 → **SHIFT-BACK**
- double F1 → **SHIFT-HELP**
- double F2 → **SHIFT-LAB**
- double F3 → **SHIFT-DATA**
- double F5 → **SHIFT-EDIT**

---

# Local MicroTutor

PlatoLives includes support for running **MicroTutor floppy images locally**.

MicroTutor was one of the premier educational environments built for PLATO, distributed as standalone floppy disk images containing the interpreter and lesson data.

PlatoLives can load supported `.mte` floppy images and execute them locally, without requiring a network connection to Cyber1 or another host:

- load a local MicroTutor floppy image
- start the lesson directly from the application
- interact using the authentic PLATO display and keyboard
- run lessons completely offline
- return to a normal online PLATO session cleanly

The environment includes emulation of the original Z80 processor and floppy controller environment required by the interpreter. MicroTutor is automatically bypassed when operating in Tektronix 4014 or DEC PDP-1 mode, matching historical hardware limits.

---

# Automation, Scripting & Recovery

PlatoLives features a built-in state-aware scripting system.

PLATO sessions often involve repetitive sequences: logging in, waiting for prompts, navigating menus, or recovering from connection drops.

### Network Automation Commands

- `disconnect`: Gracefully closes active network transports, resets the protocol decoder, and clears the terminal.
- `reconnect`: Atomically reconnects to the active host and port, providing immediate self-healing recovery if a session desynchronizes.
- `connect <profile_name>`: Connects to a named connection profile from the Profile Manager.
- `connect standard`: Connects directly to the primary default server (Cyber1).
- `connect last`: Reconnects to the most recently active session endpoint.

The standard auto-login script (`PLATO_DEFAULT_AUTOLOGIN_SCRIPT`) utilizes `reconnect` in its `@reset` recovery branch, ensuring deterministic recovery without manual intervention.

---

# Connection Management & Diagnostics

Connecting to remote PLATO systems is handled through an asynchronous, non-blocking architecture:
- **Immediate Cancellation**: During connection attempts, pressing `ESC` or clicking `[ Cancel ]` aborts socket operations immediately without freezing the user interface.
- **Detailed Error Diagnostics**: In the event of network failure (unreachable host, refused connection, timeout), the connection dialog reports the specific cause of error.
- **Instant Retry**: Pressing `Enter` or clicking `[ Retry ]` initiates an immediate reconnection attempt.
- **Theme-Synchronized Interface**: Connection windows are rendered natively (GDI double-buffering on Windows, Cocoa on macOS, SDL2 vector text on Linux) and dynamically inherit the active terminal phosphor palette.

---

# Menus and Navigation

Unified menu structure across macOS, Windows, and Linux:

- **Connection**: Connect, Switch Profile, New Window, Reconnect, Disconnect, Profiles Manager, Close Window, Quit
- **Keyboard**: Standard PLATO, WASD Gaming, WASX Avatar, WADX, Numpad Roguelike, Manage Keyboard Profiles..., Keyboard Reference (`Cmd+K` / `Ctrl+K`)
- **Scripts**: Run, Manage Scripts, Scripting Reference
- **View**:
  - Real Plasma, Crisp Mono, Split Mono, Crisp Color, Real CRT
  - Real Tektronix 4014 (DVBST), Tektronix Connection Speed, Erase Flash, Accurate Background
  - Real DEC PDP-1 Type 30 (1964), PDP-1 Font Generator, PDP-1 Refresh Rate, PDP-1 Display Mode
  - Full Screen (`F11`), Show Performance HUD (`F12`), Hide Menu Bar (`Ctrl+Shift+M`), Override Color
- **Edit**: Copy Text, Copy Screen, Paste Text, Cancel Paste
- **Micro Tutor**: Load and Boot from MTE..., Stop MicroTutor, Z80 Diagnostics
- **Window (macOS)**: Tab management and window controls
- **Tools**: Diagnostic and Protocol Logging
- **Help**: Keyboard Reference, About PlatoLives

---

# Supported Systems

| Platform | Interface | Rendering Engine |
| :--- | :--- | :--- |
| **macOS 13+** | Cocoa Native Desktop | Metal / Core Animation |
| **Windows 10/11 (x86_64)** | Win32 Native Desktop | Direct3D 11 |
| **Windows 10/11 (ARM64)** | Win32 Native Desktop | Direct3D 11 (Native AArch64) |
| **Linux ARM64 & x86_64** | Native Desktop GUI | OpenGL 3.3 Core + SDL2 |
| **Linux Console (ARM64 & x86_64)** | ANSI TrueColor CLI | Headless / Terminal (Static musl) |
| **Cyber1 / CYBIS** | Supported | Full IST-III, Color, Tektronix & PDP-1 Protocols |
| **IRATA.ONLINE** | Supported | Standard ASCII / PLATO |
| **MicroTutor Floppy Images** | Local Emulation | Integrated Z80 CPU & Disk Controller |

---

# Building

PlatoLives can be built natively on macOS or cross-compiled for all supported platforms using Zig.

### Build all platforms

With Zig installed:

```bash
brew install zig
./build_all.sh
Artifacts are output to build/:
code
Text
PlatoLives.app
PlatoLives-windows-x86_64.exe
PlatoLives-windows-arm64.exe
PlatoLives-linux-gui-arm64
PlatoLives-linux-gui-x86_64
PlatoLives-console-linux-arm64
PlatoLives-console-linux-x86_64
build/linux_desktop_install/
Native macOS build
Requirements: macOS 13+, Xcode Command Line Tools, CMake 3.16+.
code
Bash
git clone git@github.com:TheSynthMaster/PlatoLives.git
cd PlatoLives
mkdir build
cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
ctest --output-on-failure
Technical Details
Component	Details
Core	Portable C11
Display Modes	Real Gas-Plasma, Color CRT, Real Tektronix 4014 (DVBST), DEC PDP-1 Type 30 (1964)
Logical Display	
512
×
512
512×512
 PLATO vector and text grid
PDP-1 Optical Canvas	
1024
×
1024
1024×1024
 native point space supersampled to 
2048
×
2048
2048×2048
 canvas
PDP-1 Phosphor Model	P7 Cascade: 
ZnS:Ag
ZnS:Ag
 prompt flash (
τ
=
350
 ms
τ=350 ms
) + 
ZnS:Cu
ZnS:Cu
 yellow-amber phosphorescence
Vector Engine	Lock-free 8192-slot ring buffer with hardware bit-credit pacer
DVBST Physics	Dual-layer dielectric charge (tek_stored) and writing-beam flare (tek_flare)
Text Matrix	
64
×
32
64×32
 character matrix with downloadable font recognition
macOS Rendering	Cocoa / Core Animation / Metal
Windows Rendering	Win32 / Direct3D 11 (Native x86_64 and ARM64)
Linux Rendering	OpenGL 3.3 Core / SDL2 / stb_truetype
Input Engine	5 Built-in layouts (Standard, WASD, WASX, WADX, Numpad), custom remapping
Networking	Non-blocking BSD/WinSock2 TCP with immediate cancel and auto-retry
Automation	State-aware scripting interpreter with network session control
MicroTutor	Pure C11 Z80 CPU and floppy controller emulation
Versioning	Single Source of Truth via root VERSION file
License
PlatoLives is released under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International license.
See LICENSE for the complete license text.
