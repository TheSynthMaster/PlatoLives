cmake_minimum_required(VERSION 3.56)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum supported macOS version" FORCE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures" FORCE)
endif()

project(PlatoLives VERSION 3.5 LANGUAGES C OBJC)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_C_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter -O2)
endif()

include_directories(include)

# Core library: libplato, pure C11
set(LIBPLATO_SOURCES
    src/plato_framebuffer.c
    src/plato_font.c
    src/plato_graphics.c
    src/plato_protocol.c
    src/plato_keyboard.c
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
        platolives-mac/test_runner.m
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
