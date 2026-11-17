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

# The version, from the VERSION file as the firmware's (tools/set_app_version.py):
# "v2.3.2+abc1234" builds as RC_VERSION "2.3.2+abc1234"; release files are
# named by the tag without the +build part, as release.sh names the others.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${REPO}/VERSION")
set(MC_VERSION "")
if(EXISTS "${REPO}/VERSION")
  file(READ "${REPO}/VERSION" MC_VERSION)
  string(STRIP "${MC_VERSION}" MC_VERSION)
  string(REGEX REPLACE "^v" "" MC_VERSION "${MC_VERSION}")
endif()
if(MC_VERSION)
  add_compile_definitions(RC_VERSION="${MC_VERSION}")
else()
  set(MC_VERSION "0.1.0")
endif()
string(REGEX REPLACE "\\+.*$" "" MC_FILE_TAG "v${MC_VERSION}")

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
# No OTA (the launcher's packages update it) and no Wi-Fi management (Linux
# owns Wi-Fi); every other feature is on. Web config is an opt-in on port
# 8765 that needs a page password (spec section 7).
set(MC_DEFS RC_LINUX=1 BOARD_HAS_PSRAM=1 LV_CONF_INCLUDE_SIMPLE RC_FEAT_OTA=0 RC_FEAT_WIFI_MGMT=0
  RC_WEB_OPT_IN=1)
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

# The Arduino, FreeRTOS and ESP-IDF APIs src/ uses, over POSIX
# (spec section 4.1).
add_library(mc_compat STATIC
  compat/arduino_compat.cpp
  compat/freertos_compat.cpp
  compat/preferences.cpp
  compat/fs_compat.cpp
  compat/httpd_compat.cpp)
target_include_directories(mc_compat PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/compat")
target_compile_definitions(mc_compat PUBLIC ${MC_DEFS})
find_package(Threads REQUIRED)
target_link_libraries(mc_compat PUBLIC Threads::Threads)

set(HAL_SOURCES
  "${REPO}/src/hal/board_defaults.cpp"
  "${REPO}/src/hal/cardputer_zero/flags_cardputer_zero.cpp")
if(MC_BOARD STREQUAL "sim")
  list(APPEND HAL_SOURCES "${REPO}/src/hal/linux_sim/board_linux_sim.cpp")
else()
  foreach(f board fb input power audio)
    list(APPEND HAL_SOURCES "${REPO}/src/hal/cardputer_zero/${f}_cardputer_zero.cpp")
  endforeach()
endif()

# The Linux versions of src/ files that talk to ESP-IDF (spec section 4.2),
# built instead of them, and the portable src/ files they need.
set(REPLACE_SOURCES
  replace/net/https_client.cpp
  replace/net/sse_client.cpp
  replace/net/tls_alloc.cpp
  replace/net/wifi_mgr.cpp
  replace/app/time_service.cpp
  replace/app/restart.cpp
  replace/store/nvs_migrate.cpp
  replace/store/mascot_store.cpp
  replace/sys/mem.cpp)
find_package(CURL REQUIRED)

# The rest of src/: everything but the files replaced above, main.cpp (the
# replacement has main()), the X4's grid UI, other boards' HALs and OTA. The
# power service is built but idle: sleepsOnBattery() is false.
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS "${REPO}/src/*.cpp")
list(FILTER APP_SOURCES EXCLUDE REGEX "/src/(grid_ui|hal)/")
foreach(r ${REPLACE_SOURCES})
  string(REGEX REPLACE "^replace/" "${REPO}/src/" orig "${r}")
  list(REMOVE_ITEM APP_SOURCES "${orig}")
endforeach()
list(REMOVE_ITEM APP_SOURCES
  "${REPO}/src/main.cpp"
  "${REPO}/src/app/ota_service.cpp")

# Spell check's dictionary, generated from dict/en.txt as the PlatformIO
# builds do (tools/gen_spell_dict.py).
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_command(
  OUTPUT "${REPO}/src/spell_dict_en.h"
  COMMAND Python3::Interpreter "${REPO}/tools/gen_spell_dict.py"
  DEPENDS "${REPO}/dict/en.txt" "${REPO}/tools/gen_spell_dict.py"
  COMMENT "Generating src/spell_dict_en.h")
add_custom_target(spell_dict DEPENDS "${REPO}/src/spell_dict_en.h")

# Web config's Fluffy presets, from site/mascots.json as the PlatformIO
# builds do (tools/gen_mascot_presets.py).
add_custom_command(
  OUTPUT "${REPO}/src/web/mascot_presets.h"
  COMMAND Python3::Interpreter "${REPO}/tools/gen_mascot_presets.py"
  DEPENDS "${REPO}/site/mascots.json" "${REPO}/tools/gen_mascot_presets.py"
  COMMENT "Generating src/web/mascot_presets.h")
add_custom_target(mascot_presets DEPENDS "${REPO}/src/web/mascot_presets.h")

set(MC_INCLUDES
  "${CMAKE_CURRENT_SOURCE_DIR}/replace"
  "${REPO}/src"
  "${REPO}/src/ui/fonts"
  "${REPO}/lib/core/src"
  "${REPO}/lib/lvui/src"
  "${arduinojson_SOURCE_DIR}/src"
  "${qrcode_SOURCE_DIR}/src")

# lib/core once, for the app and the tests below.
add_library(mc_core OBJECT ${CORE_SOURCES} "${qrcode_SOURCE_DIR}/src/qrcode.c")
target_include_directories(mc_core PUBLIC ${MC_INCLUDES})
target_link_libraries(mc_core PUBLIC mc_compat)

# One executable from object files, not archives, so the HAL's strong
# definitions win over board_defaults.cpp's weak ones.
add_executable(monster-cyberspace
  $<TARGET_OBJECTS:mc_core>
  ${LVUI_SOURCES}
  ${HAL_SOURCES}
  ${REPLACE_SOURCES}
  ${APP_SOURCES}
  "${CMAKE_CURRENT_SOURCE_DIR}/replace/main.cpp")
add_dependencies(monster-cyberspace spell_dict mascot_presets)
target_include_directories(monster-cyberspace PRIVATE ${MC_INCLUDES})
target_link_libraries(monster-cyberspace PRIVATE lvgl mc_compat CURL::libcurl)

find_package(PkgConfig REQUIRED)
if(MC_BOARD STREQUAL "sim")
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
  target_link_libraries(monster-cyberspace PRIVATE PkgConfig::SDL2)
else()
  pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa)
  target_link_libraries(monster-cyberspace PRIVATE PkgConfig::ALSA ${CMAKE_DL_LIBS})
endif()
# The HALs are ours: hold them to -Wall -Wextra.
file(GLOB HAL_OWN "${REPO}/src/hal/cardputer_zero/*.cpp" "${REPO}/src/hal/linux_sim/*.cpp")
set_source_files_properties(${HAL_OWN} PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra")

# ctest --preset sim: the compat layer's own tests.
enable_testing()
foreach(t preferences fs rtos httpd)
  add_executable(test_${t} tests/test_${t}.cpp)
  target_link_libraries(test_${t} PRIVATE mc_compat)
  add_test(NAME ${t} COMMAND test_${t})
endforeach()

# The replacements' tests, each with the replacement it covers.
add_executable(test_sse_local tests/test_sse_local.cpp replace/net/sse_client.cpp)
add_executable(test_mascot_file tests/test_mascot_file.cpp replace/store/mascot_store.cpp
  "${REPO}/src/app/queues.cpp")
foreach(t sse_local mascot_file)
  target_link_libraries(test_${t} PRIVATE mc_core CURL::libcurl)
  add_test(NAME ${t} COMMAND test_${t})
endforeach()

# The CardputerZero package (spec section 9): cpack -G DEB in build/cp0 gives
# monster-cyberspace-cardputer-zero-<tag>_arm64.deb, laid out for APPLaunch.
# Its watcher sees the .desktop come and go within 3 s, so no maintainer
# scripts.
if(MC_BOARD STREQUAL "cp0")
  install(TARGETS monster-cyberspace RUNTIME DESTINATION share/APPLaunch/bin)
  install(FILES packaging/monster-cyberspace.desktop DESTINATION share/APPLaunch/applications)
  install(FILES packaging/icon.png DESTINATION share/APPLaunch/share/images
    RENAME monster-cyberspace.png)
  install(FILES "${REPO}/LICENSE" DESTINATION share/doc/monster-cyberspace RENAME copyright)

  set(CPACK_GENERATOR DEB)
  set(CPACK_PACKAGE_NAME monster-cyberspace)
  set(CPACK_PACKAGE_VERSION "${MC_VERSION}")
  set(CPACK_PACKAGE_FILE_NAME "monster-cyberspace-cardputer-zero-${MC_FILE_TAG}_arm64")
  set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
  # The store matches this email to the publisher's GitHub account.
  set(CPACK_PACKAGE_CONTACT "oumike <michael.cojocari@gmail.com>")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Monster for Cyberspace: a cyberspace.online client")
  set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
    "A client for cyberspace.online, the text-only social network: cIRC rooms,\nC-Mail, the Feed, Guilds, People, Notes and more, for the M5Stack\nCardputerZero's launcher.")
  set(CPACK_DEBIAN_PACKAGE_SECTION APPLaunch)
  set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE arm64)
  set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/oumike/monster-cyberspace")
  # Trixie renamed both for the 64-bit time_t move; older names as fallbacks.
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libcurl4t64 | libcurl4, libasound2t64 | libasound2")
  set(CPACK_STRIP_FILES ON)
  include(CPack)
endif()
