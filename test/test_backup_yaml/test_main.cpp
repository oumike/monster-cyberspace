#include <unity.h>

#include <string>

#include "config/yaml_scalar.h"

using namespace rc;

void setUp() {}
void tearDown() {}

namespace {
bool parse(const char* in, std::string& out, ScalarKind& kind) {
  std::string err;
  return yamlScalar(in, out, kind, err);
}
void expectOk(const char* in, const char* want, ScalarKind wantKind) {
  std::string out;
  ScalarKind kind;
  TEST_ASSERT_TRUE_MESSAGE(parse(in, out, kind), in);
  TEST_ASSERT_EQUAL_STRING(want, out.c_str());
  TEST_ASSERT_TRUE(kind == wantKind);
}
}  // namespace

void test_quote_escapes() {
  TEST_ASSERT_EQUAL_STRING("\"a\\\\b\\\"c\\nd\\te\"", yamlQuote("a\\b\"c\nd\te").c_str());
  TEST_ASSERT_EQUAL_STRING("\"\\u0001\\u007F\"", yamlQuote("\x01\x7F").c_str());
  TEST_ASSERT_EQUAL_STRING("\"Café 📶\"", yamlQuote("Café 📶").c_str());
}

void test_scalar_round_trip() {
  const char* cases[] = {"", "x", "a\\b\"c\nd\te", "\x01", "Café 📶", "it's"};
  for (const char* c : cases) {
    std::string out, err;
    ScalarKind kind;
    TEST_ASSERT_TRUE_MESSAGE(yamlScalar(yamlQuote(c), out, kind, err), c);
    TEST_ASSERT_TRUE(kind == ScalarKind::Double);
    TEST_ASSERT_EQUAL_STRING(c, out.c_str());
  }
}

void test_scalar_plain_and_single() {
  expectOk("compact", "compact", ScalarKind::Plain);
  expectOk("  hello world  ", "hello world", ScalarKind::Plain);
  expectOk("'it''s'", "it's", ScalarKind::Single);
  expectOk("'a\\n'", "a\\n", ScalarKind::Single);
}

void test_scalar_comments_and_hash_in_quotes() {
  expectOk("\"Home\"  # main", "Home", ScalarKind::Double);
  expectOk("compact # small", "compact", ScalarKind::Plain);
  expectOk("\"a#b c#d\"", "a#b c#d", ScalarKind::Double);
  expectOk("'x # y'", "x # y", ScalarKind::Single);
  expectOk("a#b", "a#b", ScalarKind::Plain);
}

void test_scalar_errors() {
  const char* bad[] = {"\"open", "'open", "\"x\" junk", "\"\\q\"", "\"\\uD83D\""};
  for (const char* b : bad) {
    std::string out, err;
    ScalarKind kind;
    TEST_ASSERT_FALSE_MESSAGE(yamlScalar(b, out, kind, err), b);
    TEST_ASSERT_FALSE(err.empty());
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_quote_escapes);
  RUN_TEST(test_scalar_round_trip);
  RUN_TEST(test_scalar_plain_and_single);
  RUN_TEST(test_scalar_comments_and_hash_in_quotes);
  RUN_TEST(test_scalar_errors);
  return UNITY_END();
}
