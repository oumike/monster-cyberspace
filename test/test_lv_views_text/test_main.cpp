#include <unity.h>

#include <lvgl.h>

#include <deque>
#include <string>
#include <vector>

#include "chat/message.h"
#include "input/gestures.h"
#include "lvui/cell_font.h"
#include "lvui/display.h"
#include "lvui/input.h"
#include "lvui/lv_init.h"
#include "lvui/styles.h"
#include "lvui/views/cell_text.h"
#include "lvui/views/input_field.h"
#include "lvui/views/log_box.h"
#include "lvui/views/slider_box.h"
#include "lvui/views/text_box.h"
#include "ui/nick_color.h"
#include "ui/room_layout.h"
#include "ui/theme.h"
#include "ui/wrap.h"
#include "util/utf8.h"

using rc::Action;
using rc::ListId;

namespace {

// 320x240 in the 8x16 default font: 40 columns, 15 rows.
constexpr int kW = 320, kH = 240, kCellW = 8, kCellH = 16;

uint32_t now = 0;
uint32_t nowMs() { return now; }
void noRows(const uint16_t*, int, int) {}

int16_t ptrX = 0, ptrY = 0;
bool ptrDown = false;
bool readPtr(int16_t& x, int16_t& y) {
  x = ptrX;
  y = ptrY;
  return ptrDown;
}

std::vector<rc::KeyEvent> keys;
void sink(const rc::KeyEvent& e) { keys.push_back(e); }

void step(uint32_t ms = 40) {
  now += ms;
  lv_timer_handler();
}

void press(lv_point_t p, uint32_t holdMs) {
  ptrX = static_cast<int16_t>(p.x);
  ptrY = static_cast<int16_t>(p.y);
  ptrDown = true;
  step();
  for (uint32_t t = 0; t < holdMs; t += 40) step();
  ptrDown = false;
  step();
  step();
}
void tap(lv_point_t p) { press(p, 0); }

// A finger down at `from`, moved `dy` px down in small slow steps, then up.
void drag(lv_point_t from, int dy) {
  ptrX = static_cast<int16_t>(from.x);
  ptrY = static_cast<int16_t>(from.y);
  ptrDown = true;
  step();
  const int dir = dy < 0 ? -1 : 1;
  for (int moved = 0; moved != dy; moved += dir * 4) {
    ptrY = static_cast<int16_t>(ptrY + dir * 4);
    step(60);
  }
  ptrDown = false;
  step();
  step();
}

// A finger down at `from`, moved (dx, dy) px in twelve even steps, then up.
void diagonal(lv_point_t from, int dx, int dy) {
  ptrX = static_cast<int16_t>(from.x);
  ptrY = static_cast<int16_t>(from.y);
  ptrDown = true;
  step();
  for (int i = 1; i <= 12; ++i) {
    ptrX = static_cast<int16_t>(from.x + dx * i / 12);
    ptrY = static_cast<int16_t>(from.y + dy * i / 12);
    step(50);
  }
  ptrDown = false;
  step();
  step();
}

// The centre of cell (col, row) of an object's content.
lv_point_t cellAt(lv_obj_t* o, int col, int row) {
  lv_obj_update_layout(lv_screen_active());
  lv_area_t a;
  lv_obj_get_content_coords(o, &a);
  return {a.x1 + col * kCellW + kCellW / 2, a.y1 + row * kCellH + kCellH / 2};
}

const char* text(lv_obj_t* label) { return lv_label_get_text(label); }
uint16_t rgb565(lv_color_t c) { return lv_color_to_u16(c); }
uint16_t textColour(lv_obj_t* o) { return rgb565(lv_obj_get_style_text_color(o, LV_PART_MAIN)); }
bool hidden(lv_obj_t* o) { return lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN); }

int descendants(lv_obj_t* o) {
  int n = 0;
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) n += 1 + descendants(lv_obj_get_child(o, i));
  return n;
}

// 300 messages of three lines: "m<i> line <k>".
std::vector<rc::DisplayLine> threeLiners(int n) {
  std::vector<rc::DisplayLine> out;
  for (int i = 0; i < n; ++i)
    for (int k = 0; k < 3; ++k) {
      rc::DisplayLine l;
      l.messageId = "m" + std::to_string(i);
      l.spans.push_back({rc::utf8ToU32("m" + std::to_string(i) + " line " + std::to_string(k)), 0xFFFF});
      out.push_back(std::move(l));
    }
  return out;
}

rc::CircMessage msg(const char* id, const char* user, const char* content) {
  rc::CircMessage m;
  m.id = id;
  m.username = user;
  m.content = content;
  m.timestamp = 1700000000000;
  return m;
}

// The span of row r whose text starts with `prefix`, or -1.
int spanStarting(const lvui::LogBox& box, int r, const char* prefix) {
  for (int i = 0; i < box.spanCount(r); ++i) {
    if (box.span(r, i)) TEST_ASSERT_EQUAL_STRING(box.spanText(r, i).c_str(), text(box.span(r, i)));
    if (box.spanText(r, i).rfind(prefix, 0) == 0) return i;
  }
  return -1;
}

// The whole frame, drawn now.
std::vector<uint16_t> shot() {
  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(nullptr);
  const uint16_t* f = lvui::frame();
  return std::vector<uint16_t>(f, f + kW * kH);
}

}  // namespace

void setUp() {
  lvui::setTickSource(nowMs);
  lvui::init();
  static lv_display_t* disp = lvui::displayBegin({kW, kH, noRows});
  (void)disp;
  lvui::pointerBegin(readPtr);
  lvui::setKeySink(sink);
  lvui::setTouchOnly(false);
  lvui::applyTheme(rc::defaultTheme());
  rc::setCursorShown(true);
  lv_obj_clean(lv_screen_active());
  lv_obj_clean(lv_layer_top());
  step();
  keys.clear();
}
void tearDown() {}

void test_log_view_virtualised() {
  rc::LogView m;
  m.setLines(threeLiners(300));
  lvui::LogBox box;
  box.create(lv_screen_active());
  box.sync(m);
  TEST_ASSERT_EQUAL(kH / kCellH, box.rows());
  const int bound = 3 * box.rows();
  TEST_ASSERT_LESS_OR_EQUAL(bound, descendants(box.obj()));
  // Following: the newest lines at the bottom.
  TEST_ASSERT_EQUAL_STRING("m299 line 2", box.rowText(box.rows() - 1).c_str());
  TEST_ASSERT_FALSE(hidden(box.scrollBar().thumb()));
  // Every frame syncs: the same shape keeps the same objects.
  if (!box.painted()) {
    lv_obj_t* firstRow = box.row(0);
    lv_obj_t* firstSpan = box.span(0, 0);
    box.sync(m);
    TEST_ASSERT_EQUAL_PTR(firstRow, box.row(0));
    TEST_ASSERT_EQUAL_PTR(firstSpan, box.span(0, 0));
  }

  while (!m.atTop()) {
    m.up();
    box.sync(m);
  }
  TEST_ASSERT_EQUAL_STRING("m0 line 0", box.rowText(0).c_str());
  TEST_ASSERT_LESS_OR_EQUAL(bound, descendants(box.obj()));
  TEST_ASSERT_EQUAL(0, lv_obj_get_y(box.scrollBar().thumb()));
  // The selected message (m0) is outlined: three rows, the bar column clear.
  TEST_ASSERT_FALSE(hidden(box.selection()));
  lv_obj_update_layout(box.obj());
  TEST_ASSERT_EQUAL(0, lv_obj_get_y(box.selection()));
  TEST_ASSERT_EQUAL(3 * kCellH, lv_obj_get_height(box.selection()));
  TEST_ASSERT_EQUAL((kW / kCellW - 1) * kCellW, lv_obj_get_width(box.selection()));

}

void test_log_rows_tap_and_hold() {
  rc::LogView m;
  m.setLines(threeLiners(300));
  lvui::LogBox box;
  box.create(lv_screen_active());
  box.sync(m);
  // The bottom row is message 299: a tap acts then Selects, a hold opens the palette.
  tap(cellAt(box.obj(), 3, box.rows() - 1));
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_EQUAL(Action::Activate, keys[0].action);
  TEST_ASSERT_EQUAL(299, keys[0].index);
  TEST_ASSERT_EQUAL(static_cast<uint8_t>(ListId::Log), keys[0].target);
  TEST_ASSERT_EQUAL(Action::Select, keys[0].then);
  keys.clear();
  press(cellAt(box.obj(), 3, box.rows() - 1), 800);
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_EQUAL(Action::Palette, keys[0].then);
  // Touch-only boards: a tap does nothing, a hold Selects.
  lvui::setTouchOnly(true);
  keys.clear();
  tap(cellAt(box.obj(), 3, box.rows() - 1));
  TEST_ASSERT_EQUAL(0, keys.size());
  press(cellAt(box.obj(), 3, box.rows() - 1), 800);
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_EQUAL(Action::Select, keys[0].then);
  lvui::setTouchOnly(false);
  // A drag down (finger moving down) scrolls back: Up keys, one per cell.
  keys.clear();
  drag(cellAt(box.obj(), 3, 2), 3 * kCellH);
  TEST_ASSERT_GREATER_OR_EQUAL(2, keys.size());
  for (const rc::KeyEvent& k : keys) TEST_ASSERT_EQUAL(Action::Up, k.action);
  // A mostly sideways move from the right eighth is an edge swipe, not a
  // drag, even with more than a tap's slop downwards: the log emits nothing.
  keys.clear();
  diagonal(cellAt(box.obj(), kW / kCellW - 4, 2), -96, 3 * kCellH);
  TEST_ASSERT_EQUAL(0, keys.size());
}

void test_log_nick_colour() {
  rc::Theme th = rc::defaultTheme();
  lvui::applyTheme(th);
  std::deque<rc::CircMessage> msgs{msg("a", "alice", "hello there"), msg("b", "bob", "hi alice")};
  rc::LayoutOptions o;
  o.cols = kW / kCellW - 1;
  o.selfUsername = "bob";
  o.theme = &th;
  rc::LogView m;
  m.setLines(rc::layoutRoom(msgs, o));
  lvui::LogBox box;
  box.create(lv_screen_active());
  box.sync(m);
  int alice = -1, bob = -1, aliceRow = -1, bobRow = -1;
  for (int r = 0; r < box.rows(); ++r) {
    if (alice < 0 && (alice = spanStarting(box, r, "alice")) >= 0) aliceRow = r;
    if (bob < 0 && (bob = spanStarting(box, r, "bob")) >= 0) bobRow = r;
  }
  TEST_ASSERT_GREATER_OR_EQUAL(0, alice);
  TEST_ASSERT_GREATER_OR_EQUAL(0, bob);
  TEST_ASSERT_EQUAL_HEX16(rc::nickColor(th, "alice", false), box.spanFg(aliceRow, alice));
  TEST_ASSERT_EQUAL_HEX16(rc::nickColor(th, "bob", true), box.spanFg(bobRow, bob));
  if (!box.painted()) {
    TEST_ASSERT_EQUAL_HEX16(rc::nickColor(th, "alice", false), textColour(box.span(aliceRow, alice)));
    TEST_ASSERT_EQUAL_HEX16(rc::nickColor(th, "bob", true), textColour(box.span(bobRow, bob)));
  }
}

// A log of coloured, bold, wide, tinted and too-long lines, the second
// message selected.
rc::LogView paintLog() {
  std::vector<rc::DisplayLine> lines;
  auto line = [&](const char* id, std::vector<rc::Span> spans, bool mention = false) {
    rc::DisplayLine l;
    l.messageId = id;
    l.spans = std::move(spans);
    l.mentionTint = mention;
    lines.push_back(std::move(l));
  };
  for (int i = 0; i < 6; ++i) line(("o" + std::to_string(i)).c_str(), {{U"older " + rc::utf8ToU32(std::to_string(i)), 0x8410}});
  line("a", {{U"12:00 ", 0x07FF}, {U"alice", 0xF800, true}, {U": hi \U0001F600 there", 0xFFFF}});
  line("b", {{U"12:01 ", 0x07FF}, {U"bob", 0x07E0, true}, {U": @alice look", 0xFFFF}}, true);
  line("b", {{U"      a second line for bob", 0xFFFF}}, true);
  line("c", {{U"12:02 ", 0x07FF},
             {U"carol", 0xFFE0},
             {U": a line far too long for the row, so it runs past the bar column and is cut", 0xFFFF}});
  line("d", {});
  line("e", {{U"\u2022 sys", 0x8410}, {U"", 0xFFFF}, {U" end", 0xF81F, true}});
  rc::LogView m;
  m.setLines(std::move(lines));
  return m;
}

void test_log_painted_matches_labels() {
  for (int pass = 0; pass < 2; ++pass) {
    std::vector<uint16_t> want;
    for (bool painted : {false, true}) {
      lv_obj_clean(lv_screen_active());
      lvui::LogBox::setPainted(painted);
      rc::LogView m = paintLog();
      // The second pass selects message b, so the outline shows too.
      if (pass == 1)
        while (!m.atTop()) m.up();
      lvui::LogBox box;
      box.create(lv_screen_active());
      lv_obj_set_size(box.obj(), kW, 11 * kCellH);
      box.sync(m);
      TEST_ASSERT_EQUAL(painted, box.painted());
      if (painted) TEST_ASSERT_LESS_OR_EQUAL(1 + 2 + 2, descendants(box.obj()));
      const std::vector<uint16_t> got = shot();
      if (!painted) {
        want = got;
        continue;
      }
      int diff = 0;
      for (size_t i = 0; i < got.size(); ++i) diff += got[i] != want[i];
      TEST_ASSERT_EQUAL_MESSAGE(0, diff, pass ? "selected" : "following");
      // A changed row is drawn again, and still matches.
      lv_obj_clean(lv_screen_active());
    }
  }
  lvui::LogBox::setPainted(RC_LV_LEAN);
}

void test_log_painted_redraws_changed_rows() {
  lvui::LogBox::setPainted(true);
  rc::LogView m;
  m.setLines(threeLiners(10));
  lvui::LogBox box;
  box.create(lv_screen_active());
  box.sync(m);
  step();
  lv_display_t* disp = lv_display_get_default();
  int count = 0;
  auto cb = [](lv_event_t* e) { ++*static_cast<int*>(lv_event_get_user_data(e)); };
  lv_display_add_event_cb(disp, cb, LV_EVENT_INVALIDATE_AREA, &count);
  box.sync(m);
  TEST_ASSERT_EQUAL(0, count);
  // One new line scrolls every row up: each row is drawn again.
  m.setLines(threeLiners(11));
  box.sync(m);
  lv_display_remove_event_cb_with_user_data(disp, cb, &count);
  TEST_ASSERT_GREATER_OR_EQUAL(1, count);
  TEST_ASSERT_EQUAL_STRING("m10 line 2", box.rowText(box.rows() - 1).c_str());
  // Its pixels match the labels' after the change.
  const std::vector<uint16_t> got = shot();
  lv_obj_clean(lv_screen_active());
  lvui::LogBox::setPainted(false);
  lvui::LogBox ref;
  ref.create(lv_screen_active());
  ref.sync(m);
  const std::vector<uint16_t> want = shot();
  TEST_ASSERT_TRUE(got == want);
  lvui::LogBox::setPainted(RC_LV_LEAN);
}

void test_log_painted_taps() {
  lvui::LogBox::setPainted(true);
  test_log_rows_tap_and_hold();
  lvui::LogBox::setPainted(RC_LV_LEAN);
}

void test_input_field_cursor_and_focus_tap() {
  rc::InputLine m;
  m.setText(U"hello");
  m.setCursor(2);
  m.setFieldId(5);
  lvui::InputField f;
  f.create(lv_screen_active());
  f.sync(m);
  lv_obj_update_layout(lv_screen_active());
  TEST_ASSERT_EQUAL_STRING("> ", text(f.prompt()));
  TEST_ASSERT_EQUAL_STRING("he", text(f.before()));
  TEST_ASSERT_EQUAL_STRING("l", text(f.cursor()));
  TEST_ASSERT_EQUAL_STRING("lo", text(f.after()));
  TEST_ASSERT_EQUAL(4 * kCellW, lv_obj_get_x(f.cursor()));
  TEST_ASSERT_EQUAL_HEX16(rc::defaultTheme().cursorBg,
                          rgb565(lv_obj_get_style_bg_color(f.cursor(), LV_PART_MAIN)));
  // At the end the cursor is a blank cell after the text.
  m.setCursor(5);
  f.sync(m);
  lv_obj_update_layout(lv_screen_active());
  TEST_ASSERT_EQUAL_STRING(" ", text(f.cursor()));
  TEST_ASSERT_EQUAL(7 * kCellW, lv_obj_get_x(f.cursor()));

  tap(cellAt(f.obj(), 2 + 4, 0));
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_EQUAL(Action::Focus, keys[0].action);
  TEST_ASSERT_EQUAL(4, keys[0].index);
  TEST_ASSERT_EQUAL(5, keys[0].target);
  // Past the text: the end.
  keys.clear();
  tap(cellAt(f.obj(), 30, 0));
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_EQUAL(5, keys[0].index);
  // Without the emoji button its label is never made.
  TEST_ASSERT_NULL(f.emoji());
  // The emoji button (a pointer is fitted): the first cells open the tray.
  m.setEmojiButton(true);
  f.sync(m);
  TEST_ASSERT_FALSE(hidden(f.emoji()));
  keys.clear();
  tap(cellAt(f.obj(), 0, 0));
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_EQUAL(Action::Emoji, keys[0].action);
}

void test_text_box_wraps_like_wrap() {
  const std::u32string src =
      U"The quick brown fox jumps over the lazy dog, and then it runs far away into the hills.";
  const int cols = kW / kCellW;
  rc::TextView m;
  m.setText(src);
  lvui::TextBox box;
  box.create(lv_screen_active());
  box.sync(m);
  std::vector<std::u32string> want = rc::wrapText(src, cols, cols);
  for (size_t i = 0; i < want.size(); ++i)
    TEST_ASSERT_EQUAL_STRING(rc::u32ToUtf8(want[i]).c_str(), text(box.label(static_cast<int>(i))));
  TEST_ASSERT_TRUE(hidden(box.scrollBar().thumb()));

  // Overflowing text leaves a column for the scroll bar.
  std::u32string longer;
  for (int i = 0; i < 12; ++i) longer += src + U"\n";
  m.setText(longer);
  box.sync(m);
  want = rc::wrapText(longer, cols - 1, cols - 1);
  for (int r = 0; r < box.rows(); ++r)
    TEST_ASSERT_EQUAL_STRING(rc::u32ToUtf8(want[r]).c_str(), text(box.label(r)));
  TEST_ASSERT_FALSE(hidden(box.scrollBar().thumb()));
  m.down();
  box.sync(m);
  TEST_ASSERT_EQUAL_STRING(rc::u32ToUtf8(want[1]).c_str(), text(box.label(0)));
}

void test_text_area_cursor_and_tap() {
  rc::TextArea a;
  for (char32_t c : std::u32string(U"hello world")) a.insert(c);
  a.setFieldId(3);
  a.setCursor(4);
  lvui::TextBox box;
  box.create(lv_screen_active());
  box.sync(a, true);
  lv_obj_update_layout(lv_screen_active());
  TEST_ASSERT_EQUAL_STRING("hello world", text(box.label(0)));
  TEST_ASSERT_FALSE(hidden(box.cursor()));
  TEST_ASSERT_EQUAL_STRING("o", text(box.cursor()));
  TEST_ASSERT_EQUAL(4 * kCellW, lv_obj_get_x(box.cursor()));
  tap(cellAt(box.obj(), 7, 0));
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_EQUAL(Action::Focus, keys[0].action);
  TEST_ASSERT_EQUAL(7, keys[0].index);
  TEST_ASSERT_EQUAL(3, keys[0].target);
}

void test_slider_taps() {
  rc::Slider s;
  s.open(U"Volume", 50, 100, 10, U"off");
  lvui::SliderBox box;
  box.create(lv_screen_active());
  box.sync(s);
  TEST_ASSERT_EQUAL_STRING("Volume", text(box.title()));
  lv_obj_update_layout(lv_screen_active());
  lv_area_t a;
  lv_obj_get_coords(box.box(), &a);
  const int w = lv_area_get_width(&a) / kCellW;
  // 40 columns: a 28-cell box, a 20-cell bar from box column 4, "+" at 25.
  TEST_ASSERT_EQUAL(28, w);
  const lv_point_t rightEdge{a.x1 + 25 * kCellW + kCellW / 2, a.y1 + 3 * kCellH + kCellH / 2};
  const lv_point_t leftEdge{a.x1 + kCellW + kCellW / 2, a.y1 + 3 * kCellH + kCellH / 2};
  const lv_point_t segment{a.x1 + 6 * kCellW + kCellW / 2, a.y1 + 3 * kCellH + kCellH / 2};
  const lv_point_t setBtn{a.x1 + 24 * kCellW + kCellW / 2, a.y1 + 6 * kCellH + kCellH / 2};
  tap(segment);
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_EQUAL(Action::Activate, keys[0].action);
  TEST_ASSERT_EQUAL(1, keys[0].index);
  TEST_ASSERT_EQUAL(static_cast<uint8_t>(ListId::Menu), keys[0].target);
  keys.clear();
  tap(setBtn);
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_EQUAL(Action::Select, keys[0].action);
  keys.clear();
  tap(rightEdge);
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_EQUAL(Action::Right, keys[0].action);
  keys.clear();
  tap(leftEdge);
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_EQUAL(Action::Left, keys[0].action);
  // A stray tap outside the box is swallowed.
  keys.clear();
  tap({2, 2});
  TEST_ASSERT_EQUAL(0, keys.size());
  // A drag is no tap: down on Set, across the box and up sends nothing,
  // nor does a hold that moved off it first.
  ptrX = static_cast<int16_t>(setBtn.x);
  ptrY = static_cast<int16_t>(setBtn.y);
  ptrDown = true;
  step();
  for (int i = 1; i <= 8; ++i) {
    ptrX = static_cast<int16_t>(setBtn.x - 20 * kCellW * i / 8);
    step();
  }
  step(rc::kLongPressMs);
  ptrDown = false;
  step();
  step();
  TEST_ASSERT_EQUAL(0, keys.size());
}

void test_cell_text_lines_wrap_and_hint() {
  lvui::CellText ct;
  ct.create(lv_screen_active());
  const int cols = kW / kCellW, rows = kH / kCellH;
  TEST_ASSERT_EQUAL(cols, ct.cols());
  TEST_ASSERT_EQUAL(rows, ct.rows());
  ct.begin();
  // Cut to whole cells at the right edge, as Grid::putText.
  TEST_ASSERT_EQUAL(5, ct.line(cols - 5, 2, std::string_view("abcdefgh"), 0x1234));
  TEST_ASSERT_EQUAL(2, ct.filled(3, 4, U"xy", 0xFFFF, 0x07E0));
  const std::string para = "The quick brown fox jumps over the lazy dog, and then it runs far away.";
  const int next = ct.wrapped(1, 6, para, 0x00FF);
  ct.end();
  lv_obj_update_layout(lv_screen_active());
  const auto want = rc::wrapText(rc::utf8ToU32(para), cols - 1, cols - 1);
  TEST_ASSERT_EQUAL(6 + static_cast<int>(want.size()), next);
  TEST_ASSERT_EQUAL(2 + static_cast<int>(want.size()), ct.count());
  TEST_ASSERT_EQUAL_STRING("abcde", text(ct.label(0)));
  TEST_ASSERT_EQUAL((cols - 5) * kCellW, lv_obj_get_x(ct.label(0)));
  TEST_ASSERT_EQUAL(2 * kCellH, lv_obj_get_y(ct.label(0)));
  TEST_ASSERT_EQUAL_HEX16(0x1234, textColour(ct.label(0)));
  TEST_ASSERT_EQUAL_HEX16(0x07E0, rgb565(lv_obj_get_style_bg_color(ct.label(1), LV_PART_MAIN)));
  TEST_ASSERT_EQUAL(LV_OPA_COVER, lv_obj_get_style_bg_opa(ct.label(1), LV_PART_MAIN));
  TEST_ASSERT_EQUAL_STRING(rc::u32ToUtf8(want[0]).c_str(), text(ct.label(2)));
  TEST_ASSERT_EQUAL(1 * kCellW, lv_obj_get_x(ct.label(2)));

  // A shorter frame hides the leftovers and clears a reused label's fill.
  ct.begin();
  ct.line(0, 0, U"a", 0xFFFF);
  ct.line(0, 1, U"b", 0xFFFF);
  ct.end();
  TEST_ASSERT_EQUAL(LV_OPA_TRANSP, lv_obj_get_style_bg_opa(ct.label(1), LV_PART_MAIN));
  TEST_ASSERT_FALSE(hidden(ct.label(1)));
#if RC_LOW_MEM
  TEST_ASSERT_EQUAL(2u, lv_obj_get_child_count(lv_obj_get_parent(ct.label(0))));
#else
  TEST_ASSERT_TRUE(hidden(ct.label(2)));
#endif

  // Wrapping stops above the hint row.
  ct.begin();
  std::string many;
  for (int i = 0; i < 40; ++i) many += "word" + std::to_string(i) + " ";
  TEST_ASSERT_EQUAL(rows - 1, ct.wrapped(0, rows - 3, many, 0xFFFF));
  // Or above a given row.
  TEST_ASSERT_EQUAL(4, ct.wrapped(0, 3, many, 0xFFFF, 4));
  ct.end();

  lvui::HintBar h;
  h.create(lv_screen_active());
  h.sync(U"tap to select");
  lv_obj_update_layout(lv_screen_active());
  TEST_ASSERT_EQUAL((rows - 1) * kCellH, lv_obj_get_y(h.obj()));
  TEST_ASSERT_EQUAL(kW, lv_obj_get_width(h.obj()));
  TEST_ASSERT_EQUAL_STRING("tap to select", text(h.label()));
  TEST_ASSERT_EQUAL_HEX16(rc::defaultTheme().hintBg, rgb565(lv_obj_get_style_bg_color(h.obj(), LV_PART_MAIN)));
}

int invalidations = 0;
void countInvalidation(lv_event_t*) { ++invalidations; }

void test_cell_text_unchanged_frame_invalidates_nothing() {
  lvui::CellText ct;
  ct.create(lv_screen_active());
  auto frame = [&] {
    ct.begin();
    ct.line(0, 0, U"ab", 0xFFFF);
    ct.filled(2, 0, U" ", 0x0000, 0x07E0);
    ct.line(0, 1, U"cd", 0xFFFF);
    // Off the grid: puts nothing.
    TEST_ASSERT_EQUAL(0, ct.line(0, ct.rows(), U"ef", 0xFFFF));
    ct.end();
  };
  frame();
  frame();
  step();
  TEST_ASSERT_EQUAL(2 + 1, ct.count());
  lv_display_t* disp = lv_display_get_default();
  invalidations = 0;
  lv_display_add_event_cb(disp, countInvalidation, LV_EVENT_INVALIDATE_AREA, nullptr);
  frame();
  lv_display_remove_event_cb_with_user_data(disp, countInvalidation, nullptr);
  TEST_ASSERT_EQUAL(0, invalidations);
  TEST_ASSERT_EQUAL(LV_OPA_COVER, lv_obj_get_style_bg_opa(ct.label(1), LV_PART_MAIN));
  TEST_ASSERT_EQUAL(LV_OPA_TRANSP, lv_obj_get_style_bg_opa(ct.label(2), LV_PART_MAIN));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_log_view_virtualised);
  RUN_TEST(test_log_rows_tap_and_hold);
  RUN_TEST(test_log_nick_colour);
  RUN_TEST(test_log_painted_matches_labels);
  RUN_TEST(test_log_painted_redraws_changed_rows);
  RUN_TEST(test_log_painted_taps);
  RUN_TEST(test_input_field_cursor_and_focus_tap);
  RUN_TEST(test_text_box_wraps_like_wrap);
  RUN_TEST(test_text_area_cursor_and_tap);
  RUN_TEST(test_slider_taps);
  RUN_TEST(test_cell_text_lines_wrap_and_hint);
  RUN_TEST(test_cell_text_unchanged_frame_invalidates_nothing);
  return UNITY_END();
}
