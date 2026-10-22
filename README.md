# PlatoLives (v5.0)

## Latest Changes (v5.0)

- **Authentic Tektronix 4014 (DVBST) Emulation**: A dedicated vector storage-tube engine reproducing the optical, electrical, and mechanical behavior of the Direct View Bistable Storage Tube terminals used with PLATO. Features dual-layer dielectric charge simulation, writing-beam transient flare, configurable serial transmission speeds with progressive vector drawing, authentic full-screen flash erase, and P31 emerald green phosphors.
- **Real-Time Vector HUD**: Telemetry overlay displaying vector queue saturation (`QUEUE xx%`), pacing speed, and hardware performance.
- **WADX Keyboard Profile**: A fifth built-in keyboard layout adhering to the classic directional standard of early PLATO and TUTOR games (W=Up, A=Left, D=Right, X=Down).
- **Network Automation in Scripting**: Built-in script interpreter extended with session commands (`disconnect`, `reconnect`, `connect <profile>`) and self-healing auto-login recovery.
- **Connection Diagnostics and Responsive Modals**: Non-blocking connection lifecycle with immediate cancellation (`ESC`), retry on failure (`Enter`), and automated palette synchronization with the active terminal phosphor.
- **Dynamic Theme Palette**: Interface menus and dialogue windows now dynamically adapt their chrominance and highlight colors to match the active phosphor profile.

[![Platform](https://img.shields.io/badge/platform-macOS%2013%2B%20%7C%20Windows%20(x86__64%20%26%20ARM64)%20%7C%20Linux%20(ARM64%20%26%20x86__64)-orange.svg)](#)
[![Language](https://img.shields.io/badge/language-C11%20%7C%20Direct3D%2011%20%7C%20Cocoa%2FMetal%20%7C%20OpenGL%203.3-blue.svg)](#)
[![License](https://img.shields.io/badge/license-CC%20BY--NC--SA%204.0-red.svg)](LICENSE)
[![Standard](https://img.shields.io/badge/standard-CDC%20IST--III%20%2F%20CERL%20X--20%20%2F%20CDC%20721%20%2F%20Tektronix%204014-brightgreen.svg)](#)

<img width="3658" height="1964" alt="PlatoLives Banner" src="https://github.com/user-attachments/assets/8b8e7e8c-1219-4be6-82f9-b20b1591509c" />

**PlatoLives** is a modern native client for **PLATO**, the legendary computer-based education and social system that originated at the University of Illinois.

It connects to PLATO systems such as **Cyber1/CYBIS** and **IRATA.ONLINE**, while preserving something often lost in modern PLATO clients: the feeling of actually sitting in front of a physical PLATO terminal.

The display can be rendered as an authentic **orange gas-plasma display**, a **color CRT**, or a **Tektronix 4014 Direct View Bistable Storage Tube (DVBST)**. Each mode models the physical properties of the original hardware—cell illumination, electron beam flare, dielectric storage charge, scanlines, phosphor persistence, and subtle glass curvature. The result is not simply a terminal window displaying PLATO graphics, but an authentic reproduction of the hardware that originally displayed them.

At the same time, PlatoLives is a modern client. It supports copy and paste, scripting, automation, SSH/headless operation, modern Unicode text, customizable keyboard profiles, and native cross-platform execution on macOS, Windows (x86_64 & ARM64), and Linux.

In addition, PlatoLives includes **local MicroTutor emulation**, allowing it to execute standalone MicroTutor floppy disk images locally without connecting to a host, returning cleanly to an online PLATO session when finished.

📖 **Documentation:** [User and Technical Manual (PDF)](docs/PlatoLives_5.0.0_User_and_Technical_Manual.pdf)

---

# What's new in v5.0

Version 5.0 introduces high-fidelity **Tektronix 4014 (DVBST) vector terminal emulation**, bringing historic storage-tube PLATO access to life alongside gas-plasma and CRT displays.

- **Tektronix 4014 Storage Tube**: Fully modeled Direct View Bistable Storage Tube with dual-layer dielectric simulation, line-drawing transient flare, serial line pacing, and flash erase.
- **Vector Telemetry HUD**: A dedicated heads-up display displaying real-time vector queue saturation, pacing rates, and render performance.
- **WADX Directional Profile**: Fifth built-in keyboard mapping matching early TUTOR conventions (W=Up, A=Left, D=Right, X=Down).
- **Network Scripting Automation**: Added `disconnect`, `reconnect`, and `connect <profile>` script instructions for deterministic auto-login recovery and automated multi-server workflows.
- **Connection Management & Dialogs**: Asynchronous non-blocking connection handling with immediate cancellation (`ESC`), detailed failure diagnostics, instant retry (`Enter`), and theme-aware styling.
- **Dynamic Chromatic Theming**: UI chrome and dialogues adapt their palette in real time to the active display phosphor.

---

# Authentic Tektronix 4014 (DVBST) Emulation

The defining milestone of PlatoLives 5.0 is the physical emulation of the **Tektronix 4014 Direct View Bistable Storage Tube (DVBST)**.

During the late 1960s and 1970s, before plasma panels became standard and raster CRTs gained sufficient memory buffers, Tektronix storage-tube terminals were widely used in engineering departments and research facilities to access PLATO. Recreating this experience requires modeling an entirely different display physics paradigm.
```

+-------------------------------------------------------------------------+  
| TEKTRONIX 4014 PHYSICAL PIPELINE |  
| |  
| [PLATO Protocol] |  
| │ |  
| ▼ (Vector commands: Line, Point, Block, Char) |  
| [Lock-Free Ring Buffer: 8192 Slots] ──► Telemetry (Queue Saturation %) |  
| │ |  
| ▼ |  
| [Hardware Bit-Credit Pacer] (110 to 9600 bps or Instant) |  
| │ |  
| ▼ |  
| [Sub-Pixel Continuous Vector Rasterizer] |  
| │ |  
| ├───────────────────────────────┐ |  
| ▼ ▼ |  
| tek_flare (Bloom & Decay) tek_stored (Bistable Target) |  
| Transient beam write energy Continuous flood-gun maintenance |  
| │ │ |  
| └───────────────┬───────────────┘ |  
| ▼ |  
| P31 Emerald Green Phosphor |  
| (Plus DVBST Full-Screen Flash Erase) |  
+-------------------------------------------------------------------------+

### The Direct View Bistable Storage Tube Model

Standard CRT displays use a focused electron beam that continuously scans raster lines 50 or 60 times a second; when the beam moves on, phosphors decay in milliseconds, requiring a continuous refresh buffer.

The Tektronix DVBST operated completely differently:
1. **Secondary Emission & Dielectric Storage**: The target inside the tube consists of a dielectric phosphor mesh. When the high-energy focused **writing electron beam** traces a vector across the screen, it knocks secondary electrons out of the dielectric, raising its electrical potential above a critical threshold.
2. **Continuous Flood Guns**: Two low-velocity "flood guns" continuously spray electrons across the entire face of the tube. At unwritten points, the target repels these flood electrons. But where the writing beam has created a positive charge, the flood electrons are accelerated into the phosphor, maintaining the glowing image **indefinitely without raster refresh, memory buffers, or flicker**.

PlatoLives implements this bistable physics through a dual-buffer optical state:
- **`tek_stored` (Dielectric Charge Layer)**: Represents the stable, binary charge stored on the target mesh. Once written, vectors remain pinned at their stored equilibrium potential until an electrical erase pulse resets the target.
- **`tek_flare` (Transient Beam Energy Layer)**: Models the intense, localized optical blooming that occurs under the tip of the writing electron beam while moving across the phosphor. As vectors are traced, the flare illuminates brightly and decays smoothly, mirroring the characteristic green tail seen on real storage tubes.

### Hardware-Accurate Serial Transmission Pacer

Historical Tektronix terminals were serial devices connected via current-loop or RS-232 interfaces, ranging from 110 to 9600 baud. Complex vector drawings did not appear instantaneously; they were drawn progressively, line by line, across the dark green phosphor.

PlatoLives models this transmission pacing at the hardware level:
- A high-resolution **bit-credit accumulator** paces the ingestion of vector commands based on the selected serial speed: **110, 150, 300, 600, 1200, 2400, 4800, or 9600 baud** (with an **Instant** option available for immediate drawing).
- As bytes arrive, vector coordinates are drawn progressively in real time. Fast line segments, axes, mathematical curves, and character vectors are visible as they are traced onto the storage mesh.

### Lock-Free 8192-Slot Vector Event Queue

To ensure absolute decoupling between the network transport, protocol decoding, and physical vector rendering:
- A dedicated, lock-free typed ring buffer (`plato_vector_queue_t`) holds up to **8192 vector events** (`VEC_EVENT_LINE`, `VEC_EVENT_POINT`, `VEC_EVENT_ERASE`, `VEC_EVENT_CHAR`, and `VEC_EVENT_BLOCK`).
- Decoders push vector drawing commands into the ring buffer with sub-pixel floating-point coordinates.
- The optical pacer consumes from the queue at the precise serial rate, enabling accurate telemetry.

### DVBST Full-Screen Flash Erase & Grace Period

Because storage tubes cannot selectively erase individual lines without clearing the entire dielectric target, erasing a Tektronix terminal was an unmistakable physical event:
- An erase cycle applies a positive voltage pulse to the target mesh, causing the entire display to **flash brilliant emerald green** as secondary emission saturates the screen, followed by a brief dark settling phase.
- PlatoLives reproduces the exact duration and optical intensity curve of the Tektronix erase flash.
- To prevent distracting strobing during rapid screen updates, an internal **anti-strobe grace period filter** smooths consecutive clearing requests without compromising historical accuracy.

### P31 Emerald Green Phosphor Calibration

The Tektronix 4014 utilized **P31 phosphor**, prized for its balance of high visual efficiency, bright emerald-green hue (peak emission around 525 nm), and suitable secondary-emission characteristics. PlatoLives precisely calibrates this spectral signature across its rendering backends (Metal on macOS, Direct3D 11 on Windows, and OpenGL 3.3 Core on Linux).

---

# What makes PlatoLives different?

There are plenty of ways to connect to PLATO. PlatoLives is intended for people who want something closer to **using a PLATO terminal**, rather than simply accessing a PLATO server.

### Authentic Hardware Display Modes

PlatoLives provides three primary display models:

1. **Real Plasma**: Recreates the iconic orange gas-plasma panels developed for PLATO IV at the University of Illinois, simulating the micro-cell ionization, neon glow, and bistable discharge of physical plasma panels.
2. **Real Color CRT**: Recreates the CDC color terminals, featuring RGB phosphors, electron beam blooming, scanline structures, and configurable CRT profiles.
3. **Real Tektronix 4014 (DVBST)**: The historical vector storage tube experience with progressive line tracing, P31 phosphors, and flash erase.

Each mode supports physical glass curvature simulation and optical touch compensation.

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

The environment includes emulation of the original Z80 processor and floppy controller environment required by the interpreter. MicroTutor is automatically bypassed when operating in Tektronix 4014 mode, matching historical hardware limits.

---

# Automation, Scripting & Recovery

PlatoLives features a built-in state-aware scripting system.

PLATO sessions often involve repetitive sequences: logging in, waiting for prompts, navigating menus, or recovering from connection drops.

### Network Automation Commands

Version 5.0 extends the scripting engine with session control instructions:
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
- **View**: Real Plasma, Crisp Mono, Split Mono, Crisp Color, Real CRT, Real Tektronix 4014 (DVBST), Tektronix Connection Speed, Hide Menu Bar, Show Performance HUD (`F12`)
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
| **Cyber1 / CYBIS** | Supported | Full IST-III, Color & Tektronix Protocols |
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
```

Artifacts are output to build/:

codeText

```
PlatoLives.app
PlatoLives-windows-x86_64.exe
PlatoLives-windows-arm64.exe
PlatoLives-linux-gui-arm64
PlatoLives-linux-gui-x86_64
PlatoLives-console-linux-arm64
PlatoLives-console-linux-x86_64
build/linux_desktop_install/
```

### Native macOS build

Requirements: macOS 13+, Xcode Command Line Tools, CMake 3.16+.

codeBash

```
git clone git@github.com:TheSynthMaster/PlatoLives.git
cd PlatoLives
mkdir build
cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
ctest --output-on-failure
```

----------

# Technical Details

Component

Details

**Core**

Portable C11

**Display Modes**

Real Gas-Plasma, Color CRT, Real Tektronix 4014 (DVBST)

**Logical Display**

512 × 512 PLATO vector and text grid

**Vector Engine**

Lock-free 8192-slot ring buffer with hardware bit-credit pacer

**DVBST Physics**

Dual-layer dielectric charge (tek_stored) and writing-beam flare (tek_flare)

**Text Matrix**

64 × 32 character matrix with downloadable font recognition

**macOS Rendering**

Cocoa / Core Animation / Metal

**Windows Rendering**

Win32 / Direct3D 11 (Native x86_64 and ARM64)

**Linux Rendering**

OpenGL 3.3 Core / SDL2 / stb_truetype

**Input Engine**

5 Built-in layouts (Standard, WASD, WASX, WADX, Numpad), custom remapping

**Networking**

Non-blocking BSD/WinSock2 TCP with immediate cancel and auto-retry

**Automation**

State-aware scripting interpreter with network session control

**MicroTutor**

Pure C11 Z80 CPU and floppy controller emulation

**Versioning**

Single Source of Truth via root VERSION file

----------

# License

PlatoLives is released under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International license.  
See LICENSE for the complete license text.
