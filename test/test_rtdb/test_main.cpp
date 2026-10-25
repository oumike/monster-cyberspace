#include <unity.h>

#include <string>
#include <vector>

#include "../fixtures/rtdb_fixtures.h"
#include "chat/chat_store.h"
#include "chat/presence.h"
#include "chat/typing.h"
#include "cs/rtdb.h"

using namespace rc;

void setUp() {}
void tearDown() {}

static SseEvent ev(const char* name, const char* data) { return {name, data}; }

static CircMessage seeded(const std::string& id, int64_t ts) {
  CircMessage m;
  m.id = id;
  m.timestamp = ts;
  m.content = "from history";
  return m;
}

void test_url_messages() {
  RtdbQuery q{true, 123, 1000};
  TEST_ASSERT_EQUAL_STRING(
      "https://x.firebasedatabase.app/chat_messages/general.json?auth=T"
      "&orderBy=%22timestamp%22&startAfter=123&limitToLast=1000",
      rtdbStreamUrl("https://x.firebasedatabase.app", "chat_messages/general", "T", q)
          .c_str());
}

void test_url_presence() {
  RtdbQuery q{false, -1, 0};
  TEST_ASSERT_EQUAL_STRING(
      "https://x.firebasedatabase.app/chat_presence/general.json?auth=T",
      rtdbStreamUrl("https://x.firebasedatabase.app", "chat_presence/general", "T", q)
          .c_str());
}

void test_window_put() {
  ChatStore s;
  s.upsert(seeded("-Nold", 1719600000000));
  TEST_ASSERT_EQUAL(StreamResult::Changed, applyChatEvent(s, ev("put", fx::kWindowPut)));
  // History loaded over REST survives the stream's initial window.
  TEST_ASSERT_EQUAL(3, s.size());
  const CircMessage* m1 = s.find("-Nm1");
  TEST_ASSERT_NOT_NULL(m1);
  TEST_ASSERT_EQUAL_STRING("u1", m1->userId.c_str());
  TEST_ASSERT_EQUAL_STRING("tardibyte", m1->username.c_str());
  TEST_ASSERT_EQUAL_STRING("anyone else on the 4.0 beta yet?", m1->content.c_str());
  TEST_ASSERT_TRUE(m1->timestamp == 1719700000000LL);
  const CircMessage* m2 = s.find("-Nm2");
  TEST_ASSERT_TRUE(m2->isDice);
  TEST_ASSERT_EQUAL_STRING("https://example.invalid/x.gif", m2->gifUrl.c_str());
  TEST_ASSERT_EQUAL_STRING("-Nm2", s.messages().back().id.c_str());
}

void test_null_window_put_keeps_history() {
  ChatStore s;
  s.upsert(seeded("-Nold", 1719600000000));
  TEST_ASSERT_EQUAL(StreamResult::Unchanged,
                    applyChatEvent(s, ev("put", fx::kNullWindowPut)));
  TEST_ASSERT_EQUAL(1, s.size());
}

void test_child_put() {
  ChatStore s;
  applyChatEvent(s, ev("put", fx::kWindowPut));
  TEST_ASSERT_EQUAL(StreamResult::Changed, applyChatEvent(s, ev("put", fx::kChildPut)));
  TEST_ASSERT_EQUAL(3, s.size());
  TEST_ASSERT_EQUAL_STRING("-Nm3", s.messages().back().id.c_str());
  TEST_ASSERT_EQUAL_STRING("hello @tardibyte", s.find("-Nm3")->content.c_str());
}

void test_null_put_removes() {
  ChatStore s;
  applyChatEvent(s, ev("put", fx::kWindowPut));
  TEST_ASSERT_EQUAL(StreamResult::Changed, applyChatEvent(s, ev("put", fx::kNullChildPut)));
  TEST_ASSERT_NULL(s.find("-Nm1"));
  TEST_ASSERT_EQUAL(1, s.size());
  TEST_ASSERT_EQUAL(StreamResult::Unchanged,
                    applyChatEvent(s, ev("put", fx::kNullChildPut)));
}

void test_delete_patch_tombstones() {
  ChatStore s;
  applyChatEvent(s, ev("put", fx::kWindowPut));
  TEST_ASSERT_EQUAL(StreamResult::Changed, applyChatEvent(s, ev("patch", fx::kDeletePatch)));
  const CircMessage* m = s.find("-Nm2");
  TEST_ASSERT_NOT_NULL(m);
  TEST_ASSERT_EQUAL_STRING("[DELETED]", m->content.c_str());
  TEST_ASSERT_TRUE(m->deleted);
  TEST_ASSERT_EQUAL_STRING("", m->gifUrl.c_str());
  TEST_ASSERT_FALSE(m->isDice);
  TEST_ASSERT_EQUAL(0, m->styles.size());
  TEST_ASSERT_EQUAL_STRING("genghis_khan", m->username.c_str());
  TEST_ASSERT_TRUE(m->timestamp == 1719700005000LL);
}

void test_window_patch_merges_and_adds() {
  ChatStore s;
  applyChatEvent(s, ev("put", fx::kWindowPut));
  TEST_ASSERT_EQUAL(StreamResult::Changed, applyChatEvent(s, ev("patch", fx::kWindowPatch)));
  TEST_ASSERT_EQUAL(3, s.size());
  TEST_ASSERT_EQUAL_STRING("edited", s.find("-Nm1")->content.c_str());
  TEST_ASSERT_EQUAL_STRING("tardibyte", s.find("-Nm1")->username.c_str());
  TEST_ASSERT_EQUAL_STRING("newbie", s.find("-Nm9")->username.c_str());
}

void test_style_array_and_string() {
  ChatStore s;
  applyChatEvent(s, ev("put", fx::kArtPut));
  const CircMessage* art = s.find("-Nart");
  TEST_ASSERT_EQUAL(1, art->styles.size());
  TEST_ASSERT_EQUAL_STRING("art", art->styles[0].c_str());
  TEST_ASSERT_EQUAL_STRING("IC9cXy9cCiggby5vICkKID4gXiA8", art->content.c_str());

  applyChatEvent(s, ev("put", fx::kStyleArrayPut));
  const CircMessage* m = s.find("-Nsty");
  TEST_ASSERT_EQUAL(2, m->styles.size());
  TEST_ASSERT_EQUAL_STRING("bold", m->styles[0].c_str());
  TEST_ASSERT_EQUAL_STRING("italic", m->styles[1].c_str());
  TEST_ASSERT_TRUE(m->hasAudio);
  TEST_ASSERT_EQUAL_STRING("https://example.invalid/a.mp3", m->audio.src.c_str());
  TEST_ASSERT_EQUAL_STRING("upload", m->audio.origin.c_str());
  TEST_ASSERT_EQUAL_STRING("A", m->audio.artist.c_str());
  TEST_ASSERT_EQUAL_STRING("T", m->audio.title.c_str());
  TEST_ASSERT_EQUAL_STRING("G", m->audio.genre.c_str());
}

void test_keepalive_cancel_revoked() {
  ChatStore s;
  TEST_ASSERT_EQUAL(StreamResult::KeepAlive, applyChatEvent(s, ev("keep-alive", "null")));
  TEST_ASSERT_EQUAL(StreamResult::Cancel, applyChatEvent(s, ev("cancel", "null")));
  TEST_ASSERT_EQUAL(StreamResult::AuthRevoked,
                    applyChatEvent(s, ev("auth_revoked", "credential is no longer valid")));
  PresenceMap p;
  TEST_ASSERT_EQUAL(StreamResult::KeepAlive, applyPresenceEvent(p, ev("keep-alive", "null")));
  TEST_ASSERT_EQUAL(StreamResult::AuthRevoked, applyPresenceEvent(p, ev("auth_revoked", "x")));
}

void test_bad_json() {
  ChatStore s;
  applyChatEvent(s, ev("put", fx::kWindowPut));
  uint32_t v = s.version();
  TEST_ASSERT_EQUAL(StreamResult::BadData, applyChatEvent(s, ev("put", "{\"path\":\"/\",\"data\":{")));
  TEST_ASSERT_EQUAL(StreamResult::BadData, applyChatEvent(s, ev("put", "{\"data\":{}}")));
  TEST_ASSERT_EQUAL(StreamResult::BadData,
                    applyChatEvent(s, ev("put", "{\"path\":\"/\",\"data\":{\"-Nx\":5}}")));
  TEST_ASSERT_EQUAL(2, s.size());
  TEST_ASSERT_EQUAL(v, s.version());
}

void test_presence_visible_and_idle() {
  PresenceMap p;
  TEST_ASSERT_EQUAL(StreamResult::Changed, applyPresenceEvent(p, ev("put", fx::kPresenceWindow)));
  TEST_ASSERT_EQUAL(4, p.size());
  auto vis = visiblePresence(p, 1000000, 180000);
  TEST_ASSERT_EQUAL(3, vis.size());
  TEST_ASSERT_EQUAL_STRING("ann", vis[0].username.c_str());
  TEST_ASSERT_EQUAL_STRING("Genghis_Khan", vis[1].username.c_str());
  TEST_ASSERT_EQUAL_STRING("tardibyte", vis[2].username.c_str());
  TEST_ASSERT_EQUAL_STRING("u2", vis[1].userId.c_str());
  TEST_ASSERT_TRUE(vis[1].isChatAdmin);
  TEST_ASSERT_FALSE(isIdle(vis[0], 1000000, 600000));  // no lastActivity
  TEST_ASSERT_TRUE(isIdle(vis[1], 1000000, 600000));
  TEST_ASSERT_FALSE(isIdle(vis[2], 1000000, 600000));

  TEST_ASSERT_EQUAL(StreamResult::Changed,
                    applyPresenceEvent(p, ev("put", R"({"path":"/u4","data":null})")));
  TEST_ASSERT_EQUAL(3, p.size());
  TEST_ASSERT_EQUAL(StreamResult::Changed,
                    applyPresenceEvent(p, ev("patch", R"({"path":"/u1","data":{"online":false}})")));
  TEST_ASSERT_FALSE(p["u1"].online);
  TEST_ASSERT_EQUAL_STRING("tardibyte", p["u1"].username.c_str());
  TEST_ASSERT_EQUAL(StreamResult::Changed,
                    applyPresenceEvent(p, ev("put", R"({"path":"/","data":null})")));
  TEST_ASSERT_EQUAL(0, p.size());
}

void test_typing_stream_and_names() {
  TypingMap t;
  TEST_ASSERT_EQUAL(StreamResult::Changed, applyTypingEvent(t, ev("put", fx::kTypingWindow)));
  TEST_ASSERT_EQUAL(2, int(t.size()));
  // Self is excluded case-insensitively; stale flags are dropped.
  auto names = typingNames(t, "OuMike", 1719700005000LL, 9000);
  TEST_ASSERT_EQUAL(1, int(names.size()));
  TEST_ASSERT_EQUAL_STRING("rain", names[0].c_str());
  TEST_ASSERT_EQUAL(0, int(typingNames(t, "oumike", 1719700009001LL, 9000).size()));
  TEST_ASSERT_EQUAL(StreamResult::Changed, applyTypingEvent(t, ev("patch", fx::kTypingStop)));
  TEST_ASSERT_FALSE(t["u-rain"].typing);
  TEST_ASSERT_EQUAL_STRING("rain", t["u-rain"].username.c_str());
  TEST_ASSERT_EQUAL(0, int(typingNames(t, "oumike", 1719700001000LL, 9000).size()));
  TEST_ASSERT_EQUAL(StreamResult::Changed, applyTypingEvent(t, ev("put", fx::kTypingGone)));
  TEST_ASSERT_EQUAL(1, int(t.size()));
}

void test_dm_child_put_sender_aliases() {
  ChatStore s;
  TEST_ASSERT_EQUAL(StreamResult::Changed, applyChatEvent(s, ev("put", fx::kDmChildPut)));
  const CircMessage* m = s.find("-Dm9");
  TEST_ASSERT_NOT_NULL(m);
  TEST_ASSERT_EQUAL_STRING("rain", m->username.c_str());
  TEST_ASSERT_EQUAL_STRING("u-rain", m->userId.c_str());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_url_messages);
  RUN_TEST(test_url_presence);
  RUN_TEST(test_window_put);
  RUN_TEST(test_null_window_put_keeps_history);
  RUN_TEST(test_child_put);
  RUN_TEST(test_null_put_removes);
  RUN_TEST(test_delete_patch_tombstones);
  RUN_TEST(test_window_patch_merges_and_adds);
  RUN_TEST(test_style_array_and_string);
  RUN_TEST(test_keepalive_cancel_revoked);
  RUN_TEST(test_bad_json);
  RUN_TEST(test_presence_visible_and_idle);
  RUN_TEST(test_typing_stream_and_names);
  RUN_TEST(test_dm_child_put_sender_aliases);
  return UNITY_END();
}
