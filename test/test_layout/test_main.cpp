#include <unity.h>

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <set>
#include <string>

#include "feed/entry.h"
#include "ui/feed_layout.h"
#include "ui/grid.h"
#include "ui/nick_color.h"
#include "ui/room_layout.h"
#include "ui/splash_layout.h"
#include "ui/theme.h"
#include "ui/timefmt.h"
#include "util/utf8.h"

using namespace rc;

static const int64_t kTs = 1761400920000LL;  // Sat 25 Oct 2025 14:02 UTC

void setUp() {
  setenv("TZ", "UTC0", 1);
  tzset();
}
void tearDown() {}

static CircMessage msg(const std::string& id, const std::string& user,
                       const std::string& content, int64_t ts = kTs) {
  CircMessage m;
  m.id = id;
  m.userId = "u-" + user;
  m.username = user;
  m.content = content;
  m.timestamp = ts;
  return m;
}

static std::string flat(const DisplayLine& l) {
  std::u32string s;
  for (const auto& sp : l.spans) s += sp.text;
  return u32ToUtf8(s);
}

static size_t width(const DisplayLine& l) {
  size_t n = 0;
  for (const auto& sp : l.spans) n += sp.text.size();
  return n;
}

static LayoutOptions opts(TextSize s, int cols) {
  LayoutOptions o;
  o.size = s;
  o.cols = cols;
  o.selfUsername = "oumike";
  return o;
}

// Lines of the messages only (no dividers).
static std::vector<DisplayLine> body(const std::vector<DisplayLine>& in) {
  std::vector<DisplayLine> out;
  for (const auto& l : in)
    if (!l.divider) out.push_back(l);
  return out;
}

void test_grid_dims() {
  int c, r;
  gridDims(320, 240, TextSize::Comfortable, c, r);
  TEST_ASSERT_EQUAL(40, c);
  TEST_ASSERT_EQUAL(15, r);
  gridDims(320, 240, TextSize::Compact, c, r);
  TEST_ASSERT_EQUAL(53, c);
  TEST_ASSERT_EQUAL(20, r);
  TEST_ASSERT_EQUAL(8, cellSize(TextSize::Comfortable).w);
  TEST_ASSERT_EQUAL(12, cellSize(TextSize::Compact).h);
}

void test_big_cells() {
  TEST_ASSERT_EQUAL(8, bigCellSize(TextSize::Compact).w);
  TEST_ASSERT_EQUAL(14, bigCellSize(TextSize::Compact).h);
  TEST_ASSERT_EQUAL(10, bigCellSize(TextSize::Comfortable).w);
  TEST_ASSERT_EQUAL(18, bigCellSize(TextSize::Comfortable).h);
  TEST_ASSERT_EQUAL(40, bigCols(320, TextSize::Compact));
  TEST_ASSERT_EQUAL(32, bigCols(320, TextSize::Comfortable));
}

void test_grid_row_kinds() {
  Grid g(10, 4);
  TEST_ASSERT_TRUE(g.rowKind(1) == RowKind::Normal);
  g.setRowKind(1, RowKind::Big);
  g.setRowKind(2, RowKind::Skip);
  g.setRowKind(9, RowKind::Big);  // out of range: ignored
  TEST_ASSERT_TRUE(g.rowKind(1) == RowKind::Big);
  TEST_ASSERT_TRUE(g.rowKind(9) == RowKind::Normal);
  // Rows stack by height; the last row stays at the bottom.
  std::vector<RowSpan> s = rowSpans(g, 12, 14);
  TEST_ASSERT_EQUAL(4, int(s.size()));
  TEST_ASSERT_EQUAL(0, s[0].y);
  TEST_ASSERT_EQUAL(12, s[0].h);
  TEST_ASSERT_EQUAL(12, s[1].y);
  TEST_ASSERT_EQUAL(14, s[1].h);
  TEST_ASSERT_EQUAL(0, s[2].h);
  TEST_ASSERT_EQUAL(36, s[3].y);
  TEST_ASSERT_EQUAL(12, s[3].h);
  g.fill(0);
  TEST_ASSERT_TRUE(g.rowKind(1) == RowKind::Normal);
  TEST_ASSERT_TRUE(g.rowKind(2) == RowKind::Normal);
}

void test_row_spans_cut_before_last_row() {
  Grid g(5, 3);
  g.setRowKind(0, RowKind::Big);
  g.setRowKind(1, RowKind::Big);
  std::vector<RowSpan> s = rowSpans(g, 12, 14);
  TEST_ASSERT_EQUAL(14, s[0].h);
  TEST_ASSERT_EQUAL(14, s[1].y);
  TEST_ASSERT_EQUAL(10, s[1].h);  // cut where the last row starts
  TEST_ASSERT_EQUAL(24, s[2].y);
}

void test_row_spans_pad_rows() {
  Grid g(5, 4);
  g.setRowPad(1, 7);
  std::vector<RowSpan> s = rowSpans(g, 12, 14);
  TEST_ASSERT_EQUAL(7, s[1].h);
  TEST_ASSERT_EQUAL(19, s[2].y);
  TEST_ASSERT_EQUAL(-1, g.rowPad(0));
  g.fill(0);  // clears pads with the row kinds
  TEST_ASSERT_EQUAL(-1, g.rowPad(1));
  TEST_ASSERT_EQUAL(24, rowSpans(g, 12, 14)[2].y);
}

void test_row_spans_shift() {
  Grid g(5, 3);
  g.setRowShift(1, 15);
  std::vector<RowSpan> s = rowSpans(g, 12, 14);
  TEST_ASSERT_EQUAL(0, s[0].x);
  TEST_ASSERT_EQUAL(15, s[1].x);
  g.fill(0);
  TEST_ASSERT_EQUAL(0, rowSpans(g, 12, 14)[1].x);
}

void test_grid_add_edges() {
  Grid g(4, 2);
  g.put(1, 0, U'a', 1, 2);
  g.addEdges(1, 0, kEdgeTop | kEdgeRight, 7);
  g.addEdges(1, 0, kEdgeLeft, 7);
  g.addEdges(9, 9, kEdgeTop, 7);  // out of range: ignored
  TEST_ASSERT_EQUAL(kEdgeTop | kEdgeRight | kEdgeLeft, g.at(1, 0).edges);
  TEST_ASSERT_EQUAL_HEX16(7, g.at(1, 0).edgeColor);
}

void test_grid_dirty() {
  Grid g(10, 3);
  g.clearDirty();
  g.put(1, 1, U'a', 1, 0);
  TEST_ASSERT_TRUE(g.rowDirty(1));
  TEST_ASSERT_FALSE(g.rowDirty(0));
  g.clearDirty();
  g.put(1, 1, U'a', 1, 0);
  TEST_ASSERT_FALSE(g.rowDirty(1));
  g.put(1, 1, U'a', 2, 0);
  TEST_ASSERT_TRUE(g.rowDirty(1));
  g.clearDirty();
  g.put(-1, 0, U'x', 1, 0);
  g.put(10, 0, U'x', 1, 0);
  g.put(0, 3, U'x', 1, 0);
  TEST_ASSERT_FALSE(g.rowDirty(0));
  TEST_ASSERT_EQUAL(3, g.putText(6, 2, U"hello", 1, 0, 3));
  TEST_ASSERT_EQUAL_HEX32(U'h', g.at(6, 2).cp);
  TEST_ASSERT_EQUAL_HEX32(U'e', g.at(7, 2).cp);
  TEST_ASSERT_EQUAL(2, g.putText(8, 1, U"hello", 1, 0, 9));
}

// Screens redraw from scratch each frame: fill, then put the same text back.
// Only rows that differ from what was last drawn are dirty.
void test_grid_redraw_same_frame_not_dirty() {
  Grid g(10, 3);
  g.putText(0, 1, U"hi", 1, 0, 10);
  g.clearDirty();
  g.fill(0);
  g.putText(0, 1, U"hi", 1, 0, 10);
  TEST_ASSERT_FALSE(g.rowDirty(1));
  g.fill(0);
  g.putText(0, 1, U"ho", 1, 0, 10);
  TEST_ASSERT_TRUE(g.rowDirty(1));
  TEST_ASSERT_FALSE(g.rowDirty(0));
  g.resize(10, 3);
  TEST_ASSERT_TRUE(g.rowDirty(0));
}

void test_time() {
  TEST_ASSERT_EQUAL_STRING("14:02", formatHHMM(kTs).c_str());
  TEST_ASSERT_EQUAL_STRING("Sat 25 Oct", formatDay(kTs).c_str());
  TEST_ASSERT_EQUAL_STRING("Sun 5 Oct", formatDay(1759622400000LL).c_str());
}

void test_compact_basic() {
  std::deque<CircMessage> d{msg("m1", "tardibyte",
                                "anyone else on the 4.0 beta yet? the grid build tools are finally in")};
  auto lines = body(layoutRoom(d, opts(TextSize::Compact, 53)));
  TEST_ASSERT_EQUAL(2, lines.size());
  TEST_ASSERT_EQUAL_STRING("14:02 tardibyte: anyone else on the 4.0 beta yet? the",
                           flat(lines[0]).c_str());
  TEST_ASSERT_EQUAL_STRING("                 grid build tools are finally in",
                           flat(lines[1]).c_str());
  TEST_ASSERT_EQUAL_STRING("m1", lines[1].messageId.c_str());
  const Theme& t = defaultTheme();
  TEST_ASSERT_EQUAL_HEX16(t.time, lines[0].spans[0].fg);
  TEST_ASSERT_EQUAL_HEX16(nickColor(defaultTheme(), "tardibyte", false), lines[0].spans[1].fg);
}

void test_compact_long_nick_indent() {
  std::deque<CircMessage> d{msg("m1", "averyveryverylongnick", std::string(80, 'w'))};
  auto lines = body(layoutRoom(d, opts(TextSize::Compact, 53)));
  TEST_ASSERT_TRUE(lines.size() >= 2);
  TEST_ASSERT_EQUAL_STRING("      w", flat(lines[1]).substr(0, 7).c_str());
}

void test_compact_day_divider_first() {
  std::deque<CircMessage> d{msg("m1", "a", "one"), msg("m2", "b", "two", kTs + 60000),
                            msg("m3", "c", "three", kTs + 86400000LL)};
  auto lines = layoutRoom(d, opts(TextSize::Compact, 53));
  TEST_ASSERT_EQUAL(5, lines.size());
  TEST_ASSERT_TRUE(lines[0].divider);
  TEST_ASSERT_EQUAL_STRING("", lines[0].messageId.c_str());
  std::string div = flat(lines[0]);
  TEST_ASSERT_EQUAL(0, div.find("\xe2\x94\x80\xe2\x94\x80 Sat 25 Oct "));
  TEST_ASSERT_EQUAL(53, width(lines[0]));
  TEST_ASSERT_EQUAL_HEX16(defaultTheme().dim, lines[0].spans[0].fg);
  TEST_ASSERT_FALSE(lines[1].divider);
  TEST_ASSERT_FALSE(lines[2].divider);
  TEST_ASSERT_TRUE(lines[3].divider);
  TEST_ASSERT_TRUE(flat(lines[3]).find("Sun 26 Oct") != std::string::npos);
}

void test_comfortable_layout() {
  std::deque<CircMessage> d{msg("m1", "tardibyte", "hi"),
                            msg("m2", "genghis_khan_the_great", std::string(40, 'z'))};
  auto lines = body(layoutRoom(d, opts(TextSize::Comfortable, 40)));
  TEST_ASSERT_EQUAL_STRING("tardibyte  hi", flat(lines[0]).c_str());
  TEST_ASSERT_EQUAL_STRING("genghis_k\xe2\x80\xa6 zzzzzzzzzzzzzzzzzzzzzzzzzzzzz",
                           flat(lines[1]).c_str());
  TEST_ASSERT_EQUAL_STRING("           zzzzzzzzzzz", flat(lines[2]).c_str());
}

void test_comfortable_minute_divider() {
  std::deque<CircMessage> d{msg("m1", "a", "one"), msg("m2", "b", "two", kTs + 30000),
                            msg("m3", "c", "three", kTs + 60000)};
  auto lines = layoutRoom(d, opts(TextSize::Comfortable, 40));
  int dividers = 0;
  for (const auto& l : lines) dividers += l.divider;
  TEST_ASSERT_EQUAL(2, dividers);
  TEST_ASSERT_TRUE(lines[0].divider);
  TEST_ASSERT_EQUAL(0, flat(lines[0]).find("\xe2\x94\x80\xe2\x94\x80 14:02 "));
  TEST_ASSERT_EQUAL(40, width(lines[0]));
  TEST_ASSERT_TRUE(lines[3].divider);
  TEST_ASSERT_TRUE(flat(lines[3]).find("14:03") != std::string::npos);
}

void test_action_line() {
  CircMessage m = msg("m1", "rain", "slaps tardibyte around a bit with a large trout");
  m.isAction = true;
  std::deque<CircMessage> d{m};
  auto c = body(layoutRoom(d, opts(TextSize::Compact, 53)));
  TEST_ASSERT_EQUAL(0, flat(c[0]).find("14:02 * rain slaps tardibyte"));
  TEST_ASSERT_EQUAL_HEX16(defaultTheme().sys, c[0].spans.back().fg);
  auto f = body(layoutRoom(d, opts(TextSize::Comfortable, 40)));
  TEST_ASSERT_EQUAL(0, flat(f[0]).find("* rain slaps tardibyte"));
}

void test_deleted() {
  CircMessage m = msg("m1", "rain", "[DELETED]");
  m.deleted = true;
  std::deque<CircMessage> d{m};
  auto lines = body(layoutRoom(d, opts(TextSize::Compact, 53)));
  TEST_ASSERT_EQUAL(1, lines.size());
  TEST_ASSERT_EQUAL_STRING("14:02 rain: [deleted]", flat(lines[0]).c_str());
  TEST_ASSERT_EQUAL_HEX16(defaultTheme().dim, lines[0].spans.back().fg);
}

void test_spoiler_hidden_then_revealed() {
  CircMessage m = msg("m1", "rain", "snape kills");
  m.styles = {"spoiler"};
  std::deque<CircMessage> d{m};
  LayoutOptions o = opts(TextSize::Compact, 53);
  auto hidden = body(layoutRoom(d, o));
  TEST_ASSERT_TRUE(flat(hidden[0]).find("spoiler") != std::string::npos);
  TEST_ASSERT_TRUE(flat(hidden[0]).find("snape") == std::string::npos);
  std::set<std::string> revealed{"m1"};
  o.revealed = &revealed;
  auto shown = body(layoutRoom(d, o));
  TEST_ASSERT_EQUAL_STRING("14:02 rain: snape kills", flat(shown[0]).c_str());
}

void test_eightball_fortune() {
  CircMessage e = msg("m1", "rain", "will it work?");
  e.isEightball = true;
  e.eightballAnswer = "Ask again later";
  CircMessage f = msg("m2", "rain", "");
  f.isFortune = true;
  f.fortuneText = "You will ship.";
  std::deque<CircMessage> d{e, f};
  auto lines = body(layoutRoom(d, opts(TextSize::Compact, 53)));
  TEST_ASSERT_EQUAL_STRING(
      "14:02 rain: \xf0\x9f\x8e\xb1 will it work? \xe2\x86\x92 Ask again later",
      flat(lines[0]).c_str());
  TEST_ASSERT_EQUAL_STRING("14:02 rain: \xf0\x9f\xa5\xa0 You will ship.", flat(lines[1]).c_str());
}

void test_art_clipped() {
  // 70 x's then 10 y's, base64.
  CircMessage m = msg("m1", "rain",
                      "eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4"
                      "eHh4eHh4eHh4eHh4eHh4eAp5eXl5eXl5eXl5Cg==");
  m.styles = {"art"};
  std::deque<CircMessage> d{m};
  auto lines = body(layoutRoom(d, opts(TextSize::Compact, 53)));
  TEST_ASSERT_EQUAL(3, lines.size());
  TEST_ASSERT_EQUAL_STRING("14:02 rain:", flat(lines[0]).c_str());
  TEST_ASSERT_EQUAL_STRING(std::string(53, 'x').c_str(), flat(lines[1]).c_str());
  TEST_ASSERT_EQUAL_STRING("yyyyyyyyyy", flat(lines[2]).c_str());
  for (const auto& l : lines) TEST_ASSERT_TRUE(width(l) <= 53);
}

void test_image_only_no_double_url() {
  CircMessage m = msg("m1", "rain", "https://b.io/i.png");
  m.imageUrl = "https://b.io/i.png";
  CircMessage g = msg("m2", "rain", "look");
  g.gifUrl = "https://t.co/x.gif";
  CircMessage s = msg("m3", "rain", "");
  s.hasAudio = true;
  s.audio.artist = "Artist";
  s.audio.title = "Song";
  std::deque<CircMessage> d{m, g, s};
  auto lines = body(layoutRoom(d, opts(TextSize::Compact, 53)));
  TEST_ASSERT_EQUAL(4, lines.size());
  TEST_ASSERT_EQUAL_STRING("14:02 rain: [img] https://b.io/i.png", flat(lines[0]).c_str());
  TEST_ASSERT_EQUAL_STRING("14:02 rain: look", flat(lines[1]).c_str());
  TEST_ASSERT_EQUAL_STRING("            [gif] https://t.co/x.gif", flat(lines[2]).c_str());
  TEST_ASSERT_EQUAL_STRING("14:02 rain: [song] Artist \xe2\x80\x93 Song", flat(lines[3]).c_str());
}

void test_mention_tint() {
  std::deque<CircMessage> d{msg("m1", "rain", "hey @OuMike " + std::string(30, 'q')),
                            msg("m2", "rain", "hey all")};
  auto lines = body(layoutRoom(d, opts(TextSize::Compact, 53)));
  TEST_ASSERT_EQUAL(3, lines.size());
  TEST_ASSERT_TRUE(lines[0].mentionTint);
  TEST_ASSERT_TRUE(lines[1].mentionTint);
  TEST_ASSERT_FALSE(lines[2].mentionTint);
}

void test_self_gold() {
  std::deque<CircMessage> d{msg("m1", "oumike", "me")};
  auto lines = body(layoutRoom(d, opts(TextSize::Compact, 53)));
  TEST_ASSERT_EQUAL_HEX16(0xFE8C, lines[0].spans[1].fg);
}

void test_muted_skipped() {
  std::deque<CircMessage> d{msg("m1", "Troll", "spam"), msg("m2", "rain", "ok")};
  std::set<std::string> muted{"troll"};  // lower-cased
  LayoutOptions o = opts(TextSize::Compact, 53);
  o.muted = &muted;
  auto lines = body(layoutRoom(d, o));
  TEST_ASSERT_EQUAL(1, lines.size());
  TEST_ASSERT_EQUAL_STRING("m2", lines[0].messageId.c_str());
}

void test_long_no_space_message() {
  std::deque<CircMessage> d{msg("m1", "x", std::string(2048, 'a'))};
  auto lines = body(layoutRoom(d, opts(TextSize::Compact, 53)));
  // Body starts at 6 + 1 + 2 = 9, so 44 cells per line.
  const int w = 53 - 9;
  TEST_ASSERT_EQUAL((2048 + w - 1) / w, lines.size());
  for (const auto& l : lines) TEST_ASSERT_TRUE(width(l) <= 53);
}

void test_emoji_body() {
  std::deque<CircMessage> d{msg("m1", "rain", "\xf0\x9f\x8e\xb2\xf0\x9f\x8e\xb2")};
  auto lines = body(layoutRoom(d, opts(TextSize::Compact, 53)));
  TEST_ASSERT_EQUAL(1, lines.size());
  TEST_ASSERT_EQUAL(12 + 2, width(lines[0]));
}

// ---- Feed ----

static const int64_t kMin = 60 * 1000LL, kHour = 60 * kMin, kDay = 24 * kHour;

static Entry feedEntry() {
  Entry e;
  e.id = "e1";
  e.author = "tardibyte";
  e.title = "Hello world";
  e.body = "First line\n\nSecond paragraph about **spiders**.";
  e.createdMs = kTs - 3 * kHour;
  e.replies = 5;
  return e;
}

static EntryLayoutOptions entryOpts(bool revealed = false) {
  EntryLayoutOptions o;
  o.cols = 40;
  o.size = TextSize::Comfortable;
  o.nowMs = kTs;
  o.self = "oumike";
  o.revealed = revealed;
  return o;
}

static bool anyLineContains(const std::vector<DisplayLine>& ls, const std::string& s) {
  for (const auto& l : ls)
    if (flat(l).find(s) != std::string::npos) return true;
  return false;
}

void test_relative_age() {
  TEST_ASSERT_EQUAL_STRING("now", relativeAge(kTs - 30000, kTs).c_str());
  TEST_ASSERT_EQUAL_STRING("5m", relativeAge(kTs - 5 * kMin, kTs).c_str());
  TEST_ASSERT_EQUAL_STRING("3h", relativeAge(kTs - 3 * kHour, kTs).c_str());
  TEST_ASSERT_EQUAL_STRING("2d", relativeAge(kTs - 2 * kDay, kTs).c_str());
  TEST_ASSERT_EQUAL_STRING("25 Oct", relativeAge(kTs, kTs + 7 * kDay).c_str());
}

void test_feed_row() {
  Entry e = feedEntry();
  const Theme& t = defaultTheme();
  FeedRow r = layoutFeedRow(e, true, kTs, 40, t);
  TEST_ASSERT_TRUE(r.line1 == U"tardibyte \u00b7 3h \u00b7 \u21a95 \u2605");
  TEST_ASSERT_EQUAL_STRING("Hello world", u32ToUtf8(r.line2).c_str());
  TEST_ASSERT_EQUAL_HEX16(t.dim, r.fg2);
  TEST_ASSERT_EQUAL_HEX16(nickColor(defaultTheme(), "tardibyte", false), r.fg1);

  e.title.clear();
  r = layoutFeedRow(e, false, kTs, 40, t);
  TEST_ASSERT_TRUE(r.line1 == U"tardibyte \u00b7 3h \u00b7 \u21a95");
  TEST_ASSERT_EQUAL_STRING("First line", u32ToUtf8(r.line2).c_str());

  e.contentWarning = "spiders";
  r = layoutFeedRow(e, false, kTs, 40, t);
  TEST_ASSERT_TRUE(r.line2 == U"CW: spiders");

  e.contentWarning.clear();
  e.nsfw = true;
  r = layoutFeedRow(e, false, kTs, 40, t);
  TEST_ASSERT_TRUE(r.line2 == U"CW: NSFW");

  e = feedEntry();
  e.title = "A very long title that goes past twenty columns";
  r = layoutFeedRow(e, true, kTs, 20, t);
  TEST_ASSERT_TRUE(r.line1.size() <= 20);
  TEST_ASSERT_TRUE(r.line2.size() <= 20);

  // The header line can be cut narrower, for its larger font.
  e = feedEntry();
  r = layoutFeedRow(e, true, kTs, 40, t, 10);
  TEST_ASSERT_TRUE(r.line1.size() <= 10);
  TEST_ASSERT_EQUAL_STRING("Hello world", u32ToUtf8(r.line2).c_str());

  e.deleted = true;
  r = layoutFeedRow(e, false, kTs, 40, t);
  TEST_ASSERT_TRUE(r.line2 == U"[deleted]");
}

void test_self_gold_ignores_case() {
  std::deque<CircMessage> d{msg("m1", "OuMike", "me")};
  auto lines = body(layoutRoom(d, opts(TextSize::Compact, 53)));
  TEST_ASSERT_EQUAL_HEX16(0xFE8C, lines[0].spans[1].fg);
  Entry e = feedEntry();
  e.author = "OuMike";
  std::deque<Reply> none;
  auto ls = layoutEntry(e, none, entryOpts());
  TEST_ASSERT_EQUAL_HEX16(0xFE8C, ls[0].spans[0].fg);
}

void test_entry_layout_reveal() {
  Entry e = feedEntry();
  e.contentWarning = "spiders";
  e.topics = {"t1", "t2"};
  e.isPublic = true;
  std::deque<Reply> none;
  auto ls = layoutEntry(e, none, entryOpts(false));
  TEST_ASSERT_TRUE(flat(ls[0]) == "tardibyte \xc2\xb7 3h");
  TEST_ASSERT_EQUAL_STRING("#t1 #t2 \xc2\xb7 public", flat(ls[1]).c_str());
  TEST_ASSERT_FALSE(anyLineContains(ls, "Second paragraph"));
  TEST_ASSERT_TRUE(anyLineContains(ls, "CW: spiders"));
  for (const auto& l : ls)
    if (!l.divider) TEST_ASSERT_EQUAL_STRING("e1", l.messageId.c_str());
  ls = layoutEntry(e, none, entryOpts(true));
  TEST_ASSERT_TRUE(anyLineContains(ls, "Second paragraph"));
  TEST_ASSERT_FALSE(anyLineContains(ls, "CW: spiders"));
  for (const auto& l : ls) TEST_ASSERT_TRUE(width(l) <= 40);
}

void test_entry_layout_threading() {
  Entry e = feedEntry();
  std::deque<Reply> replies;
  Reply a;
  a.id = "r1";
  a.author = "rain";
  a.body = "top-level reply";
  a.createdMs = kTs - kHour;
  Reply b;
  b.id = "r2";
  b.parentId = "r1";
  b.parentAuthor = "tardibyte";
  b.author = "oumike";
  b.body = "nested reply";
  b.createdMs = kTs - kMin;
  replies.push_back(a);
  replies.push_back(b);
  auto ls = layoutEntry(e, replies, entryOpts());
  TEST_ASSERT_TRUE(anyLineContains(ls, "\xe2\x94\x80\xe2\x94\x80 5 replies \xe2\x94\x80\xe2\x94\x80"));
  int marker = -1, nested = -1;
  for (int i = 0; i < static_cast<int>(ls.size()); ++i) {
    if (flat(ls[i]) == "\xe2\x86\xb3 @tardibyte") marker = i;
    if (flat(ls[i]).find("nested reply") != std::string::npos) nested = i;
  }
  TEST_ASSERT_TRUE(marker >= 0);
  TEST_ASSERT_EQUAL(marker + 1, nested);
  TEST_ASSERT_EQUAL_STRING("r2", ls[marker].messageId.c_str());
  TEST_ASSERT_EQUAL_HEX16(defaultTheme().dim, ls[marker].spans[0].fg);
  TEST_ASSERT_EQUAL_STRING("r2", ls[nested].messageId.c_str());
  for (const auto& l : ls)
    if (flat(l).find("top-level reply") != std::string::npos)
      TEST_ASSERT_EQUAL_STRING("r1", l.messageId.c_str());
}

void test_entry_layout_truncated() {
  Entry e = feedEntry();
  e.truncated = true;
  e.attachments = {"[img] https://x/y.png"};
  std::deque<Reply> none;
  auto ls = layoutEntry(e, none, entryOpts());
  int last = -1;
  for (int i = 0; i < static_cast<int>(ls.size()); ++i)
    if (ls[i].messageId == "e1") last = i;
  TEST_ASSERT_TRUE(last >= 1);
  TEST_ASSERT_TRUE(flat(ls[last]) == "\xe2\x80\xa6 read the rest on the web");
  TEST_ASSERT_TRUE(flat(ls[last - 1]) == "[img] https://x/y.png");
}

void test_entry_layout_deleted() {
  Entry e = feedEntry();
  e.deleted = true;
  std::deque<Reply> replies;
  Reply r;
  r.id = "r1";
  r.author = "rain";
  r.deleted = true;
  r.createdMs = kTs;
  replies.push_back(r);
  auto ls = layoutEntry(e, replies, entryOpts());
  int n = 0;
  for (const auto& l : ls) {
    if (flat(l) == "[deleted]") {
      ++n;
      TEST_ASSERT_EQUAL_STRING("e1", l.messageId.c_str());
      TEST_ASSERT_EQUAL_HEX16(defaultTheme().dim, l.spans.back().fg);
    }
  }
  TEST_ASSERT_EQUAL(1, n);
  TEST_ASSERT_FALSE(anyLineContains(ls, "Second paragraph"));
  bool replyDeleted = false;
  for (const auto& l : ls)
    if (l.messageId == "r1" && flat(l).find("[deleted]") != std::string::npos) replyDeleted = true;
  TEST_ASSERT_TRUE(replyDeleted);
}

static const SplashLine* findRole(const std::vector<SplashLine>& v, SplashRole r) {
  for (const auto& l : v)
    if (l.role == r) return &l;
  return nullptr;
}

static void checkSplashFits(int cols, int rows) {
  auto v = splashLayout(cols, rows, "v0.1.0 \u00b7 M9");
  TEST_ASSERT_FALSE(v.empty());
  int top = rows, bottom = -1;
  for (const auto& l : v) {
    int w = static_cast<int>(utf8Length(l.text));
    TEST_ASSERT_TRUE(l.col >= 0);
    TEST_ASSERT_TRUE(l.col + w <= cols);
    TEST_ASSERT_TRUE(l.row >= 0 && l.row < rows);
    top = std::min(top, l.row);
    bottom = std::max(bottom, l.row);
  }
  // Vertically centred: the gaps above and below differ by at most one row.
  TEST_ASSERT_TRUE(std::abs(top - (rows - 1 - bottom)) <= 1);
  const SplashLine* title = findRole(v, SplashRole::Title);
  TEST_ASSERT_NOT_NULL(title);
  TEST_ASSERT_EQUAL_STRING("Platy for Cyberspace", title->text.c_str());
  int w = static_cast<int>(utf8Length(title->text));
  TEST_ASSERT_TRUE(std::abs(title->col - (cols - w - title->col)) <= 1);
  TEST_ASSERT_NOT_NULL(findRole(v, SplashRole::Detail));
  TEST_ASSERT_NOT_NULL(findRole(v, SplashRole::Prompt));
}

void test_splash_fits_both_text_sizes() {
  checkSplashFits(40, 15);
  checkSplashFits(53, 20);
}

void test_splash_art_is_one_block() {
  auto v = splashLayout(40, 15, "v0.1.0 \u00b7 M9");
  int col = -1, n = 0;
  for (const auto& l : v) {
    if (l.role != SplashRole::Art) continue;
    if (col < 0) col = l.col;
    TEST_ASSERT_EQUAL(col, l.col);  // shared left edge keeps the drawing intact
    ++n;
  }
  TEST_ASSERT_TRUE(n >= 4);
}

void test_splash_drops_art_when_too_small() {
  auto v = splashLayout(24, 6, "v0.1.0 \u00b7 WioL2");
  TEST_ASSERT_NULL(findRole(v, SplashRole::Art));
  TEST_ASSERT_NOT_NULL(findRole(v, SplashRole::Title));
  for (const auto& l : v) TEST_ASSERT_TRUE(l.col + static_cast<int>(utf8Length(l.text)) <= 24);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_grid_dims);
  RUN_TEST(test_big_cells);
  RUN_TEST(test_grid_row_kinds);
  RUN_TEST(test_row_spans_cut_before_last_row);
  RUN_TEST(test_row_spans_pad_rows);
  RUN_TEST(test_row_spans_shift);
  RUN_TEST(test_grid_add_edges);
  RUN_TEST(test_grid_dirty);
  RUN_TEST(test_grid_redraw_same_frame_not_dirty);
  RUN_TEST(test_time);
  RUN_TEST(test_compact_basic);
  RUN_TEST(test_compact_long_nick_indent);
  RUN_TEST(test_compact_day_divider_first);
  RUN_TEST(test_comfortable_layout);
  RUN_TEST(test_comfortable_minute_divider);
  RUN_TEST(test_action_line);
  RUN_TEST(test_deleted);
  RUN_TEST(test_spoiler_hidden_then_revealed);
  RUN_TEST(test_eightball_fortune);
  RUN_TEST(test_art_clipped);
  RUN_TEST(test_image_only_no_double_url);
  RUN_TEST(test_mention_tint);
  RUN_TEST(test_self_gold);
  RUN_TEST(test_self_gold_ignores_case);
  RUN_TEST(test_muted_skipped);
  RUN_TEST(test_long_no_space_message);
  RUN_TEST(test_emoji_body);
  RUN_TEST(test_relative_age);
  RUN_TEST(test_feed_row);
  RUN_TEST(test_entry_layout_reveal);
  RUN_TEST(test_entry_layout_threading);
  RUN_TEST(test_entry_layout_truncated);
  RUN_TEST(test_entry_layout_deleted);
  RUN_TEST(test_splash_fits_both_text_sizes);
  RUN_TEST(test_splash_art_is_one_block);
  RUN_TEST(test_splash_drops_art_when_too_small);
  return UNITY_END();
}
