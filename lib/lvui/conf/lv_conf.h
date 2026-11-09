// LVGL 9.5 configuration for Monster for Cyberspace. Found through
// -DLV_CONF_INCLUDE_SIMPLE -Ilib/lvui/conf; anything not set here takes
// lv_conf_internal.h's default. Included from C as well as C++.
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16

// Lean: boards without PSRAM, where LVGL shares the internal heap with TLS,
// leave out what lvui never uses. PSRAM boards and the host tests keep the
// full set; -DRC_LV_LEAN=1 runs the host tests lean.
#ifndef RC_LV_LEAN
#if defined(BOARD_HAS_PSRAM) || defined(RC_NATIVE)
#define RC_LV_LEAN 0
#else
#define RC_LV_LEAN 1
#endif
#endif

// Host tests use the C library; on the ESP32 lv_init.cpp supplies
// lv_malloc_core and friends (PSRAM, or a capped internal heap).
#ifdef RC_NATIVE
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#else
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CUSTOM
#endif
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_BUILTIN

// Only ui_task's thread calls LVGL.
#define LV_USE_OS LV_OS_NONE

#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_STYLE 0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ 0

// Lean draws no rounded corners: one circle-mask cache entry, not four.
#if RC_LV_LEAN
#define LV_DRAW_SW_CIRCLE_CACHE_SIZE 1
#endif
// A failed assert reports where and aborts (the device reboots), not hangs.
#define LV_ASSERT_HANDLER_INCLUDE "lv_assert_hook.h"
#define LV_ASSERT_HANDLER lvui_assert_failed(__FILE__, __LINE__);

// Fonts: only the Terminus cells, through the adapter (lvui/cell_font.h).
#define LV_FONT_MONTSERRAT_8 0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 0
#define LV_FONT_MONTSERRAT_14 0
#define LV_FONT_MONTSERRAT_16 0
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 0
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0
#define LV_FONT_SOURCE_HAN_SANS_SC_14_CJK 0
#define LV_FONT_SOURCE_HAN_SANS_SC_16_CJK 0
#define LV_FONT_UNSCII_8 0
#define LV_FONT_UNSCII_16 0
#define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(lv_font_cell8x16)
#define LV_FONT_DEFAULT &lv_font_cell8x16

// Widgets: only what the screens use.
#define LV_USE_ANIMIMG 0
#define LV_USE_ARC 0
#define LV_USE_ARCLABEL 0
#define LV_USE_BAR 1
#define LV_USE_BUTTON 1
#define LV_USE_BUTTONMATRIX 1
#define LV_USE_CALENDAR 0
#define LV_USE_CANVAS 1
#define LV_USE_CHART 0
#define LV_USE_CHECKBOX 0
#define LV_USE_DROPDOWN 0
#define LV_USE_IMAGE 1
#define LV_USE_IMAGEBUTTON 0
#define LV_USE_KEYBOARD 1
#define LV_USE_LABEL 1
// Lean: no label keeps selection bounds or a long-text hint (no view selects
// text; the hint only speeds up labels over 1024 px tall), 20 bytes less a
// label.
#define LV_LABEL_TEXT_SELECTION (!RC_LV_LEAN)
#define LV_LABEL_LONG_TXT_HINT (!RC_LV_LEAN)
#define LV_USE_LED 0
#define LV_USE_LINE 0
#define LV_USE_LIST 0
#define LV_USE_LOTTIE 0
#define LV_USE_MENU 0
#define LV_USE_MSGBOX 0
#define LV_USE_ROLLER 0
#define LV_USE_SCALE 0
#define LV_USE_SLIDER 1
// No view uses spans; lean boards skip the snippet stack lv_init allocates.
#define LV_USE_SPAN (!RC_LV_LEAN)
#define LV_USE_SPINBOX 0
#define LV_USE_SPINNER 0
#define LV_USE_SWITCH 0
#define LV_USE_TABLE 0
#define LV_USE_TABVIEW 0
#define LV_USE_TEXTAREA 1  // lv_keyboard needs it
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0

// The default theme only styles objects lvui strips again (plainBox,
// lineLabel, the keyboard's matrix) and the screen behind the trees, which
// display.cpp gives the same look without it (lvui::displayBegin). Lean
// boards leave it out for the internal RAM its styles take.
#ifndef RC_LV_THEME
#define RC_LV_THEME (!RC_LV_LEAN)
#endif
#define LV_USE_THEME_DEFAULT RC_LV_THEME
#define LV_USE_THEME_SIMPLE 0
#define LV_USE_THEME_MONO 0
#define LV_USE_FLEX 1
#define LV_USE_GRID 0

#define LV_USE_SNAPSHOT 1
#define LV_USE_OBSERVER 0
#define LV_USE_SYSMON 0
#define LV_USE_PROFILER 0

#define LV_BUILD_EXAMPLES 0
#define LV_BUILD_DEMOS 0

#endif
