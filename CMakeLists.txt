cmake_minimum_required(VERSION 3.13)

# --- Target board: must be set BEFORE the SDK / project() is initialised ---
set(PICO_BOARD pimoroni_picosystem CACHE STRING "Target board")

# --- Pull in the Raspberry Pi Pico SDK (must come before project()) ---
# Honours the PICO_SDK_PATH env var (the chess clock reuses the SDKs vendored in
# pomodoro_timer/third_party — see SPEC §11), otherwise a local third_party/pico-sdk.
if(NOT DEFINED ENV{PICO_SDK_PATH} AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/third_party/pico-sdk")
    set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/third_party/pico-sdk")
endif()
include(pico_sdk_import.cmake)

project(chess_clock C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise the SDK (toolchain, board config, stage-2 bootloader, flash size...).
pico_sdk_init()

# --- Locate the picosystem library ---
# picosystem ships a CONFIG-mode package (picosystem-config.cmake), found via
# PICOSYSTEM_DIR / CMAKE_PREFIX_PATH. SPEC §11 points the PICOSYSTEM_DIR env var at
# pomodoro_timer/third_party; promote it to a CMake var so find_package() uses it
# reliably across CMake versions. The EXISTS block covers a local vendored copy.
if(NOT PICOSYSTEM_DIR AND DEFINED ENV{PICOSYSTEM_DIR})
    set(PICOSYSTEM_DIR "$ENV{PICOSYSTEM_DIR}")
endif()
if(NOT PICOSYSTEM_DIR AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/third_party/picosystem/picosystem-config.cmake")
    set(PICOSYSTEM_DIR "${CMAKE_CURRENT_LIST_DIR}/third_party/picosystem")
endif()
find_package(PICOSYSTEM CONFIG REQUIRED)

# --- Build the app from every .cpp in src/ ---
file(GLOB CHESS_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp")
picosystem_executable(chess_clock ${CHESS_SOURCES})

# storage.cpp persists settings to the last flash sector via the Pico SDK flash + sync
# APIs; link them explicitly in case the picosystem runtime doesn't.
target_link_libraries(chess_clock hardware_flash hardware_sync)

# --- Launcher app-SDK: slot-ready from day one (SPEC §2/§11) ---
# launcher_app(chess_clock)        -> standalone image @ 0x10000000 (test/flash now)
# launcher_app(chess_clock SLOT n) -> one-flag switch to launcher slot n
if(NOT LAUNCHER_SDK)
    set(LAUNCHER_SDK "${CMAKE_CURRENT_LIST_DIR}/../picosystem_launcher/launcher_sdk")
endif()
include(${LAUNCHER_SDK}/app_slot.cmake)
if (DEFINED APP_SLOT)
    launcher_app(chess_clock SLOT ${APP_SLOT})  # chain-loaded by the launcher
else()
    launcher_app(chess_clock)                    # standalone @ 0x10000000
endif()

# --- Optional picosystem build flags (uncomment as needed) ---
# pixel_double(chess_clock)         # render 120x120 doubled to 240x240 (faster, chunkier)
# disable_startup_logo(chess_clock) # skip the Pimoroni splash
# no_spritesheet(chess_clock)       # drop the built-in spritesheet to save flash
# pico_enable_stdio_usb(chess_clock 1)  # USB-serial debug output
