# PlatoLives (v4.3)

[![Platform](https://img.shields.io/badge/platform-macOS%2013%2B%20%7C%20Windows%2010%2F11%20%7C%20Linux%20ARM64%20%26%20x86__64-orange.svg)](#)
[![Language](https://img.shields.io/badge/language-C%20%7C%20Direct3D%2011%20%7C%20Cocoa%2FMetal-blue.svg)](#)
[![License](https://img.shields.io/badge/license-CC%20BY--NC--SA%204.0-red.svg)](LICENSE)
[![Standard](https://img.shields.io/badge/standard-CDC%20IST--III%20%2F%20CERL%20X--20%20%2F%20CDC%20721-brightgreen.svg)](#)

<img width="3658" height="1964" alt="PlatoLives Banner" src="https://github.com/user-attachments/assets/8b8e7e8c-1219-4be6-82f9-b20b1591509c" />

**PlatoLives** is a modern native client for **PLATO**, the legendary computer-based education and social system that originated at the University of Illinois.

It connects to PLATO systems such as **Cyber1/CYBIS** and **IRATA.ONLINE**, while trying to preserve something that is often lost in modern PLATO clients: the feeling of actually sitting in front of a PLATO terminal.

The display can be rendered as either the original-style **orange gas-plasma display** or as a **color CRT**, complete with phosphor persistence, beam characteristics, scanlines and subtle glass curvature. The result is not simply a terminal window displaying PLATO graphics, but an attempt to recreate the character of the hardware that originally displayed them.

At the same time, PlatoLives is very much a modern client. It runs natively on **macOS, Windows and Linux**, supports copy and paste, scripting, automation, SSH/headless operation and modern Unicode text, and can be used with both traditional PLATO services and private nodes.

Version 4.3 extends the automation side of PlatoLives with a **standalone headless script runner**, allowing complete PLATO scripts to run without the graphical client. Scripts can also capture the current PLATO screen directly to PNG or BMP files, and the complete scripting language reference is now available directly from the application and command line.

📖 **Documentation:** [User and Technical Manual (PDF)](docs/PlatoLives_3.5_User_and_Technical_Manual.pdf)

---

# What makes PlatoLives different?

There are plenty of ways to connect to PLATO. PlatoLives is intended for people who want something closer to **using a PLATO terminal**, rather than simply accessing a PLATO server.

### A real PLATO-style display

PlatoLives has two main display modes.

**Real Plasma** recreates the characteristic orange glow of the original PLATO plasma panels, including the way individual cells illuminate and fade.

**Real Color CRT** recreates the later color PLATO terminals, including RGB phosphors, beam characteristics, persistence and scanlines. It supports the full Cyber1 color mode and can be configured with different CRT profiles.

Both modes can also simulate the curvature of a physical CRT or plasma display.

The intention is not to turn the screen into an exaggerated retro effect. The display should still be perfectly usable; the effects are there to reproduce the visual character of the original terminals.

### Modern hardware, native applications

PlatoLives is a native application rather than a web wrapper or an emulator running inside another environment.

It runs on:

- **macOS 13+**, including Apple Silicon and Intel Macs
- **Windows 10/11**
- **Linux ARM64**
- **Linux x86_64**

The Windows version uses native Win32/Direct3D rendering, while the Mac version uses Cocoa and Metal.

There are also small standalone Linux command-line builds designed for machines such as Raspberry Pi and NAS systems.

### PLATO is more than a screen

PLATO had a distinctive keyboard and interaction model, and modern keyboards do not map naturally onto it.

PlatoLives includes a number of ways to make that interaction practical on today's keyboards. It supports PLATO function keys directly, while its double-tap input system provides alternatives when a particular PLATO key is unavailable.

For example:

- double Return → **SHIFT-NEXT**
- double F4 → **SHIFT-STOP**
- double F8 → **SHIFT-BACK**
- double F1 → **SHIFT-HELP**
- double F2 → **SHIFT-LAB**
- double F3 → **SHIFT-DATA**
- double F5 → **SHIFT-EDIT**

Normal typing remains immediate; the double-tap mechanism only becomes active when the same key is pressed twice within a short interval.

---

# Automation and scripting

One of the biggest additions in PlatoLives is its built-in scripting system.

PLATO sessions often involve repetitive sequences: connecting to a host, entering a user and group, waiting for a prompt, selecting a lesson, navigating menus, or recovering when a connection starts from an unexpected screen.

PlatoLives scripts can interact with the actual PLATO screen rather than simply sending a blind sequence of keystrokes.

A script can:

- look for text on the current screen
- wait for a particular prompt
- use regular expressions when spacing or formatting varies
- send text and PLATO keys
- store variables
- make decisions
- repeat operations with loops
- jump between sections of a script
- implement recovery procedures
- capture the current PLATO screen
- terminate automatically when the desired state is reached

This makes it possible to build scripts that are **state-aware rather than timing-dependent**.

For example, an automatic login script can determine whether the terminal is currently showing the initial banner, asking for a username, asking for a group or asking for a password. If it finds itself in an unexpected state, it can attempt to return to the login screen and try again.

Scripts can be executed interactively from the application or remotely in headless mode.

---

# Manage Scripts

PlatoLives includes a script manager for creating and maintaining scripts without having to edit files manually.

Scripts can be:

- created
- cloned
- deleted
- edited
- assigned keyboard shortcuts
- configured with their own timing parameters

Scripts are stored persistently and can be triggered directly from the **Scripts** menu.

The built-in editor is deliberately simple: it is intended for editing PlatoLives scripts, not for replacing a full programming editor.

---

# Headless and SSH operation

PlatoLives can also be used without its graphical interface.

The console version allows a PLATO session to be run directly from a terminal:

```bash
./PlatoLives --console
```

or connected directly to a host:

```bash
./PlatoLives --c cyberserv.org 8005
```

Version 4.3 adds a standalone script runner that can execute a complete script without starting the graphical client:

```bash
./PlatoLives --script login.plato
```

The `--test-script` option remains available as a compatibility alias.

The script runner can connect to a PLATO host, execute the script, and terminate when the script finishes. This makes it suitable for unattended automation as well as interactive testing.

The scripting reference can also be displayed directly from the command line:

```bash
./PlatoLives --script --help
```

This makes PlatoLives useful over **SSH**, on remote Linux machines, and on systems where no graphical desktop is available.

The console client uses ANSI TrueColor and reproduces the PLATO text display in a terminal window, including its text and semigraphics.

The scripting system can operate in this environment, allowing automated PLATO sessions to run without a display server.

---

# Screenshots from scripts

Scripts can capture the current PLATO display directly to an image file.

For example:

```text
screenshot "screen.png"
```

or:

```text
screenshot "screen.bmp"
```

PNG and BMP output are supported.

The screenshot represents the logical **512 × 512 PLATO display** and follows the currently selected display palette, making it possible to capture automated sessions without requiring the graphical client.

The older `salva-screen` and `schermata` commands are also supported for compatibility.

---

# Linux appliances

The Linux command-line version is intentionally small.

It has no dependency on X11, Wayland, GTK, SDL, ncurses or another graphical framework. The resulting binaries are statically linked and can be deployed on ARM64 or x86_64 Linux systems.

This makes it possible to use PlatoLives on devices such as:

- Raspberry Pi
- NAS systems
- small home servers
- remote Linux machines

A particularly useful combination is a small Linux appliance running a permanent or remotely accessible PLATO connection while the full graphical client remains available on a Mac or Windows PC.

With version 4.3, the same systems can also run complete PLATO automation scripts without starting a graphical session.

---

# Working with PLATO text

PLATO's character system predates Unicode by decades, and some of its graphical characters do not have a straightforward modern equivalent.

PlatoLives translates the common PLATO character set into Unicode where possible, including:

- box drawing
- arrows
- mathematical symbols
- Greek characters
- geometric symbols

It also recognizes downloaded M2/M3 character sets and can identify common box-drawing graphics from their bitmap patterns.

This same text representation is used throughout the application, so PLATO text can be handled consistently by the graphical client, console mode, scripting engine and copy/paste facilities.

---

# Live Text Buffer

PlatoLives includes a separate **Live Text Buffer** window for working with the textual content of the current PLATO screen.

It mirrors the 64 × 32 PLATO text matrix and can be used to select and copy text even while the main terminal continues to run.

When text is selected, the buffer temporarily stops updating so that the selection does not move underneath the mouse.

The window supports:

- Select All
- copying selected text
- copying the complete screen
- compact text copying
- copying the screen image
- pasting text back into PLATO

This is particularly useful for chat sessions, online documentation, MUD-style applications and other PLATO software where the textual content is more important than the pixels themselves.

---

# Copying PLATO screens

The console and scripting systems can extract either the complete PLATO screen or a selected area.

For example:

```text
copy-all
copy-all compact
copy-area 0 0 63 15
copy-area 10 5 40 20 compact
```

Output can be directed to a file, clipboard or standard output.

Screens can also be captured as PNG images. The graphical renderer can produce high-resolution screenshots while retaining the display characteristics of the selected plasma or CRT renderer.

---

# Connection profiles

Multiple connection profiles can be stored and selected from the application.

A profile can contain the information needed to connect to a particular PLATO service and can optionally run a startup script.

This is useful when switching between services such as Cyber1, IRATA.ONLINE or private PLATO systems.

Profiles and scripts are stored persistently in the user's application configuration directory.

---

# PLATO graphics and semigraphics

PlatoLives maintains the original PLATO logical display rather than treating the screen as a conventional modern bitmap.

The underlying PLATO display is a **512 × 512** graphics area with a separate **64 × 32** text matrix.

This matters because much of the software written for PLATO relies on the characteristics of that original display system, including downloadable character sets and combinations of text and graphics.

The renderer then scales that logical display to the modern screen.

---

# macOS integration

The macOS version uses native application features where they are useful without changing the way PLATO itself behaves.

Version 4.3 adds native macOS window tabs:

- **Cmd+T** opens a new tab
- **Cmd+[** and **Cmd+]** switch between tabs
- **Cmd+1** through **Cmd+9** select tabs directly
- the tab bar provides a `+` button for opening a new tab

The standard macOS **Window** menu is also available, including normal window minimization with **Cmd+M**.

The Live Text Buffer shortcut is **Cmd+Shift+B**, avoiding conflicts with the macOS tab system.

---

# Supported systems

PlatoLives currently targets:

| Platform | Support |
| :--- | :--- |
| **macOS** | macOS 13+, Apple Silicon and Intel |
| **Windows** | Windows 10/11, x86_64 |
| **Linux** | ARM64 and x86_64 |
| **Cyber1 / CYBIS** | Supported |
| **IRATA.ONLINE** | Supported |
| **Private PLATO nodes** | Supported |

The client communicates using the protocols used by the supported PLATO systems, including CDC IST-III, CERL X-20 and CDC 721 variants.

The graphical clients provide the same core scripting functionality across macOS and Windows, while the Linux builds provide the console and headless environments.

---

# Scripting Language Reference

The scripting language is designed specifically for interacting with PLATO screens.

It is intentionally small, but supports variables, expressions, conditions, loops, labels, jumps, screen searches, asynchronous waits and screenshots.

The complete reference can be displayed from the command line:

```bash
./PlatoLives --script --help
```

It is also available from the **Scripts → Scripting Reference...** menu in the graphical applications.

## Comments

Lines beginning with `#` are comments.

```text
# This is a comment
```

## Variables

Variables can contain integers or strings.

```text
let username = "myuser"
let group = "mygroup"
let password = "mypassword"
let attempts = 0
let found_pos = -1
```

## Searching the screen

`find_text` performs an immediate search of the current PLATO text screen.

```text
find_text "Press  NEXT  to begin" into banner_x, banner_y
```

Regular expressions can be used when spacing is variable:

```text
find_text regex "Type +your +CYBIS +name" into user_x, user_y
```

`wait_text` waits until the requested text appears:

```text
wait_text regex "Enter +your +password" timeout 10s
```

The default timeout is five seconds.

## Sending text and keys

Text can be sent directly:

```text
send username
```

PLATO keys can be sent with `key`:

```text
key NEXT
key SHIFT-STOP
key BACK
```

Supported keys include:

```text
NEXT
SHIFT-NEXT
BACK
SHIFT-BACK
STOP
SHIFT-STOP
HELP
SHIFT-HELP
LAB
SHIFT-LAB
DATA
SHIFT-DATA
EDIT
SHIFT-EDIT
ANS
TERM
SQUARE
ACCESS
ERASE
TAB
RETURN
ESC
```

## Conditions and branches

Labels are defined with `@`:

```text
@retry:
```

Execution can jump to a label:

```text
goto retry
```

Conditional jumps use expressions:

```text
if login_ok >= 0 goto finished
```

Supported comparison operators are:

```text
==
!=
<
<=
>
>=
```

## Loops

Counted loops are supported:

```text
for attempt = 1 to 5
    key SHIFT-STOP
    wait 300ms
next attempt
```

## Delays

Scripts can wait for a specified amount of time:

```text
wait 2s
wait 300ms
```

## Screenshots

The current PLATO screen can be saved as an image:

```text
screenshot "screen.png"
```

or:

```text
screenshot "screen.bmp"
```

## Terminating a script

```text
exit
```

---

# Example: adaptive Cyber1 login

The following is an example of a state-aware login script.

Instead of assuming that the connection is always at exactly the same point, it looks at the current screen and decides what to do next.

```text
let username = "user"
let group = "group"
let password = "password"

@start:

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

goto reset

@do_next:
key NEXT

@do_user:
wait_text regex "Type +your +CYBIS +name" timeout 5s
send username
key NEXT

@do_group:
wait_text regex "Type +the +name +of +your +CYBIS +group." timeout 5s
send group
key SHIFT-STOP

@do_password:
wait_text regex "Enter +your +password" timeout 5s
send password
key NEXT

wait 2s

let login_ok = -1
find_text regex "Choose +a +lesson" into login_ok, dummy_y

if login_ok < 0 goto reset

exit

@reset:
let r_banner = -1
let r_user = -1

for reset_step = 1 to 10
    key SHIFT-STOP
    wait 400ms

    find_text regex "Type +your +CYBIS +name" into r_user, dummy_y
    if r_user >= 0 goto do_user

    find_text regex "Press +NEXT +to begin" into r_banner, dummy_y
    if r_banner >= 0 goto do_next
next reset_step

exit
```

---

# Building

PlatoLives can be built natively on macOS or cross-compiled for all supported platforms using Zig.

## Build all platforms

With Zig installed:

```bash
brew install zig
```

run:

```bash
./build_all.sh
```

The resulting files are placed in `build/`:

```text
PlatoLives.app
PlatoLives-windows-x86_64.exe
PlatoLives-linux-arm64
PlatoLives-linux-x86_64
```

The macOS build can also be packaged as a DMG for distribution.

## Native macOS build

Requirements:

- macOS 13 or later
- Xcode Command Line Tools
- CMake 3.16 or later

```bash
git clone git@github.com:TheSynthMaster/PlatoLives.git
cd PlatoLives

mkdir build
cd build

cmake ..
make -j$(sysctl -n hw.ncpu)

ctest --output-on-failure
```

The application can then be launched with:

```bash
./PlatoLives.app/Contents/MacOS/PlatoLives
```

---

# Technical details

For those interested in the implementation, PlatoLives is built around a portable C core with native graphics and windowing code on each supported platform.

| Component | Details |
| :--- | :--- |
| **Core** | Portable C |
| **Display** | 512 × 512 logical PLATO display |
| **Text** | 64 × 32 character matrix |
| **macOS rendering** | Cocoa / Core Animation / Metal |
| **Windows rendering** | Win32 / Direct3D 11 |
| **Linux client** | Standalone console application |
| **Render surface** | Up to 2048 × 2048 |
| **Input** | Native keyboard handling plus PLATO key mapping |
| **Networking** | TCP |
| **Automation** | Integrated scripting engine and standalone script runner |
| **Console** | ANSI TrueColor |
| **Linux binaries** | Static ARM64 and x86_64 builds |

The project is deliberately kept lightweight and does not depend on a large cross-platform GUI framework.

---

# License

PlatoLives is released under the **Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International** license.

See [LICENSE](LICENSE) for the complete license text.
