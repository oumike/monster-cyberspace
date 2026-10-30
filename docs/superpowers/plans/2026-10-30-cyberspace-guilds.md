# Guilds Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Browse guilds, read and start guild threads, show info and members, join/leave/promote, and alert on new guild threads; the M9's Map button opens it.

**Architecture:** Guild data (guilds, my guilds, members) gets its own Arduino-free parse and store in `lib/core/src/guild/`, held in `FeedState` behind `lockedFeed()`. The `FeedWorker` serves the guild commands; a guild's threads load into the `FeedStore` side list, so the thread list is `FeedListScreen{Kind::Guild}` and threads open in the existing Entry screen. New screens: `GuildListScreen`, `MembersScreen`; an Info view inside the Guild screen.

**Tech Stack:** C++17, PlatformIO (pioarduino, ESP32-S3), ArduinoJson 7, Unity native tests.

**Spec:** `docs/superpowers/specs/2026-10-30-cyberspace-guilds-design.md`

## Global Constraints

- Never call the live Cyberspace API from tests, scripts or an agent session; tests use fixtures.
- Every write comes from a key press, is confirmed first, and is never retried automatically.
- Membership limiter (join, promote, leave share it): 3/min, 15/day. Guild threads share `entryL_` (2/min, 24/day).
- Page sizes: guilds and members `limit=20`; threads `limit=10` through `fetchList` (10 → 5 → 2).
- `GuildStore` caps: 200 browse guilds, 200 members; bios cut to 280 bytes.
- Keep logic board-agnostic: portable code in `lib/core`, hardware only through `src/hal/board.h`.
- Never call `feedSource().command()` inside `lockedFeed().with(...)`.
- Confirm copy: "Join <name>? (3 per minute, 15 per day)", "Leave <name>? (3 per minute, 15 per day)" plus " Your badge will be cleared." for the member guild, "Make <name> my guild? (3 per minute, 15 per day)", "Post thread? (2 per minute, 24 per day)".
- Error copy: 409 join → the server message; 403 leave/promote → "Manage this on the web"; 404 → "Guild not found"; local limit → the existing "Slow down: try in Ns".
- Each task: tests first, watch them fail, implement, green, commit with the task's message plus the `Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>` trailer. Tick each box as you finish.
- Do not push.

## Review Focus

- A guild the user joins or leaves while GuildList is cached: after the write, my guilds are re-read and the browse list must re-show a guild that was left (it was filtered out while pinned) — Task 2 `test_left_guild_returns_to_browse`.
- A guild with an empty or missing `icon`, `bio` or `name`: the row must still have a readable first line (falls back to the slug) — Task 4 `test_row_without_icon_or_name`.
- `apprenticeCount` absent or `role` null in Get Guild: must read as 0 and "not in it", showing Join, not Leave — Task 1 `test_guild_missing_counts_and_null_role`.
- Back from a guild thread must land on that guild's thread list, not the Feed — Task 9 step "Entry back target".
- Offline or signed-out guild commands must settle their screens (no endless "loading…") — Task 6 `needNet`/`refuseCommand` step.

---

### Task 1: Guild types and parsing

**Files:**
- Create: `lib/core/src/guild/guild.h`, `lib/core/src/guild/guild_parse.h`, `lib/core/src/guild/guild_parse.cpp`
- Modify: `lib/core/src/cs/api_parse.h:56-61`, `lib/core/src/cs/api_parse.cpp:143-152`
- Test: `test/test_guild_parse/test_main.cpp`, `test/fixtures/guild_fixtures.h`, `test/test_api_parse/test_main.cpp`

**Interfaces:**
- Produces:
  ```cpp
  struct Guild {
    std::string id, slug, name, icon, founder, bio, link, linkText;
    std::string role;   // "founder" | "member" | "apprentice" | "" (not in it / unknown)
    int64_t createdMs = -1;
    int members = 0, apprentices = 0;
  };
  struct MyGuild { std::string slug, name, icon, role; };
  struct Member { std::string id, username, role; int64_t joinedMs = -1; };  // id = membershipId
  bool parseGuildPage(std::string_view body, std::vector<Guild>& out, std::string& cursor);
  bool parseGuild(std::string_view body, Guild& out);      // Get Guild: object + isMember/role
  bool parseMyGuilds(std::string_view body, std::vector<MyGuild>& out);
  bool parseMembers(std::string_view body, std::vector<Member>& out, std::string& cursor);
  bool parseGuildRole(std::string_view body, std::string& role);  // join/promote {data:{role}}
  ```
  `Mention` gains `std::string guildName;` from `metadata.guildName`.

- [x] **Step 1: Write fixtures and failing tests.** Fixtures copy the docs' JSON shapes (List Guilds object, Get Guild with `isMember`/`role`, the two-guild my-guilds list, the members list, join `{"data":{"guildId":"g","role":"apprentice"}}`). Tests:
  - `test_guild_page`: two guilds; `out[0].slug=="night-owls"`, `name=="Night Owls"`, `icon=="🦉"`, `founder=="someone"`, `members==42`, `apprentices==7`, `link`/`linkText` set, `createdMs` from `2026-03-27T10:12:01.516Z`; cursor `"xyz"`; a `"cursor":null` page gives `""`.
  - `test_guild_missing_counts_and_null_role`: Get Guild without `apprenticeCount` and with `"role":null,"isMember":false` → `apprentices==0`, `role==""`.
  - `test_guild_role_member`: Get Guild `"isMember":true,"role":"member"` → `role=="member"`.
  - `test_my_guilds_six`: six entries keep order; `[0].role=="member"`, `[1..5].role=="apprentice"`.
  - `test_members_page`: `id=="guildId_uid"`, `username=="someone"`, `role=="member"`, `joinedMs` parsed, cursor `""` for null.
  - `test_join_role`: `parseGuildRole` → `"apprentice"`; malformed body → false.
  - In `test_api_parse`: `test_guild_new_thread_notice`: a `guild_new_thread` notification with `metadata.guildName:"Night Owls"`, `postContent:"hello"` → `type`, `targetId`, `guildName=="Night Owls"`, `preview=="hello"`.

- [x] **Step 2: Run** `pio test -e native -f test_guild_parse -f test_api_parse` — FAIL (missing headers / field).

- [x] **Step 3: Implement** the structs and parsers, reusing `parseIsoMs` from `feed/feed_parse.h`; null/absent strings read as "", absent counts as 0. Add `guildName` to `Mention` and its parse.

- [x] **Step 4: Run** the same command — PASS.

- [x] **Step 5: Commit** `Guilds: types and parsing`.

### Task 2: GuildStore

**Files:**
- Create: `lib/core/src/guild/guild_store.h`, `lib/core/src/guild/guild_store.cpp`
- Test: `test/test_guild_store/test_main.cpp`

**Interfaces:**
- Consumes: Task 1 types.
- Produces:
  ```cpp
  constexpr size_t kMaxGuilds = 200, kMaxMembers = 200, kGuildBioBytes = 280;
  class GuildStore {
   public:
    void setMine(std::vector<MyGuild> mine);          // sets mineLoaded
    const std::vector<MyGuild>& mine() const;
    bool mineLoaded() const;
    const MyGuild* myGuild(const std::string& slug) const;  // nullptr when not in it
    // Browse list. first=true replaces; else appends, skipping held ids. Bios cut.
    void setBrowse(std::vector<Guild> page, const std::string& cursor, bool first);
    // The browse list without guilds in mine(), in server order.
    std::vector<Guild> others() const;
    // Every guild in mine() resolved from the browse list or the open guild
    // (fields missing → a Guild with slug/name/icon/role from MyGuild), mine() order.
    std::vector<Guild> pinned() const;
    const std::string& browseCursor() const;
    bool browseEnd() const;                 // loaded and cursor ""
    void openGuild(Guild g);                // clears members; bio cut
    void closeGuild();                      // frees the open guild and members
    const Guild* guild() const;
    void appendMembers(std::vector<Member> page, const std::string& cursor, bool first);
    const std::vector<Member>& members() const;
    const std::string& membersCursor() const;
    void clear();                           // sign-out: everything
  };
  ```
  Over a cap, the page that would exceed it is cut to fit and the cursor becomes "" (the list ends).

- [x] **Step 1: Write failing tests:**
  - `test_pinned_removed_from_others`: mine `{b}`, browse `{a,b,c}` → `others()` ids `{a,c}`, `pinned()` slugs `{b}` with browse's `members`.
  - `test_mine_after_browse`: browse first, then `setMine({c})` → `others()` `{a,b}`.
  - `test_left_guild_returns_to_browse`: mine `{b}` then `setMine({})` → `others()` `{a,b,c}`.
  - `test_pinned_unresolved_uses_my_guild`: mine `{z}` not in browse → `pinned()[0].name` from MyGuild, `members==0`.
  - `test_append_dedupes`: page `{a,b}` then `{b,c}` → `{a,b,c}`.
  - `test_browse_cap_ends_list`: 199 held + page of 5 → size 200, `browseCursor()==""`, `browseEnd()`.
  - `test_bio_cut`: a bio of 200 × "é" (400 bytes) stores ≤ 280 bytes, an even byte count (no split code point), and `u32ToUtf8(utf8ToU32(bio)) == bio`.
  - `test_members_cap_and_first`: `first=true` replaces; cap 200 ends the cursor.
  - `test_clear`: everything empty, `mineLoaded()==false`, `guild()==nullptr`.

- [x] **Step 2: Run** `pio test -e native -f test_guild_store` — FAIL.
- [x] **Step 3: Implement** in `guild_store.cpp`.
- [x] **Step 4: Run** — PASS.
- [x] **Step 5: Commit** `Guilds: store`.

### Task 3: Compose and limiter

**Files:**
- Modify: `lib/core/src/feed/compose.h`, `lib/core/src/feed/compose.cpp`, `lib/core/src/cs/rate_limiter.h:42`, `lib/core/src/cs/rate_limiter.cpp:55`
- Test: `test/test_feed_compose/test_main.cpp`, `test/test_limits_token/test_main.cpp`

**Interfaces:**
- Produces:
  ```cpp
  // Validated with validateDraft (cw/isPublic/nsfw ignored). Leaves out an
  // empty title and empty topics; never writes isPublic, isNSFW or contentWarning.
  std::string guildThreadJson(const EntryDraft& d);
  RateLimiter makeMembershipLimiter();  // {3, 60000}, {15, 86400000}
  ```

- [x] **Step 1: Write failing tests:** `test_guild_thread_json_minimal` (`{"content":"hi"}`), `test_guild_thread_json_full` (title trimmed, topics `["a","b"]`), `test_guild_thread_json_ignores_flags` (a draft with `isPublic`, `nsfw`, `cw` set has none of `isPublic`, `isNSFW`, `contentWarning`); in `test_limits_token`: `checkMinuteEdge(makeMembershipLimiter(), 3)`, `checkDayEdge(makeMembershipLimiter(), 15)`.
- [x] **Step 2: Run** `pio test -e native -f test_feed_compose -f test_limits_token` — FAIL.
- [x] **Step 3: Implement** both.
- [x] **Step 4: Run** — PASS.
- [x] **Step 5: Commit** `Guilds: thread JSON and membership limiter`.

### Task 4: Guild layout and text

**Files:**
- Create: `lib/core/src/ui/guild_layout.h`, `lib/core/src/ui/guild_layout.cpp`
- Test: `test/test_layout/test_main.cpp` (add cases)

**Interfaces:**
- Consumes: Task 1 types; `FeedRow`, `relativeAge` from `ui/feed_layout.h`; `markdownToText`.
- Produces:
  ```cpp
  // line1 "🦉 Night Owls · 42+7" (+ " · member" etc. when role set; "+N" only when N>0);
  // no icon → no leading icon and space; no name → slug. line2 dim: the bio's first
  // non-empty markdownToText line, else "no bio". Cut like layoutFeedRow.
  FeedRow layoutGuildRow(const Guild& g, int cols, const Theme& t, int headCols = -1);
  // "@user", right: "role · 3mo" (relativeAge of joinedMs).
  ListItem memberItem(const Member& m, int64_t nowMs);
  // The Info page text: "<icon> <name>", "founded by @x", "42 members · 7 apprentices",
  // "founded Mar 2026", "you: member" (only when role set), "", the bio as
  // markdownToText, then "", "linkText <url>" when link is set.
  std::u32string guildInfoText(const Guild& g);
  // "@user in Night Owls: <preview>"; no guildName → "@user in a guild: <preview>".
  std::string guildNoticeText(const std::string& actor, const std::string& guildName,
                              const std::string& preview);
  ```

- [x] **Step 1: Write failing tests:** `test_guild_row_counts_role` (`🦉 Night Owls · 42+7 · member`), `test_guild_row_no_apprentices` (`· 42` with no `+0`), `test_row_without_icon_or_name` (line1 starts with the slug), `test_guild_row_no_bio` (`no bio`), `test_member_item`, `test_guild_info_text` (contains each line above; no "you:" when role empty; no link line when link empty), `test_guild_notice_text` (both forms).
- [x] **Step 2: Run** `pio test -e native -f test_layout` — FAIL.
- [x] **Step 3: Implement.**
- [x] **Step 4: Run** — PASS, then the whole `pio test -e native` — all green.
- [x] **Step 5: Commit** `Guilds: rows, info text and notice text`.

### Task 5: CsApi guild calls

**Files:**
- Modify: `src/cs/cs_api.h`, `src/cs/cs_api.cpp` (constructor limiter list near line 61; `mentions()` at line 334)

**Interfaces:**
- Consumes: Tasks 1, 3.
- Produces (all reads unlimited locally; writes through `limited()`):
  ```cpp
  ApiError guilds(const std::string& cursor, int limit, std::vector<Guild>&, std::string& next);
  ApiError guild(const std::string& slug, Guild&);   // 404 passes through as e.http
  ApiError guildMembers(const std::string& slug, const std::string& cursor, int limit,
                        std::vector<Member>&, std::string& next);
  ApiError guildThreads(const std::string& slug, const std::string& cursor, int limit,
                        std::vector<Entry>&, std::string& next);   // parseFeedPage
  ApiError myGuilds(std::vector<MyGuild>&);                        // GET /v1/users/me/guilds
  ApiError postGuildThread(const std::string& slug, const std::string& json, std::string& postId);  // entryL_
  ApiError joinGuild(const std::string& slug, std::string& role);     // membershipL_
  ApiError promoteGuild(const std::string& slug, std::string& role);  // membershipL_
  ApiError leaveGuild(const std::string& slug);                        // membershipL_
  ```
  Add `RateLimiter membershipL_` (`makeMembershipLimiter()`). Paths use `enc(slug)` and the existing `pageQuery`. `mentions()` type list gains `guild_new_thread`.

- [x] **Step 1: Implement** the calls following `feed()`/`postEntry()`/`setWatch()`.
- [x] **Step 2: Build** `pio run -e m9` — SUCCESS.
- [x] **Step 3: Commit** `Guilds: API calls`.

### Task 6: Messages, state and FeedWorker

**Files:**
- Modify: `src/app/messages.h`, `src/store/locked_feed.h`, `src/app/feed_worker.h`, `src/app/feed_worker.cpp`, `src/app/net_worker.cpp:540-560`, `src/app/queues.cpp:30-66`

**Interfaces:**
- Consumes: Tasks 1–5.
- Produces:
  - `CmdType`: `OpenGuilds` (n=1 next page), `OpenGuild` (a=slug; loads the guild, my guilds if not loaded, and the first thread page), `GuildThreadsMore` (a=slug), `CloseGuild`, `OpenMembers` (a=slug; n=1 next page), `PostGuildThread` (a=slug, b=JSON), `JoinGuild`, `LeaveGuild`, `PromoteGuild` (a=slug).
  - `EvtType`: `GuildsUpdated` (ok=false on a failed load), `GuildUpdated` (ok=false, code=404 when gone), `MembersUpdated` (ok), `GuildWriteDone` (ok; text = error or the success banner: "Joined as member"/"Joined as apprentice", "Left <name>", "<name> is now your guild").
  - `FeedState` gains `GuildStore guilds;`.
  - Thread pages go to `s.store.setSideList(entries, next, append)` and post `SideListUpdated`.

- [x] **Step 1: Add the enums and `FeedState.guilds`.**
- [x] **Step 2: FeedWorker handlers.** Add each command to `isFeedCommand` and `handle`. `OpenGuilds`: `myGuilds` when `!mineLoaded()` or `n==0`, then `fetchList(20, …guilds…)`; `setBrowse(page, next, n==0)`; post `GuildsUpdated`. `OpenGuild`: `guild(slug)` → 404 posts `GuildUpdated` code 404 ok=false and banner "Guild not found"; else `openGuild`, post `GuildUpdated`, then the first thread page (`fetchList(kSideLimit, …)`). `CloseGuild` → `closeGuild()`. Writes: `PostGuildThread` posts `EntryPosted` like `postEntry`; Join/Leave/Promote map errors (409 → `e.message`, 403 → "Manage this on the web", 404 → "Guild not found", else `errorText`), post `GuildWriteDone`; on success re-read `myGuilds` and the open guild, then post `GuildsUpdated` and `GuildUpdated`. `signOut()` also clears `s.guilds`.
- [x] **Step 3: Notices.** In `onNotice`, `guild_new_thread` uses `guildNoticeText(m.actorUsername, m.guildName, preview)`; the rest is unchanged.
- [x] **Step 4: Settle offline/busy.** `needNet` and `refuseCommand`: `OpenGuilds` → `GuildsUpdated` ok=false; `OpenGuild`/`GuildThreadsMore` → `SideListUpdated` ok=false; `OpenMembers` → `MembersUpdated` ok=false; `PostGuildThread` → `EntryPosted` ok=false with the reason; Join/Leave/Promote → `GuildWriteDone` ok=false with the reason.
- [x] **Step 5: Build** `pio run -e m9` — SUCCESS.
- [x] **Step 6: Commit** `Guilds: worker commands and alerts`.

### Task 7: Demo fake source

**Files:**
- Modify: `src/feed/fake_feed_source.cpp` (and its header block of members near line 180)

- [x] **Step 1: Seed** two guilds ("Night Owls" 🦉, the user a `member`; "Deep Divers" 🐳, not in it), three threads each (`guildSlug` set, `isGuildThread`), three members each; handle every Task 6 command locally with the same events, applying `makeMembershipLimiter`/entry limits through the file's `limited()`; join makes the user an apprentice, leave removes, promote swaps roles.
- [x] **Step 2: Build** `PLATFORMIO_BUILD_DIR=.pio/demo PLATFORMIO_BUILD_FLAGS=-DRC_FAKE_API=1 pio run -e m9` — SUCCESS.
- [x] **Step 3: Commit** `Guilds: demo build fixtures`.

### Task 8: GuildList, router, Map and Home

**Files:**
- Create: `src/screens/guild_list.cpp`
- Modify: `src/screens/screens.h`, `src/screens/router.h`, `src/screens/router.cpp`, `src/screens/home.cpp`

**Interfaces:**
- Consumes: Tasks 2, 4, 6.
- Produces: `router::Id::GuildList`, `Guild`, `Members`; `class GuildListScreen : public Screen` (EntryList rows, `Overlay` palette like `FeedListScreen`); `router::previous()` returning the `{Id, arg}` before the current screen:
  ```cpp
  struct Place { Id id; std::string arg; };
  Place previous();
  ```

- [x] **Step 1: Router.** Add the ids and `previous()` (set in `go()`). In `handleGlobal`, `OpenMap` is no longer swallowed: signed in and past Splash, it closes overlays and goes to `GuildList`, except from the Composer (pass the key to the screen) and on `GuildList` with an overlay open (only closes it).
- [x] **Step 2: GuildListScreen.** `enter` sends `OpenGuilds`; rows = a divider row `── my guilds ──` (when `pinned()` non-empty), the pinned guilds, `── all guilds ──`, then `others()`, each via `layoutGuildRow`; divider rows are skipped by Up/Down. Down at the end with `!browseEnd()` sends `OpenGuilds` n=1; Select/Right opens `Guild` with the slug; Left/Back → Home; palette Refresh (re-sends `OpenGuilds` n=0) and Text size. Status `Guilds`; empty text "offline" / "loading…" / "couldn't load: Ctrl, Refresh" (touch: "⋯, Refresh") / "no guilds"; hint `↑↓ pick  ● open  ← home  ctrl cmds`, touch `tap to open`. Registers taps and the side strip like `FeedListScreen`.
- [x] **Step 3: Home.** Add `kGuilds` after `kFeed` (list item `Guilds`, tile `guilds`), `kTileCount = 7`.
- [x] **Step 4: Build** `pio run -e m9` and the demo build — SUCCESS; on the demo build `scripts/screenshot.py` of GuildList is optional.
- [x] **Step 5: Commit** `Guilds: list screen, Map button and Home item`.

### Task 9: Guild thread list, Info, Members and Entry back

**Files:**
- Create: `src/screens/guild_members.cpp`, `src/screens/guild_info.h`, `src/screens/guild_info.cpp`
- Modify: `src/screens/screens.h` (`FeedListScreen`), `src/screens/feed_list.cpp`, `src/screens/entry.cpp`, `src/screens/entry.h`

**Interfaces:**
- Consumes: Tasks 4, 6, 8.
- Produces: `FeedListScreen::Kind::Guild` (arg = slug); `class GuildInfoView { void open(const Guild&); bool key(const KeyEvent&) /* false = closed */; void render(Grid&, const Theme&); }` (a `TextView` of `guildInfoText`; Centre with a link opens a `LinkView` of it); `class MembersScreen : public Screen` (arg = slug).

- [x] **Step 1: Kind::Guild.** `enter(slug)` sends `OpenGuild`; `requestMore` sends `GuildThreadsMore`; The open guild is freed by `GuildListScreen::enter` (it sends `CloseGuild`), so Entry, Composer and Members keep it; `FeedListScreen::leave` sends nothing for `Kind::Guild`. Status shows `<icon> <name>` from `s.guilds.guild()`. Left/Back → `GuildList`. `GuildUpdated` with code 404 → `router::go(GuildList)`.
- [x] **Step 2: Palette.** Items per spec §5, built from `guild()->role`: New thread → `router::go(Composer, "guild:" + slug)`; Info → the `GuildInfoView`; Members → `router::go(Members, slug)`; Join/Leave/Make my guild → a confirm `Overlay` with the Global Constraints copy, then the command; Refresh → `OpenGuild`; Text size. `GuildWriteDone` shows its text as a banner (error when !ok).
- [x] **Step 3: MembersScreen.** `ListView` of `memberItem`s; enter sends `OpenMembers`; Down on the last item with a cursor sends `OpenMembers` n=1; Back → `Guild` with the slug. Status `members · <name>`.
- [x] **Step 4: Entry back target.** In `EntryScreen::enter`, remember `router::previous()` when its id is `Guild`; Back (when following) returns there with its arg, else `FeedList` as now.
- [x] **Step 5: Build** `pio run -e m9` and the demo build — SUCCESS.
- [x] **Step 6: Commit** `Guilds: thread list, info, members`.

### Task 10: Composer guild target

**Files:**
- Modify: `src/screens/composer.h`, `src/screens/composer.cpp`

- [x] **Step 1: Target.** `enter("guild:<slug>")` sets a file-static `gGuildSlug`; `enter("")` clears it. With a slug: fields are Title, Body, Topics only (focus skips `kPublic`, `kNsfw`, `kCw` and they aren't drawn); status `new thread · <name>` (name from `s.guilds.guild()`, else the slug); Post confirms "Post thread? (2 per minute, 24 per day)" and sends `PostGuildThread{slug, guildThreadJson(gDraft)}`; leaving/discard goes to `Guild` with the slug instead of `FeedList`. `EntryPosted` opens the Entry as today. The OpenMap key asks "Discard draft?" like the Feed key.
- [x] **Step 2: Build** `pio run -e m9` and the demo build — SUCCESS.
- [x] **Step 3: Commit** `Guilds: new thread in the Composer`.

### Task 11: Docs and full build

**Files:**
- Modify: `docs/USE.md` (Buttons table: Map → Guilds; a Guilds section), `docs/TESTING.md` (an unticked "Guilds (M9)" checklist: Map button, browse and pinning, open, thread and reply, Back to the guild, new thread, join, leave, make my guild, info and QR, members, alert), `AGENTS.md` (Guild spec/plan pointers and a progress line), `docs/superpowers/specs/2026-10-25-cyberspace-m9-circ-design.md:123` (Map → Guilds).

- [x] **Step 1: Write the docs.**
- [x] **Step 2: Run** `pio test -e native` — all PASS; `scripts/build-upload-monitor.sh -B` — every env SUCCESS.
- [x] **Step 3: Commit** `Guilds: docs`.
