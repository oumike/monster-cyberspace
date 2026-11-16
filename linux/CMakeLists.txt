# SPDX-License-Identifier: GPL-3.0-or-later
# The Linux builds of Monster for Cyberspace: the M5Stack CardputerZero
# (MC_BOARD=cp0, aarch64, in linux/docker's container) and the desktop
# simulator (MC_BOARD=sim, SDL2). See
# docs/superpowers/specs/2026-11-16-cyberspace-cardputer-zero-design.md and
# scripts/build-cardputer-zero.sh.
cmake_minimum_required(VERSION 3.25)
project(monster_cyberspace LANGUAGES C CXX)

set(MC_BOARD "sim" CACHE STRING "Linux board: sim (SDL2 simulator) or cp0 (CardputerZero)")
set_property(CACHE MC_BOARD PROPERTY STRINGS sim cp0)
option(MC_FAKE_API "Demo build: scripted fake API, no network (never release)" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

get_filename_component(REPO "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

# Dependencies, pinned exactly as in platformio.ini. SOURCE_SUBDIR points
# nowhere so their own CMake files stay out; the sources are listed below.
include(FetchContent)
FetchContent_Declare(lvgl
  URL https://github.com/lvgl/lvgl/archive/refs/tags/v9.5.0.tar.gz
  URL_HASH SHA256=34a955cdf3a2d005507b704e87357af669a114523b6d3f77b5344fdc68717bc6
  SOURCE_SUBDIR _none)
FetchContent_Declare(arduinojson
  URL https://github.com/bblanchon/ArduinoJson/archive/refs/tags/v7.4.3.tar.gz
  URL_HASH SHA256=a2a8a277fc3cd0404499199bf1468682ea3688f354be0eaf6af8648366911257
  SOURCE_SUBDIR _none)
# QRCode 0.0.1 is the repository's only release, at this commit.
FetchContent_Declare(qrcode
  URL https://github.com/ricmoo/QRCode/archive/eafbde494979abc2445c363cc2602230bcbe299c.tar.gz
  URL_HASH SHA256=ed85acf3ea44af275139fb78e580e2918d2c265d0a78f0034e05b89625c69621
  SOURCE_SUBDIR _none)
FetchContent_MakeAvailable(lvgl arduinojson qrcode)

# BOARD_HAS_PSRAM: plenty of RAM, so the PSRAM paths (a whole frame, the full
# LVGL, two screen trees) apply. lv_conf.h takes LVGL's allocator from libc
# for RC_LINUX.
set(MC_DEFS RC_LINUX=1 BOARD_HAS_PSRAM=1 LV_CONF_INCLUDE_SIMPLE)
if(MC_BOARD STREQUAL "sim")
  list(APPEND MC_DEFS BOARD_LINUX_SIM=1)
elseif(MC_BOARD STREQUAL "cp0")
  list(APPEND MC_DEFS BOARD_CARDPUTER_ZERO=1)
else()
  message(FATAL_ERROR "MC_BOARD must be sim or cp0, not '${MC_BOARD}'")
endif()
if(MC_FAKE_API)
  list(APPEND MC_DEFS RC_FAKE_API=1)
endif()

# LVGL as its own library: its C sources, configured by lib/lvui/conf.
file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS "${lvgl_SOURCE_DIR}/src/*.c")
add_library(lvgl STATIC ${LVGL_SOURCES})
target_include_directories(lvgl PUBLIC "${lvgl_SOURCE_DIR}" "${REPO}/lib/lvui/conf")
target_compile_definitions(lvgl PUBLIC ${MC_DEFS})

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS "${REPO}/lib/core/src/*.cpp")
file(GLOB_RECURSE LVUI_SOURCES CONFIGURE_DEPENDS "${REPO}/lib/lvui/src/*.cpp")

set(HAL_SOURCES
  "${REPO}/src/hal/board_defaults.cpp"
  "${REPO}/src/hal/cardputer_zero/flags_cardputer_zero.cpp")
if(MC_BOARD STREQUAL "sim")
  list(APPEND HAL_SOURCES "${REPO}/src/hal/linux_sim/board_linux_sim.cpp")
else()
  list(APPEND HAL_SOURCES "${REPO}/src/hal/cardputer_zero/board_cardputer_zero.cpp")
endif()

# One executable from object files, not archives, so the HAL's strong
# definitions win over board_defaults.cpp's weak ones.
add_executable(monster-cyberspace
  ${CORE_SOURCES}
  ${LVUI_SOURCES}
  ${HAL_SOURCES}
  "${qrcode_SOURCE_DIR}/src/qrcode.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/replace/main.cpp")
target_include_directories(monster-cyberspace PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/replace"
  "${REPO}/src"
  "${REPO}/src/ui/fonts"
  "${REPO}/lib/core/src"
  "${REPO}/lib/lvui/src"
  "${arduinojson_SOURCE_DIR}/src"
  "${qrcode_SOURCE_DIR}/src")
target_link_libraries(monster-cyberspace PRIVATE lvgl)
find_package(Threads REQUIRED)
target_link_libraries(monster-cyberspace PRIVATE Threads::Threads)

if(MC_BOARD STREQUAL "sim")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
  target_link_libraries(monster-cyberspace PRIVATE PkgConfig::SDL2)
endif()
