# Rhino Cyberspace M3 (Feed) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a Feed to the M9 client:

- read and auto-refresh the feed;
- open an entry with its replies;
- reply, write entries, bookmark, watch, delete and report;
- alert on replies and mentions.

**Architecture:** This follows the C-Mail pattern.

- Portable parsing, Markdown-to-text, compose, store and layout code lives in
  `lib/core/src/feed/` and `lib/core/src/ui/`, with native tests.
- On the device:
  - a `FeedSource` (net or fake) sends commands;
  - a `FeedWorker` inside the net task does the HTTP calls;
  - `LockedFeed` shares the state;
  - five new screens show it.

**Tech Stack:** C++17, pioarduino (Arduino 3.3 / ESP-IDF 5.5), ArduinoJson 7, Unity (native tests).

**Spec:** `docs/superpowers/specs/2026-10-26-cyberspace-m9-feed-design.md` (and M1 §2, §4, §9).

## Global Constraints

- Never call the live API (`api.cyberspace.online` or its RTDB) from tests, scripts or an agent. Fixtures are hand-written from the documented shapes.
- More boards will follow, so keep Cyberspace code board-agnostic. Parsing, store, layout and widget logic stay in `lib/core` (no Arduino, no board headers). Device code outside `src/hal/` touches hardware only through `src/hal/board.h` (`board::panelWidth()`, `board::beep()`, …) and never includes `src/hal/<board>/` headers or hard-codes panel size, pins or key codes. Board-specific values belong behind the HAL.
- Every write comes from a key press. Nothing is retried, apart from `CsApi::call`'s existing single repeat after a 401 refresh.
- Never log passwords or tokens.
- Device caps:
  - feed `limit=10`;
  - a list preview of 280 code points;
  - an open entry body of 16384 bytes;
  - reply pages of 20, at most 100 held, each body at most 4096 bytes;
  - a composed body of at most 4096 code points.
- Client limits:
  - entries 2/min, 24/day;
  - replies 3/min, 48/day;
  - bookmarks 5/min, 75/day;
  - watch 10/min, 100/day;
  - flags use the existing shared `flagL_`.
- Auto-refresh is every 120000 ms, only while FeedList is the current screen and the screen is on.
- The notification poll's type filter is exactly `chat_mention,reply,thread_reply,post_mention,reply_mention,post_cooldown`.
- Feed alerts beep at 1800 Hz for 80 ms when the buzzer is on. A `post_cooldown` alert shows a banner only.
- Never call `feedSource().command()` inside `lockedFeed().with(...)`.
- Menus and lists wrap round: Up on the first item selects the last, Down on the last selects the first (`ListView`, `Drawer`, `EntryList`). A paged list loads its next page from the last row before wrapping. Text views (logs, entry body) and form fields do not wrap.
- Each task follows TDD where it has host tests, then commits with the given message and the trailer `Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>`.
- Verify:
  - `pio test -e native` is green;
  - from Task 6 on, `pio run -e m9` and `pio run -e m9_fake` both build.

## Review Focus

1. **More than 10 new entries since the last refresh:** the first page doesn't overlap the held list. Expected: the list is replaced, with no hidden gap (Task 4: `test_first_page_without_overlap_replaces`).
2. **Truncation never splits a UTF-8 sequence:** a cut inside a 4-byte emoji or a CJK character still gives valid UTF-8 (Task 2: `test_truncate_utf8_boundary`).
3. **Deleted or sparse items:** `deleted:true` with empty content, no title, slug or topics, and an unknown attachment type. Expected: they parse and render `[deleted]` or `[attachment]` without crashing (Task 1: `test_parse_sparse_and_deleted`; Task 5: `test_entry_layout_deleted`).
4. **An oversized response:** the device falls back from `limit` 10 to 5 to 2, then stops with a banner. It never loops (Task 2: `test_next_smaller_limit`).
5. **Sign-out, then sign-in as someone else:** the bookmark map, feed and alerts are cleared, so no stale `★` marks or deletes are offered (Task 4: `test_clear_drops_everything`). A body made only of whitespace is rejected (Task 3: `test_whitespace_body_rejected`).

---

### Task 1: Feed types and response parsing

**Files:**
- Create: `lib/core/src/feed/entry.h`, `lib/core/src/feed/feed_parse.{h,cpp}`
- Create: `test/fixtures/feed_fixtures.h`, `test/test_feed_parse/test_main.cpp`

**Interfaces:**
- Produces, in `entry.h` (namespace `rc`):
  ```cpp
  struct Entry {
    std::string id, author, title, slug, contentWarning, body, guildSlug;
    std::vector<std::string> topics, attachments;  // attachments as display text
    int64_t createdMs = -1;
    int replies = 0;
    bool isPublic = false, nsfw = false, deleted = false, edited = false, truncated = false;
  };
  struct Reply {
    std::string id, postId, parentId, parentAuthor, author, body;
    std::vector<std::string> attachments;
    int64_t createdMs = -1;
    bool deleted = false, edited = false, truncated = false;
  };
  struct Bookmark { std::string id, postId, replyId; };  // id = bookmarkId
  ```
- Produces, in `feed_parse.h` (for a list, `cursor` is "" when null or absent):
  ```cpp
  int64_t parseIsoMs(std::string_view iso);  // UTC "2026-03-27T10:12:01.516Z" → epoch ms; -1 if malformed
  bool parseFeedPage(std::string_view body, std::vector<Entry>& out, std::string& cursor);
  bool parseEntry(std::string_view body, Entry& out);
  bool parseReplies(std::string_view body, std::vector<Reply>& out, std::string& cursor);
  bool parseReply(std::string_view body, Reply& out);
  // {"data":{postId|replyId|bookmarkId: ...}} → the first present of those keys.
  bool parseCreatedId(std::string_view body, std::string& id);
  bool parseBookmarks(std::string_view body, std::vector<Bookmark>& out, std::string& cursor);
  bool parseWatches(std::string_view body, std::vector<std::string>& postIds, std::string& cursor);
  bool parseWatching(std::string_view body, bool& watching);
  ```
- Mapping:
  - `postId`→`id`, `authorUsername`→`author`, `content`→`body`, `isNSFW`→`nsfw`.
  - `editedAt` present → `edited`.
  - Reply: `replyId`→`id`, `parentReplyId`→`parentId`, `parentReplyAuthor`→`parentAuthor`.
  - Attachment display text:
    - audio → `"[song] artist – title"`, or `"[song]"` when either is missing;
    - image → `"[img]"`;
    - anything else → `"[attachment]"`.

- [x] **Step 1: Write the fixtures and failing tests**

  Fixtures in `feed_fixtures.h`, namespace `fx`, hand-written from the doc shapes with the header comment used in `api_fixtures.h`:
  - `kFeedPage`: 3 entries, one with title, topics `["music","linux"]`, `isNSFW:true` and `contentWarning:"spiders"`; `cursor:"p3"`.
  - `kFeedLast`: 1 entry, `cursor:null`.
  - `kEntry`: one entry with an audio attachment `{type:"audio",artist:"Boards",title:"Roygbiv"}` and `editedAt`.
  - `kEntrySparse`: `{"data":{"postId":"p9","authorId":"u","authorUsername":"x","content":"","repliesCount":0,"bookmarksCount":0,"deleted":true,"createdAt":"2026-03-27T10:12:01.516Z","attachments":[{"type":"video","src":"s"}]}}`.
  - `kReplies`: 3 replies, the third with `parentReplyId:"r1"` and `parentReplyAuthor:"tardibyte"`; `cursor:"r3"`.
  - `kReply`.
  - `kCreatedPost`: `{postId:"p10",slug:"hello"}`.
  - `kCreatedReply`.
  - `kCreatedBookmark`.
  - `kBookmarks`: one post bookmark and one reply bookmark.
  - `kWatches`.
  - `kWatching`: `true`.

  Tests:
  ```cpp
  void test_iso_ms() {
    TEST_ASSERT_EQUAL_INT64(1774606321516LL, parseIsoMs("2026-03-27T10:12:01.516Z"));
    TEST_ASSERT_EQUAL_INT64(1774606321000LL, parseIsoMs("2026-03-27T10:12:01Z"));
    TEST_ASSERT_EQUAL_INT64(-1, parseIsoMs("yesterday"));
  }
  void test_parse_feed_page() {
    // 3 entries, cursor "p3"; entry 2 has title, topics[1]=="linux", nsfw, cw "spiders"
    // kFeedLast → cursor ""
  }
  void test_parse_entry_attachments_and_edit() {
    // attachments[0]=="[song] Boards – Roygbiv"; edited true
  }
  void test_parse_sparse_and_deleted() {
    Entry e; TEST_ASSERT_TRUE(parseEntry(fx::kEntrySparse, e));
    TEST_ASSERT_TRUE(e.deleted); TEST_ASSERT_TRUE(e.title.empty());
    TEST_ASSERT_EQUAL(0, (int)e.topics.size());
    TEST_ASSERT_EQUAL_STRING("[attachment]", e.attachments[0].c_str());
  }
  void test_parse_replies_threading() {
    // replies[2].parentId=="r1", parentAuthor=="tardibyte"; cursor "r3"
  }
  void test_parse_created_ids()       // "p10", the reply id and the bookmark id
  void test_parse_bookmarks_watches() // post vs reply bookmark fields; watch postIds; watching==true
  void test_parse_rejects_garbage()   // every parse* returns false on "<html>" and on {"data":null}
  ```
- [x] **Step 2: Run** `pio test -e native -f test_feed_parse`. Expected: it fails to build (the headers are missing).
- [x] **Step 3: Implement** `entry.h` and `feed_parse.cpp` with ArduinoJson. Reuse the `load()` pattern from `lib/core/src/cs/api_parse.cpp`. `parseIsoMs` uses days-from-civil arithmetic, not `mktime` (it is UTC and must not depend on the time zone).
- [x] **Step 4: Run** `pio test -e native -f test_feed_parse`. Expected: PASS.
- [x] **Step 5: Commit** `feat(core): Feed types and response parsing`.

### Task 2: Markdown to text, truncation, limit fallback

**Files:**
- Create: `lib/core/src/feed/markdown_text.{h,cpp}`, `test/test_feed_text/test_main.cpp`

**Interfaces:**
- Produces:
  ```cpp
  // Plain text with '\n' line breaks, ready for wrapText().
  std::u32string markdownToText(std::string_view md);
  // Cuts s to at most maxBytes without splitting a UTF-8 sequence. True if it cut.
  bool truncateUtf8(std::string& s, size_t maxBytes);
  // Cuts to at most n code points. True if it cut.
  bool truncateCodePoints(std::string& s, size_t n);
  // 10 → 5 → 2 → 0 (0 = give up).
  int nextSmallerLimit(int limit);
  ```
- Rules (spec §3):
  - Headings: `#`…`######` and the space after them are removed.
  - `**`, `__`, `*`, `_` and `~~` emphasis markers are removed, but not inside code. A lone `*` in `2 * 3` stays.
  - `[t](u)` → `t <u>`; `![a](u)` → `[img: a]`; `![](u)` → `[img]`.
  - Inline code keeps its text without the backticks. Fenced blocks keep their lines verbatim and drop the fence lines.
  - `- `, `* ` and `+ ` bullets → `• `; `> ` → `│ `.
  - More than one blank line in a row collapses to one.

- [x] **Step 1: Write the failing tests**
  ```cpp
  void test_md_headings_emphasis() {
    TEST_ASSERT_TRUE(U"Title\nbold and it" == markdownToText("## Title\n**bold** and *it*"));
    TEST_ASSERT_TRUE(U"2 * 3" == markdownToText("2 * 3"));
  }
  void test_md_links_images() {
    TEST_ASSERT_TRUE(U"see site <https://a.b>" == markdownToText("see [site](https://a.b)"));
    TEST_ASSERT_TRUE(U"[img: cat] [img]" == markdownToText("![cat](https://x/c.png) ![](https://x/d.png)"));
  }
  void test_md_code_kept_verbatim() {
    TEST_ASSERT_TRUE(U"a **b**\nx_y" == markdownToText("```\na **b**\n```\n`x_y`"));
  }
  void test_md_lists_quotes_blank_lines() {
    TEST_ASSERT_TRUE(U"• one\n• two\n│ quoted\n\nend" ==
                     markdownToText("- one\n* two\n> quoted\n\n\n\nend"));
  }
  void test_md_utf8_and_empty() {
    TEST_ASSERT_TRUE(U"héllo 🦉" == markdownToText("_héllo_ 🦉"));
    TEST_ASSERT_TRUE(U"" == markdownToText(""));
  }
  void test_truncate_utf8_boundary() {
    std::string s = "ab🦉";                   // 2 + 4 bytes
    TEST_ASSERT_TRUE(truncateUtf8(s, 4));
    TEST_ASSERT_EQUAL_STRING("ab", s.c_str());
    std::string t = "ok"; TEST_ASSERT_FALSE(truncateUtf8(t, 16));
    std::string u = "日本語"; TEST_ASSERT_TRUE(truncateCodePoints(u, 2));
    TEST_ASSERT_EQUAL_STRING("日本", u.c_str());
  }
  void test_next_smaller_limit() {
    TEST_ASSERT_EQUAL(5, nextSmallerLimit(10));
    TEST_ASSERT_EQUAL(2, nextSmallerLimit(5));
    TEST_ASSERT_EQUAL(0, nextSmallerLimit(2));
  }
  ```
- [x] **Step 2: Run** `pio test -e native -f test_feed_text`. Expected: FAIL.
- [x] **Step 3: Implement.** Work line by line with a "fenced" flag. Use `lib/core/src/util/utf8.h` to decode. Handle inline markup with a single left-to-right scan.
- [x] **Step 4: Run** the tests. Expected: PASS.
- [x] **Step 5: Commit** `feat(core): Markdown to plain text and truncation`.

### Task 3: Compose and Feed rate limits

**Files:**
- Create: `lib/core/src/feed/compose.{h,cpp}`, `test/test_feed_compose/test_main.cpp`
- Modify: `lib/core/src/cs/rate_limiter.{h,cpp}`, `test/test_limits_token/test_main.cpp`

**Interfaces:**
- Produces:
  ```cpp
  struct EntryDraft {
    std::string title, body, topics, cw;  // topics: comma-separated as typed
    bool isPublic = false, nsfw = false;
  };
  constexpr size_t kBodyMaxChars = 4096;
  // Trims; strips a leading '#'; lower-cases ASCII; drops empties.
  std::vector<std::string> splitTopics(std::string_view s);
  // "" when valid, else the message to show.
  std::string validateDraft(const EntryDraft& d);
  // Leaves out an empty title, empty topics and an empty cw; leaves out
  // isPublic and isNSFW when false.
  std::string entryJson(const EntryDraft& d);
  std::string replyJson(const std::string& postId, const std::string& body,
                        const std::string& parentReplyId);  // parentReplyId only when non-empty
  std::string flagJson(const std::string& reason);           // {} when reason is empty
  std::string bookmarkJson(const std::string& postId, const std::string& replyId);  // type "post" or "reply"
  RateLimiter makeEntryLimiter();     // {2, 60000}, {24, 86400000}
  RateLimiter makeReplyLimiter();     // {3, 60000}, {48, 86400000}
  RateLimiter makeBookmarkLimiter();  // {5, 60000}, {75, 86400000}
  RateLimiter makeWatchLimiter();     // {10, 60000}, {100, 86400000}
  ```
- Messages (exact copy), checked in this order:
  1. `"Body is empty"`;
  2. `"Body over 4096 characters"`;
  3. `"Title over 100 characters"`;
  4. `"At most 3 topics"`;
  5. `"Warning over 60 characters"`.

  Lengths count code points.

- [x] **Step 1: Write the failing tests**
  ```cpp
  void test_split_topics() {
    auto t = splitTopics(" Music, #Linux ,, ");
    TEST_ASSERT_EQUAL(2, (int)t.size());
    TEST_ASSERT_EQUAL_STRING("music", t[0].c_str());
    TEST_ASSERT_EQUAL_STRING("linux", t[1].c_str());
  }
  void test_validate_limits() {
    EntryDraft d; d.body = "hi";
    TEST_ASSERT_EQUAL_STRING("", validateDraft(d).c_str());
    d.title = std::string(101, 'a');
    TEST_ASSERT_EQUAL_STRING("Title over 100 characters", validateDraft(d).c_str());
    d.title = ""; d.topics = "a,b,c,d";
    TEST_ASSERT_EQUAL_STRING("At most 3 topics", validateDraft(d).c_str());
    d.topics = ""; d.cw = std::string(61, 'w');
    TEST_ASSERT_EQUAL_STRING("Warning over 60 characters", validateDraft(d).c_str());
    d.cw = ""; d.body.clear(); for (int i = 0; i < 4096; ++i) d.body += "é";
    TEST_ASSERT_EQUAL_STRING("", validateDraft(d).c_str());  // 4096 code points, 8192 bytes
    d.body += "é";
    TEST_ASSERT_EQUAL_STRING("Body over 4096 characters", validateDraft(d).c_str());
  }
  void test_whitespace_body_rejected() {
    EntryDraft d; d.body = " \n\t ";
    TEST_ASSERT_EQUAL_STRING("Body is empty", validateDraft(d).c_str());
  }
  void test_entry_json_omits_empties() {
    EntryDraft d; d.body = "hi";
    TEST_ASSERT_EQUAL_STRING("{\"content\":\"hi\"}", entryJson(d).c_str());
    d.title = "T"; d.topics = "Music"; d.cw = "cw"; d.isPublic = true; d.nsfw = true;
    // parse the result with ArduinoJson: title "T", topics ["music"], contentWarning "cw",
    // isPublic true, isNSFW true
  }
  void test_reply_flag_bookmark_json() {
    TEST_ASSERT_EQUAL_STRING("{\"postId\":\"p1\",\"content\":\"yo\"}", replyJson("p1", "yo", "").c_str());
    // with "r1": also has "parentReplyId":"r1"
    TEST_ASSERT_EQUAL_STRING("{}", flagJson("").c_str());
    TEST_ASSERT_EQUAL_STRING("{\"postId\":\"p1\",\"type\":\"post\"}", bookmarkJson("p1", "").c_str());
    TEST_ASSERT_EQUAL_STRING("{\"replyId\":\"r1\",\"type\":\"reply\"}", bookmarkJson("", "r1").c_str());
  }
  ```
  In `test_limits_token`, add `test_feed_limiters`:
  - `makeEntryLimiter` allows 2 acquires at t=0, refuses the 3rd, and allows one at t=60000;
  - after 24 acquires spread across a day it refuses until 86400000 has passed;
  - the same shape for reply (3/48), bookmark (5/75) and watch (10/100) at the per-minute edge.
- [x] **Step 2: Run** `pio test -e native -f test_feed_compose -f test_limits_token`. Expected: FAIL.
- [x] **Step 3: Implement.** `compose` uses ArduinoJson. Add the limiters to `rate_limiter.{h,cpp}` next to `makeTypingLimiter`.
- [x] **Step 4: Run** the tests. Expected: PASS.
- [x] **Step 5: Commit** `feat(core): entry and reply compose, Feed rate limits`.

### Task 4: Feed store

**Files:**
- Create: `lib/core/src/feed/feed_store.{h,cpp}`, `test/test_feed_store/test_main.cpp`

**Interfaces:**
- Consumes: Task 1 types, and `truncateUtf8`/`truncateCodePoints` from Task 2.
- Produces:
  ```cpp
  constexpr size_t kPreviewChars = 280, kEntryBodyBytes = 16384, kReplyBodyBytes = 4096;
  constexpr size_t kMaxReplies = 100;
  class FeedStore {
   public:
    // Feed list. Bodies are cut to kPreviewChars (truncated=true when cut).
    // First page: if any id is already held, new ids go on top in page order
    // and held ones are updated in place, keeping the old cursor; with no
    // overlap the list is replaced and the cursor taken from the page.
    void mergeFirstPage(std::vector<Entry> page, const std::string& cursor);
    void appendPage(std::vector<Entry> page, const std::string& cursor);  // skips held ids
    const std::vector<Entry>& feed() const;
    bool feedEnd() const;            // cursor "" after at least one page
    const std::string& feedCursor() const;
    int indexOf(const std::string& postId) const;  // -1 when absent

    // Open entry (body cut to kEntryBodyBytes) and its replies.
    void openEntry(Entry e);         // clears replies, cursor and watching
    void closeEntry();               // frees the entry and its replies
    const Entry* entry() const;      // nullptr when none
    // Skips held ids and cuts bodies to kReplyBodyBytes. Over kMaxReplies,
    // drops the oldest.
    void appendReplies(std::vector<Reply> page, const std::string& cursor);
    const std::deque<Reply>& replies() const;
    bool repliesEnd() const;
    // The cursor if set, else the newest held reply's id ("" when none).
    std::string repliesResumeCursor() const;
    void setWatching(bool w);
    bool watching() const;
    void markDeleted(const std::string& id);  // an entry (feed or open) or a reply

    // Bookmarks: post or reply id → bookmarkId.
    void setBookmarks(const std::vector<Bookmark>& all);  // sets bookmarksLoaded
    void addBookmark(const Bookmark& b);
    void removeBookmark(const std::string& targetId);
    std::string bookmarkIdFor(const std::string& targetId) const;  // "" when none
    bool bookmarksLoaded() const;

    // Watched and Bookmarks screens: resolved entries, previews like the feed.
    void setSideList(std::vector<Entry> entries, const std::string& cursor, bool append);
    const std::vector<Entry>& sideList() const;
    const std::string& sideCursor() const;

    void clear();  // on sign-out: everything above, plus bookmarksLoaded=false
  };
  ```

- [x] **Step 1: Write the failing tests** (build the entries with a helper `E(id)`)
  ```cpp
  void test_first_page_prepends_new_keeps_cursor()
    // hold p3,p2,p1 cursor "c1"; merge p5,p4,p3 cursor "x" → p5,p4,p3,p2,p1; cursor "c1"
  void test_first_page_without_overlap_replaces()
    // hold p3,p2,p1; merge p9,p8 cursor "c8" → exactly p9,p8; cursor "c8"
  void test_append_page_dedupes_and_end()
    // append overlapping page; no duplicate; cursor "" → feedEnd()
  void test_preview_cut()
    // body of 300 'é' → 280 code points, truncated true
  void test_entry_and_reply_caps()
    // 20000-byte body → 16384 bytes, truncated; 5000-byte reply → 4096
  void test_reply_cap_drops_oldest()
    // append 6 pages of 20 → size 100; front id is that of reply #21
  void test_resume_cursor()
    // cursor "r3" → "r3"; cursor "" with last reply "r7" → "r7"
  void test_bookmark_map()
    // setBookmarks {b1→p1}, {b2→reply r1}; bookmarkIdFor("r1")=="b2"; remove("p1") → ""
  void test_mark_deleted() // marks the feed copy and the open entry, and a reply
  void test_index_of()     // keeps the selection after a merge
  void test_clear_drops_everything()
    // after clear: feed empty, entry null, bookmarkIdFor("p1")=="", !bookmarksLoaded()
  ```
- [x] **Step 2: Run** `pio test -e native -f test_feed_store`. Expected: FAIL.
- [x] **Step 3: Implement** with `std::vector` for the feed and `std::deque` for replies, plus a `std::map` for bookmarks.
- [x] **Step 4: Run** the tests. Expected: PASS.
- [x] **Step 5: Commit** `feat(core): Feed store with paging, caps and bookmarks`.

### Task 5: Feed layout and widgets

**Files:**
- Create: `lib/core/src/ui/feed_layout.{h,cpp}`
- Create: `lib/core/src/ui/widgets/entry_list.{h,cpp}`, `lib/core/src/ui/widgets/text_area.{h,cpp}`
- Test: `test/test_layout/test_main.cpp`, `test/test_widgets/test_main.cpp`

**Interfaces:**
- Consumes: `Entry`, `Reply`, `markdownToText`, `wrapText`, `DisplayLine` and `Span` from `ui/room_layout.h`, `nickColor`, `Grid::outline`.
- Produces:
  ```cpp
  // "now", "5m", "3h", "2d", or the date "25 Oct" once ≥ 7 days.
  std::string relativeAge(int64_t createdMs, int64_t nowMs);
  struct FeedRow { std::u32string line1, line2; uint16_t fg1, fg2; };
  // line1: "nick · 3h · ↩5" (+ " ★" when bookmarked); line2 dim: the title, else the
  // first non-empty line of markdownToText(body); with a cw: "CW: <cw>"; nsfw only: "CW: NSFW";
  // deleted: "[deleted]". Both lines are cut to cols.
  FeedRow layoutFeedRow(const Entry& e, bool bookmarked, int64_t nowMs, int cols, const Theme& t);

  struct EntryLayoutOptions {
    int cols; TextSize size; int64_t nowMs; std::string self;
    bool revealed;                   // the CW/NSFW body is shown
    const Theme* theme = nullptr;
  };
  // Header lines (messageId = entry id): "author · 3h", then "#t1 #t2 · public · NSFW".
  // Body lines (messageId = entry id): the wrapped markdownToText(body), or "CW: …" until
  // revealed; then the attachment lines; then "… read the rest on the web" if truncated.
  // A divider line "── N replies ──" (divider=true). Each reply: an optional dim
  // "↳ @parentAuthor" line, then the reply in the room log style for o.size
  // (messageId = reply id). Deleted bodies render as a dim "[deleted]".
  std::vector<DisplayLine> layoutEntry(const Entry& e, const std::deque<Reply>& replies,
                                       const EntryLayoutOptions& o);

  // Two rows per item; outlines both rows of the selection (like ListView).
  class EntryList {
   public:
    void setRows(std::vector<FeedRow> rows);
    void up(); void down();          // wrap round the ends, like ListView
    int selected() const; void setSelected(int i);  // clamps
    size_t size() const;
    bool atEnd() const;  // the last row is selected
    void render(Grid& g, int top, int bottom, const Theme& t) const;
  };

  // Multi-line editor for the Composer body. Lines break at '\n' and wrap at
  // the render width.
  class TextArea {
   public:
    explicit TextArea(size_t maxChars = 4096);
    bool insert(char32_t c);            // refuses past maxChars (counted in code points)
    void backspace();
    bool left(); bool right();
    bool up(int width);                 // false on the first visual line
    bool down(int width);               // false on the last visual line
    size_t length() const;
    std::string utf8() const;
    void clear();
    void render(Grid& g, int top, int bottom, const Theme& t, bool focused) const;  // keeps the cursor visible
  };
  ```

- [x] **Step 1: Write the failing tests**
  - In `test_layout`:
    - `test_relative_age`: 30 s → `"now"`, 5 min → `"5m"`, 3 h → `"3h"`, 2 d → `"2d"`.
    - `test_feed_row`: line1 == `U"tardibyte · 3h · ↩5 ★"`; line2 is the title; a cw entry gives `U"CW: spiders"`; NSFW only gives `U"CW: NSFW"`; at cols 20 both lines are ≤ 20 code points.
    - `test_entry_layout_reveal`: a cw entry not revealed has no line containing the body text, but has a `"CW: spiders"` line; revealed, it has the body.
    - `test_entry_layout_threading`: the reply with `parentAuthor` is preceded by `U"↳ @tardibyte"`, and that line's `messageId` is the reply id.
    - `test_entry_layout_truncated`: the last body line is `U"… read the rest on the web"`.
    - `test_entry_layout_deleted`: a deleted entry body gives `U"[deleted]"`.
  - In `test_widgets`:
    - `test_entry_list_outline`: 3 rows, `down()`; the cells of rows 2–3 carry `kEdgeTop` on row 2 and `kEdgeBottom` on row 3.
    - `test_entry_list_wraps`: 3 rows; `up()` from 0 selects 2 and `atEnd()`; `down()` then selects 0; on an empty list `up()`/`down()` are no-ops.
    - `test_text_area_newlines_and_cap`: insert `"ab\ncd"`; `utf8()=="ab\ncd"`; at `TextArea(3)`, the 4th insert returns false.
    - `test_text_area_up_down`: the cursor on line 2 `up(40)` moves to line 1, and `up(40)` again returns false.
- [x] **Step 2: Run** `pio test -e native -f test_layout -f test_widgets`. Expected: FAIL.
- [x] **Step 3: Implement.**
  - Replies reuse the per-message line format of `layoutRoom` by converting each `Reply` into a `CircMessage` (`id`, `username`, `content` = plain text, `timestamp` = `createdMs`) and calling `layoutRoom` on a one-element deque.
  - Use the theme's dim colour for line 2 and for the `↳` and `[deleted]` lines.
- [x] **Step 4: Run** the tests. Expected: PASS (the full suite too).
- [x] **Step 5: Commit** `feat(ui): Feed rows, entry layout, EntryList and TextArea`.

### Task 6: API calls, commands, shared state and sources

**Files:**
- Modify: `src/net/https_client.{h,cpp}`: add `bool overflow` to `HttpResult`, set it when the sink overflowed.
- Modify: `src/cs/cs_api.{h,cpp}`, `src/app/messages.h`.
- Modify: `lib/core/src/cs/api_parse.{h,cpp}`, `test/fixtures/api_fixtures.h`, `test/test_api_parse/test_main.cpp`.
- Create: `src/store/locked_feed.{h,cpp}`, `src/feed/feed_source.h`, `src/feed/net_feed_source.cpp` (`#if !RC_FAKE_API`).

**Interfaces:**
- `Mention` gains `std::string type, targetId, replyId, preview`:
  - `type` comes from `type` and `targetId` from `targetId`;
  - `replyId` comes from `metadata.replyId`;
  - `preview` comes from `metadata.replyContent`, else `metadata.postContent`, else `reason`.
- `CsApi::mentions` uses the Global Constraints type filter.
- An overflowing response returns `ApiError{0, "TOO_LARGE", "Response too large"}`.
- New `CsApi` calls. Each write checks its limiter (`entryL_`, `replyL_`, `bookmarkL_`, `watchL_`, and the existing `flagL_` and `deleteL_`) before any request:
  ```cpp
  ApiError feed(const std::string& cursor, int limit, std::vector<Entry>&, std::string& next);
  ApiError entry(const std::string& id, Entry&);
  ApiError replies(const std::string& postId, const std::string& cursor, int limit,
                   std::vector<Reply>&, std::string& next);
  ApiError reply(const std::string& id, Reply&);
  ApiError postEntry(const std::string& json, std::string& postId);
  ApiError postReply(const std::string& json, std::string& replyId);
  ApiError deleteEntry(const std::string& id);
  ApiError deleteReply(const std::string& id);
  ApiError flagEntry(const std::string& id, const std::string& reason);
  ApiError flagReply(const std::string& id, const std::string& reason);
  ApiError watchStatus(const std::string& postId, bool& watching);
  ApiError setWatch(const std::string& postId, bool on);  // POST or DELETE
  ApiError watches(const std::string& cursor, std::vector<std::string>& postIds, std::string& next);
  ApiError bookmarks(const std::string& cursor, std::vector<Bookmark>&, std::string& next);
  ApiError addBookmark(const std::string& postId, const std::string& replyId, std::string& bookmarkId);
  ApiError removeBookmark(const std::string& bookmarkId);
  ```
- `messages.h`:
  - Commands, with their fields:
    - `OpenFeed`, `CloseFeed`, `FeedMore`, `FeedRefresh`;
    - `OpenEntry` (a = postId, b = notification id or "");
    - `MoreReplies`;
    - `PostEntry` (a = JSON), `PostReply` (a = JSON);
    - `DeleteEntry` and `DeleteReply` (a = id);
    - `FlagEntry` and `FlagReply` (a = id, b = reason);
    - `Bookmark` (a = postId, b = replyId) and `Unbookmark` (a = target id);
    - `Watch` and `Unwatch` (a = postId);
    - `OpenWatched` and `OpenBookmarks` (n = 1 to append the next page).
  - Events, with their fields:
    - `FeedUpdated`, `EntryUpdated`, `RepliesLoaded`, `SideListUpdated`;
    - `FeedWriteDone` (ok, text);
    - `EntryPosted` (text = postId);
    - `FeedAlert` (text = banner; code = 1 to beep).
- `locked_feed.h`:
  ```cpp
  struct FeedNotice { std::string id, postId, text; };
  struct FeedState {
    FeedStore store;
    std::vector<FeedNotice> notices;  // unread Feed notifications, newest last
    bool loading = false;
  };
  class LockedFeed { /* same with() as LockedChat */ };
  LockedFeed& lockedFeed();
  ```
- `feed_source.h`:
  ```cpp
  class FeedSource { public: virtual ~FeedSource() = default; virtual void command(const Command&) = 0; };
  FeedSource& feedSource();
  ```
  The net version forwards to `postCommand` with the same "Busy, try again" banner as `NetChatSource`.

- [x] **Step 1: Write the failing test.** Extend `kMentions` with:
  - a `reply` notification: `targetId:"p1"`, `metadata.replyId:"r1"`, `metadata.replyContent:"nice"`;
  - a `post_cooldown` with `reason:"held"` and `actorUsername:"system"`.

  Extend `test_mentions` to assert `type`, `targetId`, `replyId`, and `preview` `"nice"`/`"held"`.
- [x] **Step 2: Run** `pio test -e native -f test_api_parse`. Expected: FAIL.
- [x] **Step 3: Implement** the parse change, then the device files above.
- [x] **Step 4: Run** `pio test -e native` (PASS) and `pio run -e m9` (SUCCESS). The new commands fall into the net worker's `default:` until Task 7.

  This task is committed together with Task 7.

### Task 7: FeedWorker in the net task

**Files:**
- Create: `src/app/feed_worker.{h,cpp}` (`#if !RC_FAKE_API`)
- Modify: `src/app/net_worker.cpp`: owns a `FeedWorker`, routes Feed commands to it, calls `feed_.tick(now, screenOn_)` from `tick()`, and branches on `m.type` in `watch()`. On `Logout` it calls `feed_.signOut()`.

**Interfaces:**
- Consumes: `CsApi` (Task 6), `lockedFeed()`, `FeedStore`, `nextSmallerLimit`, `postEvent`.
- Produces:
  ```cpp
  class FeedWorker {
   public:
    explicit FeedWorker(CsApi& api);
    void handle(const Command& c);            // the Feed commands only
    void tick(int64_t monoMs, bool screenOn); // auto-refresh
    // A Feed-type notification from watch(); dedupes by id.
    void onNotice(const Mention& m, bool buzzer);
    void signOut();                           // lockedFeed store.clear(), notices cleared
  };
  ```

**Behaviour** (spec §6–§7):

- **`OpenFeed`:** first page, then the bookmark map if it isn't loaded (pages of 50, at most 4). Starts auto-refresh (`nextRefresh = now + 120000`). `CloseFeed` stops it.
- **`FeedRefresh`** (palette, or the 120 s tick):
  - re-fetches the first page and calls `mergeFirstPage`;
  - from the palette only, it also reloads the bookmark map;
  - `FeedMore` appends a page with `feedCursor()`.
- **Fetching with fallback:** every list fetch goes through one helper. On `TOO_LARGE` it retries with `nextSmallerLimit`. At 0 it shows the banner `"Too large to load"`.
- **`OpenEntry`:** entry, then replies (limit 20), then watch status, posting `EntryUpdated` and `RepliesLoaded`.
  - With `b` set, it marks that notification read and removes it from `notices`.
  - A 404 shows the banner `"Entry gone"` and posts `EntryUpdated` with `ok=false`.
- **`PostReply`:** on success it fetches `replies(postId, store.repliesResumeCursor(), 20)`. `FeedWriteDone` carries `ok` and the error text.
- **`PostEntry`:** on success it posts `EntryPosted(postId)`.
- **Error banners:**
  - 403 on delete or flag: `"Not allowed"`;
  - 429 or a local limit: `"Slow down"`;
  - otherwise `ApiError::message`.
- **Bookmark, Unbookmark, Watch and Unwatch** update the store only after success.
- **`onNotice`:**
  - For `post_cooldown`, the banner is `"Entry held for review (saved as a note)"` with code 0 (no beep).
  - Otherwise it shows `"@actor replied: preview"`, or `"@actor mentioned you: preview"` for the `*_mention` types, with code 1.
  - If `targetId` is the open entry, it fetches new replies and marks the notification read instead of adding it to `notices`.
- **`watch()`:** `chat_mention` keeps its current path; the other types go to `feed_.onNotice`.
- **Memory log:** `logMemory()` appends `feed <n> replies <n>`.

- [x] **Step 1: Implement** `FeedWorker` and the `net_worker.cpp` wiring.
- [x] **Step 2: Run** `pio test -e native` (PASS), then `pio run -e m9` (SUCCESS).
- [x] **Step 3: Commit** Tasks 6 and 7 together: `feat(net): Feed API calls and worker`.

### Task 8: Fake Feed source

**Files:**
- Create: `src/feed/fake_feed_source.cpp` (`#if RC_FAKE_API`)

**Interfaces:**
- Consumes: `FeedSource`, `lockedFeed()`, `FeedStore`, `compose`'s limiters, `postEvent`.
- Behaviour:
  - 15 scripted entries. Entry 3 has `contentWarning "spiders"`; entry 5 is NSFW only; entry 7 is about 6 KB of Markdown (headings, a list, a link, an image and a code fence). Each has 0–25 replies, a few of them threaded.
  - Pages hold 10 entries.
  - Writes apply locally under the real limiters (a new entry goes on top as author `oumike`), and `CsApi`-style banners appear for limit hits.
  - Bookmarks and watches toggle locally.
  - Deleting is allowed on `oumike` items only; other items get `"Not allowed"`.
  - A `reply` notice on entry 1 arrives every 90 s: an esp_timer posts a `Command` to itself, so `lockedFeed` is never re-entered.
  - Commands are applied inline, as in `fake_chat_source.cpp`.
- Header comment lists the script, as in `fake_chat_source.cpp`.

- [x] **Step 1: Implement.**
- [x] **Step 2: Run** `pio run -e m9_fake`. Expected: SUCCESS.
- [x] **Step 3: Commit** `feat(fake): scripted Feed for m9_fake`.

### Task 9: FeedList, Watched and Bookmarks screens, router, Home, alerts

**Files:**
- Modify: `src/screens/screens.h`, `src/screens/router.{h,cpp}`, `src/screens/home.cpp`, `src/app/ui_task.cpp`, `src/app/app_state.h`
- Create: `src/screens/feed_list.cpp`

**Interfaces:**
- `router::Id` gains `FeedList`, `Entry`, `Composer`, `Watched`, `Bookmarks`.
  - `Action::OpenFeed` goes to `FeedList` (replacing `ComingSoon`).
  - Inside `Entry`, `Composer`, `Watched` or `Bookmarks`, OpenFeed returns to `FeedList`, after the screen's own confirmation (the Composer's discard prompt).
- `FeedListScreen(Kind kind)`, with `Kind { Feed, Watched, Bookmarks }`:
  - one class, three router instances (like `RoomScreen{true}`);
  - `enter` sends `OpenFeed`, `OpenWatched` or `OpenBookmarks`; `leave` sends `CloseFeed` for `Feed`.
  - It renders `EntryList` from `store.feed()` or `store.sideList()`, and keeps the selection by id across `FeedUpdated` using `indexOf`.
  - Down on the last row (`atEnd()`) sends `FeedMore`, or `OpenWatched`/`OpenBookmarks` with n = 1, while more remain (`!feedEnd()`, or a non-empty `sideCursor()`), and keeps the selection; once the list is complete it wraps to the top. Up on the first row wraps to the last held row.
  - Select or Right goes to `Entry` with arg = postId; Left or Back goes Home.
  - Ctrl opens a `ListView` palette: `New entry`, `Refresh`, `Watched threads`, `Bookmarks`, `Text size`.
  - The status title is `Feed`, `Watched` or `Bookmarks`. Feed adds ` ↻ 2m`.
- Home adds the row `field("feed", "<n> unread")` from `lockedFeed().notices.size()`. Selecting it goes to `Entry` with arg `postId + '\n' + noticeId` of the newest notice.
- `ui_task.cpp`: `FeedAlert` shows the banner, and when `code == 1 && cfg.buzzer` calls `board::beep(1800, 80)`.

- [x] **Step 1: Implement.**
- [x] **Step 2: Run** `pio run -e m9_fake` and `pio run -e m9`. Expected: SUCCESS.
- [x] **Step 3: Flash `m9_fake`** (stop any monitor first) and check on hardware:
  - the list scrolls and pages;
  - the CW row reads `CW: spiders`;
  - `★` follows bookmarks;
  - the 90 s alert beeps once.
- [x] **Step 4: Commit** `feat(ui): FeedList, Watched and Bookmarks screens`.

### Task 10: Entry screen

**Files:**
- Create: `src/screens/entry.{h,cpp}`
- Modify: `src/screens/router.cpp`

**Interfaces:**
- Consumes: `layoutEntry`, `LogView`, `InputLine`, `ListView` menus, the QR viewer used by `RoomScreen` (reuse its link/QR code path), `replyJson`, `flagJson`.
- `EntryScreen`:
  - `enter(arg)`: the arg is `postId`, or `postId\nnoticeId`. It sends `OpenEntry{a = postId, b = noticeId}`.
  - It rebuilds the lines on `EntryUpdated`, `RepliesLoaded` and `FeedWriteDone`.
  - Text size comes from `app().cfg`.

**Keys** (spec §5):

- Up/Down drive `LogView`; Down while following at the end sends `MoreReplies` unless `repliesEnd()`.
- A Char goes to the `InputLine`.
- Select with text sends `PostReply`:
  - `replyJson(postId, text, selected reply id or "")`;
  - the line is disabled until `FeedWriteDone`;
  - it is cleared on ok and kept on failure.
- Select on an empty line:
  - if the CW is unrevealed and the body is selected or following, it reveals;
  - otherwise it opens the entry menu, or the reply menu when a reply is selected.
- Above the input line, the screen shows `↳ @name` when a reply is selected.
- Entry menu:
  - `Bookmark`/`Unbookmark`, from `bookmarkIdFor`;
  - `Watch`/`Unwatch`;
  - `Show link` (`https://cyberspace.online/<author>/<slug>`);
  - `Report…` (confirm, then an optional reason line);
  - `Delete…` (only when `author == self`; confirm, default Cancel);
  - `Text size`.
- Reply menu: `Reply`, `Bookmark`/`Unbookmark`, `Show link` (`…/<author>/<slug>#<replyId>`), `Report…`, `Delete…` (own only).
- If `EntryUpdated` has `ok=false`, it goes back to `FeedList`.
- Leaving it calls `store.closeEntry()` through `lockedFeed`.

- [x] **Step 1: Implement.**
- [x] **Step 2: Run** `pio test -e native`, `pio run -e m9_fake` and `pio run -e m9`. Expected: all green.
- [x] **Step 3: Flash `m9_fake`** and check:
  - the CW reveal;
  - the long Markdown entry renders and scrolls;
  - a threaded reply shows `↳`;
  - replying to a reply sets the marker;
  - bookmark, watch and delete (own) work;
  - delete on others is refused.
- [x] **Step 4: Commit** `feat(ui): Entry screen with replies and menus`.

### Task 11: Composer screen

**Files:**
- Create: `src/screens/composer.{h,cpp}`
- Modify: `src/screens/router.cpp`

**Interfaces:**
- Consumes: `TextArea`, `InputLine` (title, topics, CW), `EntryDraft`, `validateDraft`, `entryJson`.
- Fields in order:
  1. Title (`InputLine`);
  2. Body (`TextArea(4096)`);
  3. Topics (`InputLine`);
  4. Public [ ];
  5. NSFW [ ];
  6. CW (`InputLine`).
- **Keys:**
  - Up/Down move between fields. In Body they move by line first, using `TextArea::up/down`.
  - In Body, Select inserts `'\n'`; on a checkbox, Select toggles it.
  - Ctrl opens a menu: `Post`, `Discard`.
  - `Post`:
    - runs `validateDraft`; an error shows a banner;
    - otherwise it asks to confirm with the text `"Post entry? (2 per minute, 24 per day)"`, then sends `PostEntry{a = entryJson}`.
  - `EntryPosted` clears the draft and goes to `Entry` with that postId. A failed `FeedWriteDone` keeps the draft.
- Back (or OpenFeed) with any field non-empty asks `"Discard draft?"` (default Cancel).
- The draft is a static `EntryDraft` that survives leaving the screen. It is cleared only by Post or Discard.
- The counter reads `n/4096` beside the Body label.

- [x] **Step 1: Implement.**
- [x] **Step 2: Run** `pio run -e m9_fake` and `pio run -e m9`. Expected: SUCCESS.
- [x] **Step 3: Flash `m9_fake`** and check:
  - a multi-line body;
  - the counter;
  - 4 topics are rejected;
  - the third post within a minute shows "Slow down";
  - posting opens the new entry;
  - Back asks to discard.
- [x] **Step 4: Commit** `feat(ui): entry Composer`.

### Task 12: Documentation

**Files:**
- Modify: `docs/USE.md`, `docs/TESTING.md`, `AGENTS.md`

- [x] **Step 1: `USE.md`.** Add a Feed section covering the keys from spec §5 (FeedList, Entry, Composer, Watched and Bookmarks) and the alerts.
- [x] **Step 2: `TESTING.md`.**
  - Under "Open hardware checks", add the Task 9–11 `m9_fake` checks.
  - In the live checklist, add a Feed block:
    - read the feed;
    - post a test entry, then delete it;
    - reply, and reply to a reply;
    - bookmark and unbookmark;
    - watch and unwatch;
    - a reply alert from a second account within a minute;
    - auto-refresh shows a new entry within 2 minutes.
- [x] **Step 3: `AGENTS.md`.** Point the M3 line at this plan, and update the progress section (task status and the native test count from `pio test -e native`).
- [x] **Step 4: Commit** `docs: Feed user guide and checks`.
