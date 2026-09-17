# PlatoLives (v3.7)

[![Platform](https://img.shields.io/badge/platform-macOS%2013%2B%20%28Apple%20Silicon%20%26%20Intel%29-orange.svg)](#)
[![Language](https://img.shields.io/badge/language-C11%20%7C%20Objective--C%20%28Cocoa%2FMetal%29-blue.svg)](#)
[![License](https://img.shields.io/badge/license-CC%20BY--NC--SA%204.0-red.svg)](LICENSE)
[![Standard](https://img.shields.io/badge/standard-CDC%20IST--III%20%2F%20CERL%20X--20%20%2F%20CDC%20721-brightgreen.svg)](#)

<img width="3658" height="1964" alt="PlatoLives Banner" src="https://github.com/user-attachments/assets/8b8e7e8c-1219-4be6-82f9-b20b1591509c" />

**PlatoLives** is a high-performance, native client for the legendary **PLATO** computer-based education and social system, designed specifically for Apple Silicon (M1/M2/M3/M4) and modern Intel Macs running macOS 13+.

Built from the ground up as a pure C11 core (`libplato`) with a hardware-accelerated Cocoa/Core Animation presentation layer, PlatoLives connects over standard TCP to active CYBIS and Cyber1 mainframes (`cyberserv.org:8005`), IRATA.ONLINE, and private PLATO nodes. It delivers an authentic sub-pixel gas-discharge neon plasma and color CRT experience at 60 FPS with near-zero idle CPU usage.

📖 **Documentation**: Read the official [User and Technical Manual v3.5 (PDF)](docs/PlatoLives_3.5_User_and_Technical_Manual.pdf).

---

## Key Features

### 1. Dual Physical Display Engines (60 FPS)
- **"Real Plasma" Engine**: Simulates the 1972 Owens-Illinois neon-argon gas-discharge flat panel. Features cell ionization profiles, wide-spectrum neon glow with two-stage box blur (local $r=4$ + wide $r=12$), and smooth floating-point exponential decay.
- **"Real Color CRT" Engine (CDC 721 / IST-III)**: High-fidelity analog color CRT emulation supporting full Cyber1 color mode (Subtype 16 handshake).
  - **3-Tap Analog Electron Beam Reconstruction**: Continuous 2D beam profile modeling ($\sigma = 1.8 \dots 2.8$).
  - **Phosphor P22 Color Persistence**: Pure floating-point RGB phosphor decay with selectable durations (20 ms fast P22 up to 5000 ms storage-tube mode) extinguishing seamlessly into pitch black.
  - **Pure Chromatic Shadow Mask**: Modulated RGB sub-pixel triad matrix preserving 100% light efficiency without grid darkening or color crosstalk.
  - **Soft Scanlines**: 512 analog raster lines with natural cathode groove modulation ($18\% - 22\%$).
  - **CRT Beam Profiles**: Three selectable profiles (*Standard/Authentic 13"*, *High/Soft Glow [default]*, *Ultra/Vintage Arcade*).

### 2. Optical Geometric Distortion (Glass Curvature)
- **Calibrated 13" CRT / Tube Geometry**: Physically proportioned curvature ($k = 0.018$) tailored for the central 1:1 PLATO active area inside a 4:3 cathode-ray tube, eliminating distorted fisheye artifacts while providing an authentic glass bezel frame.
- **Selectable Curvature Types**:
  - **Cylindrical [Default]**: Curvature applied exclusively along the horizontal X axis with a flat vertical Y axis, keeping scanlines straight while curving vertical borders.
  - **Barrel (Spherical)**: Organic spherical glass curvature along both X and Y axes.
  - **None (Flat)**: 1:1 rectilinear rendering.
- Available for both **Real Color CRT** and full-screen **Real Plasma** display modes.
- **Integrated Touchscreen Coordinate Mapping**: Touch input automatically compensates for glass curvature, maintaining pixel-perfect accuracy on Cyber1 lessons and keypad games.

### 3. Display Modes
- **Crisp Monochrome**: Pure digital 1-bit amber/black presentation.
- **Real Plasma**: Full analog neon-argon gas discharge panel simulation.
- **Split Monochrome**: Half Real Plasma, half Crisp Monochrome for direct side-by-side comparison.
- **Crisp Color**: Pure digital 32-bit BGRA color display.
- **Real Color CRT**: Full analog shadow mask color monitor simulation.

### 4. Canonical Typography & CDC Appendix G Font Scaling
- **Direct Provenance**: 8×16 font bitmaps for **M0** and **M1** directly sourced and transcribed from Paul Koning's canonical **`pterm`** tables (Jack Stifle's 1972 CERL hardware matrix).
- **M0**: Complete alphanumeric set including the iconic slashed zero.
- **M1**: Full canonical set: Greek alphabet, math operators, and vector symbols.
- **M2 / M3**: Dynamically loaded and compiled character sets from host lessons.
- **CDC Appendix G EXT `ESC R` Scaling**: Continuous dynamic character scaling (`scale = (size_val + 7) / 16`) with automatic multi-size tracking for giant vector titles (e.g. *Asteroids*).

### 5. Native macOS Integration & Performance
- **Zero Idle CPU**: Decoupled 60 FPS render pacing with instant quiescence (0.0% CPU when screen content is static).
- **macOS Status Bar Companion**: Live `NSStatusItem` menu displaying real-time connection status, active session metadata, and quick-connect presets.
- **Multi-Window Architecture**: Run independent simultaneous sessions across different mainframe hosts.
- **Dynamic Connection Profiles (`Cmd+,`)**: Configure display modes, beam profiles, persistence durations, curvature types, and startup presets per host.
- **Mach Performance HUD (`F12`)**: Live FPS, full/partial frame counters, and process thread CPU telemetry.
- **Fine-Grained Touch (FGT)**: Full emulation of the PLATO infrared touch panel matrix.
- **Throttled Async Paste (`Cmd+V`)**: Non-blocking text transmission spaced to match mainframe input pacing.

---

## Technical Specifications

| Parameter | Specification |
| :--- | :--- |
| **Protocol** | CDC IST-III / Jack Stifle CERL X-20 / CDC 721 (ASCII & Color modes) |
| **Logical Matrix** | 512 × 512 1-bit bitboard (32 KB) & 32-bit Little-Endian BGRA matrix |
| **Render Surface** | 2048 × 2048 32-bit sub-pixel BGRA supersampled surface (4x scale) |
| **Core Architecture** | Pure C11 (`libplato`), thread-safe ring buffer, non-blocking BSD sockets |
| **UI Framework** | Native Objective-C (Cocoa / AppKit), Core Animation (`CALayer`) |
| **Distortion Engine** | Analytical coordinate warp ($k=0.018$) with seamless black bezel framing |
| **Persistence Engine** | Floating-point exponential decay ($\tau = \text{duration}/10$) down to $<0.01\%$ |
| **Compatibility** | Cyber1 / CYBIS (`cyberserv.org:8005`), IRATA.ONLINE (`irata.online:8005`) |

---

## Building and Running

### Prerequisites
- macOS 13.0 (Ventura) or newer
- Apple Clang (via Xcode Command Line Tools: `xcode-select --install`)
- CMake >= 3.16

### Compilation
```bash
# Clone the repository
git clone git@github.com:TheSynthMaster/PlatoLives.git
cd PlatoLives

# Build
mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)

# Run unit tests
ctest --output-on-failure

# Launch PlatoLives
./PlatoLives.app/Contents/MacOS/PlatoLives
