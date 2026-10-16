# PlatoLives (v4.7)

## Latest Changes (v4.7)
- **Universal Keyboard Profiles & Custom Keymapping Engine**:
  - High-performance, pure C11 profile and keymapping engine (`plato_keymap_manager`) with JSON persistence and instant $O(1)$ action dispatch.
  - Native "Manage Keyboard Profiles" management modal across macOS (Cocoa), Windows (Win32), and Linux (SDL2/OpenGL).
  - Interactive "Press Key to Bind" interface with live hardware keycode detection (VKeys) and Universal Action Picker.
  - 4 Built-in profiles curated for authentic PLATO interaction:
    - **Standard PLATO**: The canonical terminal layout and function keys.
    - **WASD Gaming**: Modern 4-way navigation with accessible action shortcuts.
    - **WASX Avatar**: Authentic cardinal and diagonal directional layout for PLATO classics.
    - **Numpad Roguelike**: True 8-way directional movement matrix (keys 1-9) and 13 action keybindings designed for legendary PLATO dungeon crawlers (*Avatar*, *Oubliette*, *dnd*).
- **Native Windows ARM64 Support**:
  - Official 64-bit ARM executable (`PlatoLives-windows-arm64.exe`) compiled directly for Windows on ARM.
  - Native Direct3D 11 hardware rendering with optimal battery life and zero emulation overhead on Snapdragon X Elite laptops and Parallels Desktop on Apple Silicon.
- **Unified Multi-Target Distribution**:
  - Synchronized build pipeline producing native bundles for macOS (DMG), Windows (x86_64 and ARM64 executables), Linux (Desktop graphical installer), and Linux Console (headless static musl binaries).

[![Platform](https://img.shields.io/badge/platform-macOS%2013%2B%20%7C%20Windows%20(x86__64%20%26%20ARM64)%20%7C%20Linux%20(ARM64%20%26%20x86__64)-orange.svg)](#)
[![Language](https://img.shields.io/badge/language-C11%20%7C%20Direct3D%2011%20%7C%20Cocoa%2FMetal%20%7C%20OpenGL%203.3-blue.svg)](#)
[![License](https://img.shields.io/badge/license-CC%20BY--NC--SA%204.0-red.svg)](LICENSE)
[![Standard](https://img.shields.io/badge/standard-CDC%20IST--III%20%2F%20CERL%20X--20%20%2F%20CDC%20721-brightgreen.svg)](#)

<img width="3658" height="1964" alt="PlatoLives Banner" src="https://github.com/user-attachments/assets/8b8e7e8c-1219-4be6-82f9-b20b1591509c" />

**PlatoLives** is a modern native client for **PLATO**, the legendary computer-based education and social system that originated at the University of Illinois.

It connects to PLATO systems such as **Cyber1/CYBIS** and **IRATA.ONLINE**, while preserving something often lost in modern PLATO clients: the feeling of actually sitting in front of a physical PLATO terminal.

The display can be rendered as either the original-style **orange gas-plasma display** or as a **color CRT**, complete with phosphor persistence, beam characteristics, scanlines and subtle glass curvature. The result is not simply a terminal window displaying PLATO graphics, but an authentic reproduction of the hardware that originally displayed them.

At the same time, PlatoLives is a modern client. It supports copy and paste, scripting, automation, SSH/headless operation, modern Unicode text, customizable keyboard profiles, and native cross-platform execution on macOS, Windows (x86_64 & ARM64), and Linux.

In addition, PlatoLives includes **local MicroTutor emulation**, allowing it to execute standalone MicroTutor floppy disk images locally without connecting to a host, returning cleanly to an online PLATO session when finished.

📖 **Documentation:** [User and Technical Manual (PDF)](docs/PlatoLives_3.5_User_and_Technical_Manual.pdf)

---

# What's new in v4.7

- **Universal Keyboard Profiles**: A completely modular keymapping subsystem allows creating, duplicating, remapping, and saving custom keyboard profiles in pure C11. Users can switch instantly between layouts via the top-level **Keyboard** menu or cycle them on the fly (`Cmd+Shift+G` / `Ctrl+Shift+G`).
- **Numpad Roguelike Matrix**: Built-in support for 8-way directional navigation and 13 dedicated game actions specifically calibrated for Cyber1/PLATO multi-user dungeon crawlers (*Avatar*, *Oubliette*, *dnd*, *Moria*).
- **Interactive Remapping Modals**: Platform-native modal dialogs on macOS (Cocoa), Windows (Win32), and Linux (SDL2/OpenGL) provide an effortless "Press Key to Bind" workflow.
- **Native Windows ARM64**: Full native Direct3D 11 support for Qualcomm Snapdragon X Elite systems and Windows 11 ARM64 virtual machines, eliminating Prism translation overhead.

---

# What makes PlatoLives different?

There are plenty of ways to connect to PLATO. PlatoLives is intended for people who want something closer to **using a PLATO terminal**, rather than simply accessing a PLATO server.

### A real PLATO-style display

PlatoLives has two main display modes:

**Real Plasma** recreates the characteristic orange glow of the original PLATO plasma panels, including the way individual cells illuminate and fade.

**Real Color CRT** recreates the later color PLATO terminals, including RGB phosphors, beam characteristics, persistence and scanlines. It supports full Cyber1 color mode and configurable CRT profiles.

Both modes can simulate the curvature of a physical CRT or plasma display.

The intention is not to turn the screen into an exaggerated retro effect. The display remains sharp and perfectly usable; the physical modeling reproduces the genuine optical character of original terminals.

### Modern hardware, native applications

PlatoLives is a native application rather than a web wrapper or an emulator running inside another environment:

- **macOS 13+**, native Cocoa application running on Apple Silicon and Intel Macs (Metal / Core Animation)
- **Windows 10/11**, native Win32 application for **x86_64** and **ARM64** (Direct3D 11)
- **Linux ARM64 & x86_64 Desktop GUI** (OpenGL 3.3 Core Profile + SDL2 + vector TrueType typography)
- **Linux Standalone Console CLI** (Static, zero-dependency ANSI TrueColor binaries for Raspberry Pi, NAS and remote servers)

### PLATO is more than a screen

PLATO had a distinctive keyboard and interaction model. PlatoLives supports PLATO function keys directly, while its double-tap input system provides natural alternatives when a particular key is unavailable:

- double Return → **SHIFT-NEXT**
- double F4 → **SHIFT-STOP**
- double F8 → **SHIFT-BACK**
- double F1 → **SHIFT-HELP**
- double F2 → **SHIFT-LAB**
- double F3 → **SHIFT-DATA**
- double F5 → **SHIFT-EDIT**

Normal typing remains immediate; double-tap only activates when the same key is pressed twice within a short interval.

---

# Local MicroTutor

PlatoLives includes support for running **MicroTutor floppy images locally**.

MicroTutor was one of the premier educational environments built for PLATO, distributed as standalone floppy disk images containing the interpreter and lesson data.

PlatoLives can load supported `.mte` floppy images and execute them locally, without requiring a network connection to Cyber1 or another host:

- load a local MicroTutor floppy image
- start the lesson directly from the application
- interact using the authentic PLATO display and keyboard
- run lessons completely offline
- return to a normal online PLATO session when finished

The environment includes emulation of the original Z80 processor and floppy controller environment required by the interpreter.

### MicroTutor color

Color MicroTutor lessons are supported as well. Some interpreters use their own mechanisms for handling color rather than relying entirely on standard PLATO protocols. PlatoLives handles these cases so supported color lessons render with genuine PLATO color palettes.

### Switching between MicroTutor and online PLATO

A local MicroTutor session is fully isolated from online connections. When MicroTutor is stopped, PlatoLives resets the terminal state cleanly before returning online, preventing characters, downloaded fonts, or graphics state from leaking into subsequent sessions.

---

# Automation and scripting

PlatoLives features a built-in state-aware scripting system.

PLATO sessions often involve repetitive sequences: logging in, waiting for prompts, selecting lessons, navigating menus, or recovering from connection interruptions.

PlatoLives scripts interact with the logical PLATO screen rather than sending blind keystrokes:

- search for text on the screen
- wait for specific prompts
- use regular expressions when spacing or formatting varies
- send text and PLATO function keys
- store variables, evaluate expressions and loop
- jump between script sections with labels
- implement automated recovery procedures
- capture the current PLATO screen to disk
- terminate automatically when the desired state is reached

Scripts can be executed interactively from the application or remotely in headless mode. Scripts are automatically halted if a local MicroTutor session is started.

---

# Manage Scripts

PlatoLives includes an integrated script manager for creating and maintaining scripts without manual file editing:

- create, clone, delete, and edit scripts
- assign custom keyboard shortcuts
- configure individual timing parameters
- trigger scripts directly from the **Scripts** menu

While a script is running, a subtle visual indicator in the terminal shows that automated input is active.

---

# Headless and SSH operation

PlatoLives can be used without a graphical interface.

The console version allows a PLATO session to run directly from a standard terminal:

```bash
./PlatoLives --console
```

Connect directly to a host:

codeBash

```
./PlatoLives --c cyberserv.org 8005
```

The standalone script runner can execute a complete script without starting a GUI:

codeBash

```
./PlatoLives --script login.plato
```

The --test-script option remains available as a compatibility alias.

Display the scripting reference from the command line:

codeBash

```
./PlatoLives --script --help
```

The console client uses ANSI TrueColor and reproduces the PLATO text display, including semigraphics. It is useful over SSH and on remote Linux servers.

### Screenshots from scripts

Scripts can capture the current PLATO display directly to an image file:

codeText

```
screenshot "screen.png"
```

BMP is also supported:

codeText

```
screenshot "screen.bmp"
```

Screenshots capture the logical 512 × 512 PLATO display using the active palette. The legacy salva-screen and schermata commands remain supported for backward compatibility.

### Linux appliances

The Linux command-line version is intentionally compact and self-contained.  
It has no dependency on X11, Wayland, GTK, SDL, ncurses, or other GUI frameworks. The resulting binaries are statically linked and can be deployed directly on ARM64 or x86_64 systems such as:

-   Raspberry Pi
    
-   NAS systems
    
-   small home servers
    
-   remote Linux virtual machines
    

----------

# Working with PLATO text

PLATO's character system predates Unicode by decades. PlatoLives translates the common PLATO character set into Unicode where possible, including:

-   box drawing
    
-   arrows
    
-   mathematical symbols
    
-   Greek characters
    
-   geometric symbols
    

It also recognizes downloaded M2/M3 character sets and identifies common box-drawing graphics from their bitmap patterns.

### Live Text Buffer

PlatoLives includes a dedicated Live Text Buffer window for interacting with the textual content of the current PLATO screen.  
It mirrors the 64 × 32 PLATO text matrix and allows selecting and copying text while the terminal runs. When text is selected, the buffer temporarily suspends updates so that selections remain stable.

The window supports:

-   Select All
    
-   copying selected or complete screen text
    
-   compact text copying
    
-   copying screen images to clipboard
    
-   pasting text back into PLATO
    

----------

# Connection Profiles

Multiple connection profiles can be stored and selected from the application. A profile stores host and port information and can optionally launch a startup script. Profiles and scripts persist in the user's application configuration directory.

----------

# Touch and Curved Displays

PlatoLives supports PLATO touch interaction through the FGT touch protocol. Mouse and touch coordinates are compensated for optical distortion introduced by simulated curved CRT or plasma displays before being translated into PLATO touch input.

----------

# macOS Integration

The macOS application provides native desktop ergonomics:

-   Cmd+T opens a new tab
    
-   Cmd+[ and Cmd+] switch between tabs
    
-   Cmd+1 through Cmd+9 select tabs directly
    
-   tab bar + button to open new tabs
    
-   standard Window menu with native minimization (Cmd+M)
    
-   Live Text Buffer shortcut Cmd+Shift+B
    

----------

# Menus and Navigation

Menu structures are unified across macOS, Windows, and Linux:

-   **Connection**: Connect, Switch Profile, New Window, Reconnect, Disconnect, Profiles Manager, Close Window, Quit
    
-   **Keyboard**: Standard PLATO, WASD Gaming, WASX Avatar, Numpad Roguelike, Manage Keyboard Profiles..., Keyboard Reference (Cmd+K / Ctrl+K)
    
-   **Scripts**: Run, Manage Scripts, Scripting Reference
    
-   **View**: Real Plasma, Crisp Mono, Split Mono, Crisp Color, Real CRT, Hide Menu Bar, Show Performance HUD (F12)
    
-   **Edit**: Copy Text, Copy Screen, Paste Text, Cancel Paste
    
-   **Micro Tutor**: Load and Boot from MTE..., Stop MicroTutor, Z80 Diagnostics
    
-   **Window (on macOS)**: Tab management and window controls
    
-   **Tools**: Diagnostic and Protocol Logging
    
-   **Help**: Keyboard Reference, About PlatoLives
    

----------

# Supported Systems

Platform

Interface

Rendering Engine

**macOS 13+**

Cocoa Native Desktop

Metal / Core Animation

**Windows 10/11 (x86_64)**

Win32 Native Desktop

Direct3D 11

**Windows 10/11 (ARM64)**

Win32 Native Desktop

Direct3D 11 (Native AArch64)

**Linux ARM64 & x86_64**

Native Desktop GUI

OpenGL 3.3 Core + SDL2

**Linux Console (ARM64 & x86_64)**

ANSI TrueColor CLI

Headless / Terminal (Static musl)

**Cyber1 / CYBIS**

Supported

Full IST-III & Color Protocol

**IRATA.ONLINE**

Supported

Standard ASCII / PLATO

**MicroTutor Floppy Images**

Local Emulation

Integrated Z80 CPU & Disk Controller

----------

# Building

PlatoLives can be built natively on macOS or cross-compiled for all supported platforms using Zig.

### Build all platforms

With Zig installed:

codeBash

```
brew install zig
./build_all.sh
```

The resulting artifacts are placed in build/:

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

Requirements:

-   macOS 13 or later
    
-   Xcode Command Line Tools
    
-   CMake 3.16 or later
    

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

**Display**

512 × 512 logical PLATO display

**Text**

64 × 32 character matrix

**macOS rendering**

Cocoa / Core Animation / Metal

**Windows rendering**

Win32 / Direct3D 11 (Native x86_64 and ARM64)

**Linux rendering**

OpenGL 3.3 Core / SDL2 / stb_truetype

**Audio**

SDL2 Queue Audio (44.1 kHz, 880 Hz PLATO Bell)

**Render surface**

Up to 2048 × 2048

**Input**

Universal Keyboard Profiles engine, native VKey capture, and PLATO double-tap mapping

**Networking**

Non-blocking TCP with greedy socket drain

**Automation**

State-aware scripting engine and headless runner

**MicroTutor**

Pure C11 Z80 CPU and floppy controller emulation

**Versioning**

Single Source of Truth via root VERSION file

----------

# License

PlatoLives is released under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International license.  
See LICENSE for the complete license text.
