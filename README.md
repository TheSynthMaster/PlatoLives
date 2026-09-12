# PlatoLives (v3.2)

[![Platform](https://img.shields.io/badge/platform-macOS%2013%2B%20%28Apple%20Silicon%20%26%20Intel%29-orange.svg)](#)
[![Language](https://img.shields.io/badge/language-C11%20%7C%20Objective--C%20%28Cocoa%2FMetal%29-blue.svg)](#)
[![License](https://img.shields.io/badge/license-CC%20BY--NC--SA%204.0-red.svg)](LICENSE)
[![Standard](https://img.shields.io/badge/standard-CDC%20IST--III%20%2F%20CERL%20X--20-brightgreen.svg)](#)

**PlatoLives** is a high-performance, native client for the legendary **PLATO** computer-based education and social system, designed specifically for Apple Silicon (M1/M2/M3/M4) and modern Intel Macs running macOS 13+.

Built from the ground up as a pure C11 core with a hardware-accelerated Cocoa/Core Animation/Metal presentation layer, PlatoLives connects over standard TCP to active CYBIS and Cyber1 mainframes (`cyberserv.org:8005`), IRATA.ONLINE, and private PLATO nodes. It delivers an authentic, sub-pixel gas-discharge plasma display experience while maintaining 60 FPS smooth rendering and near-zero idle CPU usage.

---

## Key Features

### 1. "Real Plasma" Sub-Pixel Rendering & Glow (60 FPS)
- **4x Supersampling Engine**: Internal 2048×2048 bitmap rendering pipeline mapped to the classic PLATO 512×512 matrix.
- **Hardware Acceleration**: Zero-copy little-endian BGRA GPU transfer (`kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst`) on Apple Metal layers.
- **L1 Cache Optimized Blurring**: Sliding-window Box Blur with a 512-byte scanline accumulator residing entirely inside CPU L1 cache.
- **Dynamic Plasma Decay (Persistence)**: Configurable physical neon-argon glow decay from authentic 100 ms up to 5000 ms storage-tube mode.

### 2. Authentic Jack Stifle CERL 1972 Typography (M0 / M1)
- Reconstructed pixel-for-pixel from the 1972 University of Illinois CERL hardware specifications (8-column vertical 16-bit matrices mapped into 16×8 horizontal raster).
- **M0**: Authentic alphanumeric set including the iconic slashed zero.
- **M1**: Complete canonical character set: lowercase Greek (alpha, beta, delta, lambda, mu, pi, rho, sigma, omega), uppercase Greek (Sigma, Delta, Theta), math operators, and vector symbols.
- Programmable **M2 / M3** character sets dynamically loaded from host lessons.

### 3. Native macOS Integration
- **macOS Status Bar Companion**: Live menu bar item (`NSStatusItem`) providing real-time mainframe connection status, active session slot metadata, and quick connect shortcuts.
- **Multi-Window Architecture**: Independent terminal controllers; run simultaneous sessions to different hosts or multiple terminals on Cyber1.
- **Dynamic Profiles Manager**: Manage connection settings, display modes, and persistent full-screen preferences on the fly.
- **Accurate Mach Performance HUD**: Real-time FPS and Mach kernel thread CPU usage display (F12).
- **Fine Grained Touch (FGT)**: Full emulation of the PLATO infrared touch panel matrix.
- **Async Paste Queue**: Non-blocking character transmission spaced for CDC mainframe pacing.

---

## Technical Specifications

| Parameter | Specification |
| :--- | :--- |
| **Protocol** | CDC IST-III / Jack Stifle CERL X-20 (ASCII mode) |
| **Internal Matrix** | 512 × 512 monochromatic plasma cell bitboard (32 KB) |
| **Render Matrix** | 2048 × 2048 32-bit sub-pixel BGRA supersampled surface |
| **Core Architecture** | Pure C11 (`libplato`), thread-safe ring buffer, non-blocking BSD sockets |
| **UI Framework** | Native Objective-C (Cocoa / AppKit), Core Animation (`CALayer`), Metal-aligned memory |
| **Compatibility** | Cyber1 / CYBIS (`cyberserv.org:8005`), IRATA.ONLINE (`irata.online:8005`) |

---

## Building and Running

### Prerequisites
- macOS 13.0 (Ventura) or newer
- Apple Clang (via Xcode or Command Line Tools: `xcode-select --install`)
- CMake >= 3.16

### Compilation
```bash
# Clone the repository
git clone git@github.com:TheSynthMaster/PlatoLives.git
cd PlatoLives

# Build
mkdir build && cd build
cmake ..
make

# Run unit tests
ctest --output-on-failure

# Launch PlatoLives
./PlatoLives.app/Contents/MacOS/PlatoLives
```

---

## Keyboard Mapping

PlatoLives maps the classic PLATO keys to modern macOS keyboards:

| PLATO Key | Primary macOS Shortcut | Alternate Shortcut | Function / Game Usage |
| :--- | :--- | :--- | :--- |
| **NEXT** | `Return` / `Enter` | — | Proceed / confirm input |
| **BACK** | `Delete` (Backspace) | — | Return to previous page |
| **HELP** | `F1` | `Ctrl + H` | Lesson help / hints |
| **LAB** | `F2` | `Ctrl + L` | Laboratory / alternate help |
| **DATA** | `F3` | `Ctrl + D` | Data view / mode switch |
| **STOP** | `F4` / `F10` | `Ctrl + Shift + S` | Exit lesson / return to menu |
| **EDIT** | `F5` | `Ctrl + E` | Edit buffer |
| **ANS** | `Ctrl + A` | `Option + A` / `Ctrl + /` | Answer check |
| **TERM** | `Ctrl + T` | `Option + T` | Terminal control prompt |
| **MICRO** | `Ctrl + M` | `Option + M` | Special character prefix |
| **FONT** | `Ctrl + F` | `Option + F` | Font selector |
| **SUPER** | `Up Arrow` / `Page Up` | `Ctrl + P` | Superscript / Dungeon climb |
| **SHIFT-SUPER**| `Shift + Up Arrow` | `Shift + Page Up` | Rapid climb |
| **SUB** | `Down Arrow` / `Page Down` | `Ctrl + Y` | Subscript / Dungeon descend |
| **SHIFT-SUB** | `Shift + Down Arrow`| `Shift + Page Down` | Rapid descent |
| **SQUARE** | `Ctrl + Q` | `Option + Q` | Geometric block character |
| **ACCESS** | `Shift + Ctrl + Q` | `Shift + Option + Q` | File / System access |

---

## Clean Room Development & Ethics

PlatoLives is developed strictly under **Clean Room Reverse Engineering** practices. 
- No source code, macro libraries, or proprietary binaries from historical emulators (including `pterm` or DGI IST-III firmware) have been copied or translated.
- Specifications and protocol values are derived purely from public technical literature, RFC-style protocol specifications, and interoperability testing against live public hosts.
- The project is an independent homage to the University of Illinois Computer-based Education Research Laboratory (CERL) and the brilliant work of Donald Bitzer, Gene Slottow, and Jack Stifle.

---

## License

PlatoLives is released under the **Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License (CC BY-NC-SA 4.0)**.

- **Non-Commercial**: You may **not** sell, monetize, charge for access, or include advertisements/promotional telemetry in this software or any derivative works.
- **Attribution**: Any derivative work or distribution must clearly credit **Fabio Montarsolo** as the original author and link to the official repository.
- **Share-Alike**: All derivative works must be distributed under the exact same license.

See the [LICENSE](LICENSE) file for the full legal text.

---

## Author & Acknowledgements

- **Created by**: Fabio Montarsolo (`TheSynthMaster`) — `fabio.montarsolo@gmail.com`
- Dedicated to the members and keepers of **Cyber1.org**, preserving the legacy of PLATO for future generations.
- Inspired by Brian Dear's definitive chronicle: *"The Friendly Orange Glow: The Untold Story of the PLATO System and the Dawn of Cyberculture"* (Pantheon Books, 2017) [1].