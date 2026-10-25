#include <unity.h>

#include <cstring>
#include <string>
#include <vector>

#include "net/sse_parser.h"

using namespace rc;

void setUp() {}
void tearDown() {}

static std::vector<SseEvent> parse(SseParser& p, const std::string& s) {
  std::vector<SseEvent> out;
  p.feed(s.data(), s.size(), [&](const SseEvent& e) { out.push_back(e); });
  return out;
}

static std::vector<SseEvent> parse(const std::string& s) {
  SseParser p;
  return parse(p, s);
}

static const char* kPut = "{\"path\":\"/\",\"data\":null}";

void test_single_event() {
  auto ev = parse(std::string("event: put\ndata: ") + kPut + "\n\n");
  TEST_ASSERT_EQUAL(1, ev.size());
  TEST_ASSERT_EQUAL_STRING("put", ev[0].event.c_str());
  TEST_ASSERT_EQUAL_STRING(kPut, ev[0].data.c_str());
}

void test_crlf() {
  auto ev = parse(std::string("event: put\r\ndata: ") + kPut + "\r\n\r\n");
  TEST_ASSERT_EQUAL(1, ev.size());
  TEST_ASSERT_EQUAL_STRING("put", ev[0].event.c_str());
  TEST_ASSERT_EQUAL_STRING(kPut, ev[0].data.c_str());
}

void test_multiline_data() {
  auto ev = parse("data: one\ndata:two\n\n");
  TEST_ASSERT_EQUAL(1, ev.size());
  TEST_ASSERT_EQUAL_STRING("one\ntwo", ev[0].data.c_str());
}

void test_every_split_point() {
  const std::string stream =
      "event: put\r\ndata: {\"a\":1}\r\n\r\n"
      ": c\r\nevent: patch\r\ndata: x\r\ndata: y\r\n\r\n";
  for (size_t i = 0; i <= stream.size(); ++i) {
    SseParser p;
    std::vector<SseEvent> ev;
    auto h = [&](const SseEvent& e) { ev.push_back(e); };
    p.feed(stream.data(), i, h);
    p.feed(stream.data() + i, stream.size() - i, h);
    TEST_ASSERT_EQUAL_MESSAGE(2, ev.size(), std::to_string(i).c_str());
    TEST_ASSERT_EQUAL_STRING("put", ev[0].event.c_str());
    TEST_ASSERT_EQUAL_STRING("{\"a\":1}", ev[0].data.c_str());
    TEST_ASSERT_EQUAL_STRING("patch", ev[1].event.c_str());
    TEST_ASSERT_EQUAL_STRING("x\ny", ev[1].data.c_str());
  }
}

void test_keepalive() {
  auto ev = parse("event: keep-alive\ndata: null\n\n");
  TEST_ASSERT_EQUAL(1, ev.size());
  TEST_ASSERT_EQUAL_STRING("keep-alive", ev[0].event.c_str());
  TEST_ASSERT_EQUAL_STRING("null", ev[0].data.c_str());
}

void test_comment_ignored() {
  auto ev = parse(": hello\nevent: put\n: mid\ndata: z\n\n: tail\n\n");
  TEST_ASSERT_EQUAL(1, ev.size());
  TEST_ASSERT_EQUAL_STRING("put", ev[0].event.c_str());
  TEST_ASSERT_EQUAL_STRING("z", ev[0].data.c_str());
}

void test_overflow() {
  SseParser p;
  p.maxEventBytes = 16;
  auto ev = parse(p, "event: put\ndata: " + std::string(100, 'x') + "\n\n");
  TEST_ASSERT_EQUAL(0, ev.size());
  TEST_ASSERT_TRUE(p.overflowed());
  ev = parse(p, "data: ok\n\n");
  TEST_ASSERT_EQUAL(1, ev.size());
  TEST_ASSERT_EQUAL_STRING("ok", ev[0].data.c_str());
  TEST_ASSERT_EQUAL_STRING("", ev[0].event.c_str());
  TEST_ASSERT_FALSE(p.overflowed());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_single_event);
  RUN_TEST(test_crlf);
  RUN_TEST(test_multiline_data);
  RUN_TEST(test_every_split_point);
  RUN_TEST(test_keepalive);
  RUN_TEST(test_comment_ignored);
  RUN_TEST(test_overflow);
  return UNITY_END();
}
