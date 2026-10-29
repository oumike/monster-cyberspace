// The spell engine (ported from camillia-mt) and the suggestion strip, against
// a small list encoded here the way tools/gen_spell_dict.py encodes the real
// one: the generated English header exists only in firmware builds.
#include <unity.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "input/key_event.h"
#include "spell/spell.h"
#include "ui/widgets/input_line.h"
#include "ui/widgets/spell_strip.h"
#include "ui/widgets/text_area.h"

using namespace rc;

void setUp() {}
void tearDown() {}

// word, frequency 0..255 -- already scaled, unlike the generator's counts.
static const std::pair<const char*, int> kWords[] = {
    {"the", 255}, {"then", 180}, {"ten", 150}, {"them", 200}, {"there", 190},
    {"their", 185}, {"they", 210}, {"these", 170}, {"thief", 60}, {"tier", 40},
    {"receive", 120}, {"relieve", 80}, {"believe", 140}, {"message", 130},
    {"messages", 100}, {"manage", 90}, {"repeater", 120}, {"peter", 90},
    {"i'm", 230}, {"it", 250}, {"in", 245}, {"is", 248}, {"don't", 220},
    {"done", 160}, {"mike", 70}, {"node", 110}, {"nodes", 100}, {"none", 95},
    {"with", 240}, {"which", 200}, {"wish", 120}, {"a", 255}, {"an", 230},
    {"and", 252}, {"as", 200}, {"at", 210}, {"be", 220}, {"by", 190},
    {"cat", 90}, {"dog", 90}, {"eh", 50}, {"go", 180}, {"he", 230},
    {"hi", 150}, {"if", 200}, {"me", 230}, {"my", 225}, {"no", 230},
    {"of", 250}, {"on", 240}, {"or", 200}, {"so", 220}, {"to", 252},
    {"up", 200}, {"us", 180}, {"we", 230}, {"yes", 200}, {"you", 255},
    {"internationalization", 5}, {"zebra", 10},
};

struct Built {
  std::vector<uint8_t> data;
  std::vector<uint32_t> blocks;
  spell::Dict dict{};
};

static const spell::Dict& dict() {
  static Built b;
  if (!b.blocks.empty()) return b.dict;
  std::vector<std::pair<std::string, int>> ws;
  for (const auto& w : kWords) ws.emplace_back(w.first, w.second);
  std::sort(ws.begin(), ws.end());
  std::string prev;
  for (size_t i = 0; i < ws.size(); i++) {
    const std::string& w = ws[i].first;
    size_t p = 0;
    if (i % spell::kBlockWords == 0) {
      b.blocks.push_back((uint32_t)b.data.size());
    } else {
      while (p < std::min({w.size(), prev.size(), (size_t)15}) && w[p] == prev[p]) p++;
    }
    const size_t suffix = w.size() - p;
    if (suffix < 15) {
      b.data.push_back((uint8_t)((p << 4) | suffix));
    } else {
      b.data.push_back((uint8_t)((p << 4) | 15));
      b.data.push_back((uint8_t)suffix);
    }
    for (size_t k = p; k < w.size(); k++) b.data.push_back((uint8_t)w[k]);
    b.data.push_back((uint8_t)ws[i].second);
    prev = w;
  }
  b.dict = {b.data.data(), (uint32_t)b.data.size(), b.blocks.data(), (uint32_t)b.blocks.size(),
            (uint32_t)ws.size()};
  return b.dict;
}

static bool isKnown(const char* w) { return spell::known(dict(), w, strlen(w)); }

static std::string sugg(const char* w) {
  char out[spell::kMaxSuggest][spell::kMaxWord + 1];
  const int n = spell::suggest(dict(), w, strlen(w), out, spell::kMaxSuggest);
  std::string s;
  for (int i = 0; i < n; i++) {
    if (i) s += ' ';
    s += out[i];
  }
  return s;
}

void test_known() {
  TEST_ASSERT_TRUE(dict().blockCount > 1);
  for (const auto& w : kWords) TEST_ASSERT_TRUE_MESSAGE(isKnown(w.first), w.first);
  TEST_ASSERT_TRUE(isKnown("The"));
  TEST_ASSERT_TRUE(isKnown("Mike's"));
  TEST_ASSERT_FALSE(isKnown("teh"));
  TEST_ASSERT_FALSE(isKnown("aa"));
  TEST_ASSERT_FALSE(isKnown("zzz"));
  TEST_ASSERT_FALSE(isKnown("thei"));
  TEST_ASSERT_FALSE(isKnown("xyz's"));
}

void test_should_check() {
  auto chk = [](const char* w) { return spell::shouldCheck(w, strlen(w)); };
  TEST_ASSERT_TRUE(chk("hello"));
  TEST_ASSERT_TRUE(chk("Hello"));
  TEST_ASSERT_TRUE(chk("don't"));
  TEST_ASSERT_FALSE(chk("a"));
  TEST_ASSERT_FALSE(chk("LORA"));
  TEST_ASSERT_FALSE(chk("LongFast"));
  TEST_ASSERT_FALSE(chk("abc123"));
  TEST_ASSERT_FALSE(chk("'tis"));
  TEST_ASSERT_FALSE(chk("dogs'"));
  TEST_ASSERT_FALSE(chk("caf\xc3\xa9"));
  TEST_ASSERT_FALSE(chk("abcdefghijklmnopqrstuvwxyz"));
}

void test_suggest() {
  TEST_ASSERT_EQUAL_STRING("the ten eh", sugg("teh").c_str());
  TEST_ASSERT_EQUAL_STRING("The Ten Eh", sugg("Teh").c_str());
  TEST_ASSERT_EQUAL_STRING("THE TEN EH", sugg("TEH").c_str());
  TEST_ASSERT_EQUAL_STRING("receive relieve believe", sugg("recieve").c_str());
  TEST_ASSERT_EQUAL_STRING("repeater peter", sugg("repeter").c_str());
  TEST_ASSERT_EQUAL_STRING("message messages manage", sugg("mesage").c_str());
  TEST_ASSERT_EQUAL_STRING("I'm it is", sugg("im").c_str());
  TEST_ASSERT_EQUAL_STRING("don't done on", sugg("dont").c_str());
  TEST_ASSERT_EQUAL_STRING("node nodes none", sugg("nodez").c_str());
  TEST_ASSERT_EQUAL_STRING("", sugg("qqqqqqq").c_str());
  TEST_ASSERT_EQUAL_STRING("", sugg("zeb").c_str());  // no two-edit matches for 3 letters
  TEST_ASSERT_EQUAL_STRING("zebra", sugg("zebr").c_str());
  TEST_ASSERT_EQUAL_STRING("internationalization", sugg("internationalisation").c_str());

  char out[spell::kMaxSuggest][spell::kMaxWord + 1];
  TEST_ASSERT_EQUAL(1, spell::suggest(dict(), "teh", 3, out, 1));
  TEST_ASSERT_EQUAL_STRING("the", out[0]);
  TEST_ASSERT_EQUAL(0, spell::suggest(dict(), "teh", 3, out, 0));
  const char* longWord = "abcdefghijklmnopqrstuvwx";
  TEST_ASSERT_EQUAL(0, spell::suggest(dict(), longWord, strlen(longWord), out, 3));
}

// ---- the strip ----

static void type(InputLine& in, SpellStrip& s, const char32_t* text) {
  for (const char32_t* p = text; *p; ++p) {
    KeyEvent k{Action::Char, *p};
    if (s.key(k, in)) continue;
    in.insert(*p);
    s.typed(in, dict(), true);
  }
}

static KeyEvent act(Action a, int16_t index = -1) {
  KeyEvent k{a};
  k.index = index;
  return k;
}

void test_strip_opens_on_an_unknown_word_after_a_boundary() {
  InputLine in;
  SpellStrip s;
  type(in, s, U"teh");
  TEST_ASSERT_FALSE(s.visible());  // not finished yet
  type(in, s, U" ");
  TEST_ASSERT_TRUE(s.visible());
  TEST_ASSERT_EQUAL(4, s.count());  // as typed, then three suggestions
  TEST_ASSERT_TRUE(s.option(0) == U"teh");
  TEST_ASSERT_TRUE(s.option(1) == U"the");
  TEST_ASSERT_EQUAL(0, s.selected());
}

void test_strip_stays_shut() {
  InputLine in;
  SpellStrip s;
  type(in, s, U"the ");  // known
  TEST_ASSERT_FALSE(s.visible());
  type(in, s, U"qqqqqqq ");  // nothing close
  TEST_ASSERT_FALSE(s.visible());
  type(in, s, U"x/teh ");  // the tail of a path or URL
  TEST_ASSERT_FALSE(s.visible());
  in.clear();
  in.insert(U't'); in.insert(U'e'); in.insert(U'h');
  in.insert(U' ');
  s.typed(in, dict(), false);  // turned off
  TEST_ASSERT_FALSE(s.visible());
}

void test_strip_cycles_with_up_down_and_keeps_the_choice() {
  InputLine in;
  SpellStrip s;
  type(in, s, U"so teh,");
  TEST_ASSERT_TRUE(s.key(act(Action::Down), in));
  TEST_ASSERT_TRUE(in.text() == U"so the,");
  TEST_ASSERT_EQUAL(7, (int)in.cursor());  // still after the comma
  TEST_ASSERT_TRUE(s.key(act(Action::Down), in));
  TEST_ASSERT_TRUE(in.text() == U"so ten,");
  TEST_ASSERT_TRUE(s.key(act(Action::Up), in));
  TEST_ASSERT_TRUE(s.key(act(Action::Up), in));
  TEST_ASSERT_TRUE(in.text() == U"so teh,");
  TEST_ASSERT_TRUE(s.key(act(Action::Up), in));  // wraps round to the last
  TEST_ASSERT_TRUE(in.text() == U"so eh,");
  // Select keeps it and is used up: it doesn't also send.
  TEST_ASSERT_TRUE(s.key(act(Action::Select), in));
  TEST_ASSERT_FALSE(s.visible());
  TEST_ASSERT_TRUE(in.text() == U"so eh,");
}

void test_strip_untouched_select_falls_through_and_typing_closes_it() {
  InputLine in;
  SpellStrip s;
  type(in, s, U"teh ");
  TEST_ASSERT_FALSE(s.key(act(Action::Select), in));  // still Send
  TEST_ASSERT_FALSE(s.visible());
  type(in, s, U"recieve ");
  TEST_ASSERT_TRUE(s.visible());
  type(in, s, U"x");  // the next word: the strip goes, the key types
  TEST_ASSERT_FALSE(s.visible());
  TEST_ASSERT_TRUE(in.text() == U"teh recieve x");
}

void test_strip_tap_picks_and_closes() {
  InputLine in;
  SpellStrip s;
  type(in, s, U"I recieve ");
  TEST_ASSERT_TRUE(s.key(act(Action::Spell, 2), in));
  TEST_ASSERT_FALSE(s.visible());
  TEST_ASSERT_TRUE(in.text() == U"I relieve ");
  TEST_ASSERT_EQUAL(10, (int)in.cursor());
}

void test_strip_gives_up_when_the_word_changed() {
  InputLine in;
  SpellStrip s;
  type(in, s, U"teh ");
  in.setCursor(0);
  in.insert(U'x');  // e.g. a tap moved the caret and edited
  TEST_ASSERT_FALSE(s.key(act(Action::Down), in));  // the key does its usual job
  TEST_ASSERT_FALSE(s.visible());
  TEST_ASSERT_TRUE(in.text() == U"xteh ");
}

void test_strip_works_in_a_text_area() {
  TextArea body;
  SpellStrip s;
  for (char32_t c : std::u32string(U"hi\nteh\n")) {
    body.insert(c);
    s.typed(body, dict(), true);
  }
  // A newline doesn't end a word for the strip: Select there is a new line.
  TEST_ASSERT_FALSE(s.visible());
  for (char32_t c : std::u32string(U"teh.")) {
    body.insert(c);
    s.typed(body, dict(), true);
  }
  TEST_ASSERT_TRUE(s.visible());
  TEST_ASSERT_TRUE(s.key(act(Action::Down), body));
  TEST_ASSERT_TRUE(body.text() == U"hi\nteh\nthe.");
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_known);
  RUN_TEST(test_should_check);
  RUN_TEST(test_suggest);
  RUN_TEST(test_strip_opens_on_an_unknown_word_after_a_boundary);
  RUN_TEST(test_strip_stays_shut);
  RUN_TEST(test_strip_cycles_with_up_down_and_keeps_the_choice);
  RUN_TEST(test_strip_untouched_select_falls_through_and_typing_closes_it);
  RUN_TEST(test_strip_tap_picks_and_closes);
  RUN_TEST(test_strip_gives_up_when_the_word_changed);
  RUN_TEST(test_strip_works_in_a_text_area);
  return UNITY_END();
}
