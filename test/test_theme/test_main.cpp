#include <unity.h>

#include <string>

#include "ui/nick_color.h"
#include "ui/theme.h"

using namespace rc;

void setUp() {}
void tearDown() {}

namespace {
const ThemeId kAll[] = {ThemeId::Default, ThemeId::BlackWhite, ThemeId::WhiteBlack, ThemeId::Dark,
                        ThemeId::Light,   ThemeId::Lcd,        ThemeId::C64,        ThemeId::Vt320,
                        ThemeId::Matrix,  ThemeId::Poetry,     ThemeId::Brutalist,  ThemeId::Grid,
                        ThemeId::Crypt,   ThemeId::Bubblegum,  ThemeId::Top8};

// Pairs that are drawn on top of each other must never match.
void assertReadable(const Theme& t) {
  TEST_ASSERT_NOT_EQUAL(t.bg, t.fg);
  TEST_ASSERT_NOT_EQUAL(t.bg, t.dim);
  TEST_ASSERT_NOT_EQUAL(t.bg, t.time);
  TEST_ASSERT_NOT_EQUAL(t.bg, t.sys);
  TEST_ASSERT_NOT_EQUAL(t.bg, t.selectBg);
  TEST_ASSERT_NOT_EQUAL(t.bg, t.error);
  TEST_ASSERT_NOT_EQUAL(t.barBg, t.barFg);
  TEST_ASSERT_NOT_EQUAL(t.hintBg, t.hintFg);
  TEST_ASSERT_NOT_EQUAL(t.cursorBg, t.cursorFg);
  TEST_ASSERT_NOT_EQUAL(t.selectBg, t.selectFg);
  TEST_ASSERT_NOT_EQUAL(t.mentionBg, t.fg);
  TEST_ASSERT_NOT_EQUAL(t.inputBg, t.fg);
  TEST_ASSERT_NOT_EQUAL(t.drawerBg, t.fg);
  TEST_ASSERT_NOT_EQUAL(t.drawerEdge, t.drawerBg);
  TEST_ASSERT_NOT_EQUAL(t.bg, t.nick);
  TEST_ASSERT_NOT_EQUAL(t.bg, t.selfNick);
}

bool isGrey(uint16_t c) {
  const int r = (c >> 11) & 0x1F, g = (c >> 6) & 0x1F, b = c & 0x1F;  // g at 5 bits
  return r == g && g == b;
}
}  // namespace

void test_every_theme_readable() {
  for (ThemeId id : kAll) assertReadable(theme(id));
}

void test_default_is_the_terminal_look() {
  TEST_ASSERT_EQUAL_PTR(&defaultTheme(), &theme(ThemeId::Default));
  TEST_ASSERT_TRUE(theme(ThemeId::Default).colorNicks);
}

void test_black_white_is_white_on_black_greys_only() {
  const Theme& t = theme(ThemeId::BlackWhite);
  TEST_ASSERT_EQUAL_HEX16(0x0000, t.bg);
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, t.fg);
  TEST_ASSERT_FALSE(t.colorNicks);
  const uint16_t all[] = {t.bg, t.fg, t.dim, t.barBg, t.barFg, t.hintBg, t.hintFg, t.time, t.sys,
                          t.mentionBg, t.inputBg, t.cursorBg, t.cursorFg, t.selectBg, t.selectFg,
                          t.drawerBg, t.drawerEdge, t.error, t.nick, t.selfNick};
  for (uint16_t c : all) TEST_ASSERT_TRUE(isGrey(c));
}

void test_white_black_is_black_on_white_greys_only() {
  const Theme& t = theme(ThemeId::WhiteBlack);
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, t.bg);
  TEST_ASSERT_EQUAL_HEX16(0x0000, t.fg);
  TEST_ASSERT_FALSE(t.colorNicks);
  const uint16_t all[] = {t.bg, t.fg, t.dim, t.barBg, t.barFg, t.hintBg, t.hintFg, t.time, t.sys,
                          t.mentionBg, t.inputBg, t.cursorBg, t.cursorFg, t.selectBg, t.selectFg,
                          t.drawerBg, t.drawerEdge, t.error, t.nick, t.selfNick};
  for (uint16_t c : all) TEST_ASSERT_TRUE(isGrey(c));
}

void test_mono_nicks_follow_theme() {
  const Theme& t = theme(ThemeId::BlackWhite);
  TEST_ASSERT_EQUAL_HEX16(t.nick, nickColor(t, "tardibyte", false));
  TEST_ASSERT_EQUAL_HEX16(t.nick, nickColor(t, "oumike", false));
  TEST_ASSERT_EQUAL_HEX16(t.selfNick, nickColor(t, "oumike", true));
}

void test_default_nicks_keep_site_colours() {
  const Theme& t = defaultTheme();
  TEST_ASSERT_EQUAL_HEX16(hslToRgb565(nickHue("tardibyte"), 0.70f, 0.68f), nickColor(t, "tardibyte", false));
  TEST_ASSERT_EQUAL_HEX16(0xFE8C, nickColor(t, "oumike", true));
}

void test_theme_ids_round_trip() {
  for (ThemeId id : kAll) {
    ThemeId back = ThemeId::Default;
    TEST_ASSERT_TRUE(parseThemeId(themeKey(id), back));
    TEST_ASSERT_EQUAL(static_cast<int>(id), static_cast<int>(back));
  }
  ThemeId out = ThemeId::WhiteBlack;
  TEST_ASSERT_FALSE(parseThemeId("neon", out));
  TEST_ASSERT_EQUAL(static_cast<int>(ThemeId::WhiteBlack), static_cast<int>(out));
  TEST_ASSERT_EQUAL(static_cast<int>(ThemeId::Default), static_cast<int>(themeFromIndex(kThemeCount)));
  TEST_ASSERT_EQUAL(static_cast<int>(ThemeId::Default), static_cast<int>(themeFromIndex(-1)));
}

// Saved settings and backups hold these numbers and keys: they must not move.
void test_site_theme_ids_are_stable() {
  TEST_ASSERT_EQUAL(15, kThemeCount);
  TEST_ASSERT_EQUAL(3, static_cast<int>(ThemeId::Dark));
  TEST_ASSERT_EQUAL(14, static_cast<int>(ThemeId::Top8));
  TEST_ASSERT_EQUAL_STRING("vt320", themeKey(ThemeId::Vt320));
  TEST_ASSERT_EQUAL_STRING("GRiD", themeLabel(ThemeId::Grid));
  for (int i = 0; i < kThemeCount; ++i)
    for (int j = i + 1; j < kThemeCount; ++j) {
      TEST_ASSERT_TRUE(std::string(themeKey(themeFromIndex(i))) != themeKey(themeFromIndex(j)));
      TEST_ASSERT_TRUE(std::string(themeLabel(themeFromIndex(i))) != themeLabel(themeFromIndex(j)));
    }
}

// Bars, the input line and panels must stand out from the page.
void test_site_themes_have_visible_panels() {
  for (int i = static_cast<int>(ThemeId::Dark); i < kThemeCount; ++i) {
    const Theme& t = theme(themeFromIndex(i));
    TEST_ASSERT_NOT_EQUAL(t.bg, t.barBg);
    TEST_ASSERT_NOT_EQUAL(t.bg, t.inputBg);
    TEST_ASSERT_NOT_EQUAL(t.bg, t.mentionBg);
    TEST_ASSERT_NOT_EQUAL(t.nick, t.selfNick);
  }
}

void test_site_colours_match() {
  const Theme& d = theme(ThemeId::Dark);
  TEST_ASSERT_EQUAL_HEX16(0x0000, d.bg);
  TEST_ASSERT_EQUAL_HEX16(0xEF38, d.fg);  // #efe5c0
  const Theme& c = theme(ThemeId::C64);
  TEST_ASSERT_EQUAL_HEX16(0x2957, c.bg);  // #2a2ab8
}

void test_next_theme_wraps() {
  TEST_ASSERT_EQUAL(static_cast<int>(ThemeId::BlackWhite), static_cast<int>(nextTheme(ThemeId::Default)));
  TEST_ASSERT_EQUAL(static_cast<int>(ThemeId::WhiteBlack), static_cast<int>(nextTheme(ThemeId::BlackWhite)));
  TEST_ASSERT_EQUAL(static_cast<int>(ThemeId::Dark), static_cast<int>(nextTheme(ThemeId::WhiteBlack)));
  TEST_ASSERT_EQUAL(static_cast<int>(ThemeId::Default), static_cast<int>(nextTheme(ThemeId::Top8)));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_every_theme_readable);
  RUN_TEST(test_default_is_the_terminal_look);
  RUN_TEST(test_black_white_is_white_on_black_greys_only);
  RUN_TEST(test_white_black_is_black_on_white_greys_only);
  RUN_TEST(test_mono_nicks_follow_theme);
  RUN_TEST(test_default_nicks_keep_site_colours);
  RUN_TEST(test_theme_ids_round_trip);
  RUN_TEST(test_next_theme_wraps);
  RUN_TEST(test_site_theme_ids_are_stable);
  RUN_TEST(test_site_themes_have_visible_panels);
  RUN_TEST(test_site_colours_match);
  return UNITY_END();
}
