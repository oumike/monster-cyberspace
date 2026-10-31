#include <unity.h>

#include "web/compose_req.h"

using namespace rc;

void setUp() {}
void tearDown() {}

static std::string check(const char* json, ComposeReq& r, bool openTakesText = true) {
  std::string err = parseCompose(json, r);
  if (err.empty()) err = checkCompose(r, openTakesText);
  return err;
}

void test_open_takes_the_text() {
  ComposeReq r;
  TEST_ASSERT_EQUAL_STRING("", check(R"({"kind":"open","text":"  hello  "})", r).c_str());
  TEST_ASSERT_EQUAL(int(ComposeKind::Open), int(r.kind));
  TEST_ASSERT_EQUAL_STRING("hello", r.text.c_str());
}

void test_open_needs_somewhere_to_send() {
  ComposeReq r;
  TEST_ASSERT_EQUAL_STRING("Nothing open on the device takes text",
                           check(R"({"kind":"open","text":"hi"})", r, false).c_str());
}

void test_text_newlines_become_spaces_for_chat() {
  ComposeReq r;
  check("{\"kind\":\"open\",\"text\":\"a\\nb\\r\\nc\\td\"}", r);
  TEST_ASSERT_EQUAL_STRING("a b  c d", r.text.c_str());
}

void test_empty_and_long_text_refused() {
  ComposeReq r;
  TEST_ASSERT_EQUAL_STRING("Text is empty", check(R"({"kind":"open","text":"   "})", r).c_str());
  std::string big = R"({"kind":"open","text":")" + std::string(4097, 'x') + "\"}";
  TEST_ASSERT_EQUAL_STRING("Text over 4096 characters", check(big.c_str(), r).c_str());
}

void test_mail_strips_the_at_and_needs_a_name() {
  ComposeReq r;
  TEST_ASSERT_EQUAL_STRING("", check(R"({"kind":"mail","to":" @Ann ","text":"hi"})", r, false).c_str());
  TEST_ASSERT_EQUAL(int(ComposeKind::Mail), int(r.kind));
  TEST_ASSERT_EQUAL_STRING("Ann", r.to.c_str());
  TEST_ASSERT_EQUAL_STRING("Recipient is empty", check(R"({"kind":"mail","to":"@","text":"hi"})", r).c_str());
  TEST_ASSERT_EQUAL_STRING("Recipient: a user name, no spaces",
                           check(R"({"kind":"mail","to":"a b","text":"hi"})", r).c_str());
}

void test_thread_keeps_newlines_and_checks_the_title() {
  ComposeReq r;
  TEST_ASSERT_EQUAL_STRING(
      "", check("{\"kind\":\"thread\",\"guild\":\" retro \",\"title\":\" Hi \",\"text\":\"a\\nb\"}", r, false)
              .c_str());
  TEST_ASSERT_EQUAL(int(ComposeKind::Thread), int(r.kind));
  TEST_ASSERT_EQUAL_STRING("retro", r.guild.c_str());
  TEST_ASSERT_EQUAL_STRING("Hi", r.title.c_str());
  TEST_ASSERT_EQUAL_STRING("a\nb", r.text.c_str());
  TEST_ASSERT_EQUAL_STRING("Guild is empty", check(R"({"kind":"thread","guild":"","text":"x"})", r).c_str());
  std::string longTitle = R"({"kind":"thread","guild":"g","title":")" + std::string(101, 't') + R"(","text":"x"})";
  TEST_ASSERT_EQUAL_STRING("Title over 100 characters", check(longTitle.c_str(), r).c_str());
}

void test_bad_requests() {
  ComposeReq r;
  TEST_ASSERT_EQUAL_STRING("Bad request", parseCompose("not json", r).c_str());
  TEST_ASSERT_EQUAL_STRING("Bad request", parseCompose(R"({"kind":"fax","text":"x"})", r).c_str());
  TEST_ASSERT_EQUAL_STRING("Bad request", parseCompose(R"({"kind":"open","text":5})", r).c_str());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_open_takes_the_text);
  RUN_TEST(test_open_needs_somewhere_to_send);
  RUN_TEST(test_text_newlines_become_spaces_for_chat);
  RUN_TEST(test_empty_and_long_text_refused);
  RUN_TEST(test_mail_strips_the_at_and_needs_a_name);
  RUN_TEST(test_thread_keeps_newlines_and_checks_the_title);
  RUN_TEST(test_bad_requests);
  return UNITY_END();
}
