cmake_minimum_required(VERSION 3.56)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum supported macOS version" FORCE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures" FORCE)
else()
    # Native Linux / Docker / QNAP ARM64 CLI build
    add_executable(PlatoLives
        src/console_main.c
        src/console_runner.c
    )

    target_compile_options(
        PlatoLives
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -O2
    )

    target_link_libraries(
        PlatoLives
        plato
        pthread
    )
endif()


file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" PLATO_VERSION_RAW LIMIT_COUNT 1)
string(STRIP "${PLATO_VERSION_RAW}" PLATO_PROJECT_VERSION)
project(PlatoLives VERSION ${PLATO_PROJECT_VERSION} LANGUAGES C OBJC)

# Sincronizzazione automatica SSOT: genera include/plato/plato_version.h da VERSION
file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/include/plato/plato_version.h"
"/* Single Source of Truth - PlatoLives Version Header */
/* Automatically generated from VERSION file - DO NOT EDIT MANUALLY */
#ifndef PLATO_VERSION_H
#define PLATO_VERSION_H

#define PLATO_VERSION_STRING \"${PLATO_PROJECT_VERSION}\"
#define PLATO_VERSION_WSTRING L\"${PLATO_PROJECT_VERSION}\"

#endif /* PLATO_VERSION_H */
")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_C_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter -O2)
endif()

include_directories(include include/plato)

# Core library: libplato, pure C11
set(LIBPLATO_SOURCES
    src/plato_framebuffer.c
    src/plato_font.c
    src/plato_tek_font.c
    src/plato_graphics.c
    src/plato_protocol.c
    src/plato_keyboard.c
    src/plato_keymap.c
    src/plato_profile.c
    src/plato_script.c
    src/plato_optical.c
    
    src/plato_microtutor.c
    src/z80emu.c
    src/plato_terminal.c
    src/plato_ringbuf.c
    src/plato_transport.c
)

add_library(plato STATIC ${LIBPLATO_SOURCES})
target_include_directories(plato PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Unit test suite
enable_testing()

add_executable(test_graphics tests/test_graphics.c)
target_link_libraries(test_graphics plato)
add_test(NAME test_graphics COMMAND test_graphics)

add_executable(test_protocol tests/test_protocol.c)
target_link_libraries(test_protocol plato)
add_test(NAME test_protocol COMMAND test_protocol)

add_executable(test_terminal tests/test_terminal.c)
target_link_libraries(test_terminal plato)
add_test(NAME test_terminal COMMAND test_terminal)

# Native macOS application
if(APPLE)
    set(MACOS_SOURCES
        platolives-mac/main.m
        platolives-mac/mac_view.m
        platolives-mac/mac_window.m
        platolives-mac/mac_scripts.m
        platolives-mac/mac_keymap_window.m
        platolives-mac/mac_conn.m
        platolives-mac/test_runner.m
        src/console_runner.c
    )

    set(MACOS_RESOURCES
        platolives-mac/PlatoLives.icns
    )
    set_source_files_properties(${MACOS_RESOURCES} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    add_executable(PlatoLives ${MACOS_SOURCES} ${MACOS_RESOURCES})

    target_compile_options(
        PlatoLives
        PRIVATE
        -fobjc-arc
        -Wno-gnu-statement-expression-from-macro-expansion
    )

    target_link_libraries(
        PlatoLives
        plato
        "-framework Cocoa"
        "-framework QuartzCore"
    )

    set_target_properties(
        PlatoLives
        PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "PlatoLives"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.fabiomontarsolo.platolives"
        MACOSX_BUNDLE_ICON_FILE "PlatoLives.icns"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        MACOSX_BUNDLE_COPYRIGHT "Copyright (c) 2026 Fabio Montarsolo"
    )
endif()


add_executable(test_script tests/test_script.c)
target_link_libraries(test_script plato)
add_test(NAME test_script COMMAND test_script)

add_executable(test_microtutor tests/test_microtutor.c)
target_link_libraries(test_microtutor plato)
add_test(NAME test_microtutor COMMAND test_microtutor)

# --- Target Linux GUI (SDL2 + OpenGL 3.3) - Solo per Linux Nativo ---
if(NOT APPLE AND NOT WIN32)
    find_package(SDL2 QUIET)
    find_package(OpenGL QUIET)

    if(SDL2_FOUND AND OPENGL_FOUND)
        message(STATUS "Configurazione target GUI SDL2/OpenGL (PlatoLives-linux)...")
        add_executable(PlatoLives-linux
            platolives-linux/linux_main.c
            platolives-linux/linux_menu.c
            platolives-linux/linux_gl.c
            platolives-linux/linux_keyref.c
            platolives-linux/linux_profiles.c
            platolives-linux/linux_scriptref.c
            platolives-linux/linux_scripts.c
            platolives-linux/linux_keymap.c
            platolives-linux/linux_livetext.c
            platolives-linux/linux_about.c
            platolives-linux/linux_hud.c
            platolives-linux/linux_audio.c
            platolives-linux/stb_impl.c
        )
        target_include_directories(PlatoLives-linux PRIVATE
            include
            ${SDL2_INCLUDE_DIRS}
            ${OPENGL_INCLUDE_DIR}
        )
        target_link_libraries(PlatoLives-linux PRIVATE
            plato
            ${SDL2_LIBRARIES}
            ${OPENGL_LIBRARIES}
            m
            pthread
        )
    endif()
endif()

add_executable(test_keymap tests/test_keymap.c)
target_link_libraries(test_keymap plato)
add_test(NAME test_keymap COMMAND test_keymap)
