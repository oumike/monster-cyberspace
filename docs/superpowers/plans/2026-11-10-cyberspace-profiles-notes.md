# Fuller profiles and Notes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** A profile shows a user's Replies and Guilds and your own can be edited; Notes (private, with revisions) open from a Home tile.

**Architecture:** Portable parsing, JSON bodies and limit checks in `lib/core` (`people/`, new `notes/`), host-tested with fixtures. `CsApi` gains the calls; `PeopleWorker` runs the profile commands and `FeedWorker` the Notes ones (no new task). New LVGL list screens follow `MembersScreen`; the Composer gains a body-only Note mode.

**Tech Stack:** C++17, PlatformIO, Unity host tests (`native`), LVGL 9.5 (`lib/lvui`), ArduinoJson.

**Spec:** `docs/superpowers/specs/2026-11-10-cyberspace-profiles-notes-design.md`

## Global Constraints

- Every write comes from a key press; no automatic retries of a write (AGENTS.md).
- Tests use host fixtures; never call the live API. Fixtures are synthetic, shaped after `docs.md` / `types.d.ts`.
- Portable logic in `lib/core`; hardware only through `src/hal/board.h`.
- Never modify `src/grid_ui/` (X4's frozen UI): these features are not added there.
- No Followers list; no pinned entry or lat/long in Edit profile; no note topics; History is read-only.
- Limits (characters, not bytes): display name, website name, location 64; bio 640; website URL 2048 and `http://` or `https://`; note 32,768.
- `RC_FEAT_NOTES` defaults to `RC_FEAT_FEED`; 0 in `cardputer_adv` and `x4`.
- Build only `m9` and `wio_l2` (boards under test) plus `cardputer_adv` to prove Notes compiles out.
- Commit trailer: `Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>`. Never push.

## Review Focus

1. Limits counted in UTF-8 bytes instead of characters: a 640-emoji bio must pass, 641 fail (Task 1 test `test_profile_field_error_counts_chars`).
2. A value of only spaces in Edit profile is a cleared field, sent as `null`, not `"   "` (Task 1 test `test_patch_blank_is_null`).
3. Note content with quotes, backslashes, newlines and control characters must round-trip through `noteBodyJson` as valid JSON (Task 2 test `test_note_body_escapes`).
4. A note's list title: leading blank lines skipped, a markdown `#` heading marker stripped, and the 120-byte cap never splits a UTF-8 sequence (Task 2 test `test_note_first_line_rules`).
5. Missing optional fields (`icon` on a guild, `updatedAt` on a note, `content` on a reply) parse to empty values, not a failed parse (Tasks 1 and 2 tests `test_user_guilds_missing_icon`, `test_note_list_missing_fields`).

---

### Task 1: Profile parsers and the PATCH body (lib/core)

**Files:**
- Modify: `lib/core/src/people/profile.h`, `lib/core/src/people/people_parse.{h,cpp}`
- Create: `lib/core/src/people/profile_edit.{h,cpp}`
- Modify: `test/fixtures/people_fixtures.h`, `test/test_people/test_main.cpp`

**Interfaces:**
- Produces (in `profile.h`):
  - `struct UserReply { std::string replyId, postId, content; int64_t createdMs = -1; };`
  - `struct UserGuild { std::string slug, name, icon, role; };`
- Produces (in `people_parse.h`):
  - `bool parseUserReplies(std::string_view body, std::vector<UserReply>& out, std::string& cursor);` deleted replies skipped; content cut to `kUserReplyBytes = 512` bytes on a UTF-8 boundary; cursor "" when null.
  - `bool parseUserGuilds(std::string_view body, std::vector<UserGuild>& out);` order kept.
- Produces (in `profile_edit.h`):
  - `struct ProfileFields { std::string displayName, bio, websiteUrl, websiteName, locationName; };`
  - `ProfileFields profileFields(const Profile&);`
  - `std::string profilePatchJson(const ProfileFields& before, const ProfileFields& after);` only changed fields; a value that is empty after trimming spaces is `null`; `""` when nothing changed. Keys: `displayName`, `bio`, `websiteUrl`, `websiteName`, `locationName`.
  - `const char* profileFieldError(const ProfileFields&);` nullptr or one of: `"Display name: 64 characters at most"`, `"Bio: 640 characters at most"`, `"Website: 2048 characters at most"`, `"Website must start with http:// or https://"`, `"Website name: 64 characters at most"`, `"Location: 64 characters at most"`.

- [x] **Step 1: Write the failing tests** in `test/test_people/test_main.cpp` (fixtures `kUserReplies`, `kUserRepliesEnd`, `kUserGuilds`, `kUserGuildsNoIcon` in `people_fixtures.h`):

```cpp
void test_user_replies_parse();          // 2 rows + 1 deleted skipped; ids, postId, content, createdMs; cursor "r2"
void test_user_replies_end_cursor();     // cursor null -> ""
void test_user_guilds_parse();           // member first then apprentice; slug, name, icon "🦉", role
void test_user_guilds_missing_icon();    // icon "" and parse ok
void test_patch_only_changed();          // bio changed -> {"bio":"new"} exactly
void test_patch_blank_is_null();         // displayName "  " -> {"displayName":null}
void test_patch_no_change_empty();       // == "" 
void test_patch_escapes();               // bio with "\"\n" -> valid JSON (parse back with ArduinoJson, equals input)
void test_profile_field_error_limits();  // 64/65 display name, 640/641 bio, 64/65 site name and location
void test_profile_field_error_url();     // "ftp://x" error; "" ok; "https://a" ok; 2049 chars error
void test_profile_field_error_counts_chars(); // 640 x "🙂" ok, 641 error
```

- [x] **Step 2: Run** `pio test -e native -f test_people` — Expected: FAIL to compile (`parseUserReplies` undeclared).
- [x] **Step 3: Implement** the Produces above (ArduinoJson as `people_parse.cpp` does; character count = UTF-8 lead bytes; `profile_edit.cpp` builds JSON with ArduinoJson `JsonDocument`, adding `nullptr` for a cleared field).
- [x] **Step 4: Run** `pio test -e native -f test_people` — Expected: PASS, all tests.
- [x] **Step 5: Commit** `git commit -m "People: parse a user's replies and guilds, build the profile PATCH"`

### Task 2: Notes model and parsers (lib/core)

**Files:**
- Create: `lib/core/src/notes/notes.{h,cpp}`, `test/fixtures/notes_fixtures.h`, `test/test_notes/test_main.cpp`

**Interfaces:**
- Produces (`notes/notes.h`, namespace `rc`):
  - `constexpr size_t kNoteMaxChars = 32768, kNoteTitleBytes = 120;`
  - `struct NoteRow { std::string id, title; int64_t updatedMs = -1; int revision = 0; };`
  - `struct Note { std::string id, content; int64_t updatedMs = -1; int revision = 0; };`
  - `std::string noteTitle(std::string_view content);` first non-blank line, a leading run of `#` and the space after it stripped, trimmed, cut to `kNoteTitleBytes` on a UTF-8 boundary; `""` when none.
  - `bool parseNoteList(std::string_view body, std::vector<NoteRow>& out, std::string& cursor);` deleted notes skipped; content reduced to `title` (bodies not kept). Also used for `/revisions` (rows' `revision` from `revisionNumber`).
  - `bool parseNote(std::string_view body, Note& out);` false without `noteId`.
  - `bool parseNoteId(std::string_view body, std::string& id);` mutation responses (`data.noteId`).
  - `std::string noteBodyJson(const std::string& content);` `{"content":"…"}`.
  - `const char* noteError(const std::string& content);` nullptr, `"Note is empty"` (blank after trimming whitespace) or `"Note is too long"` (> 32,768 characters).

- [x] **Step 1: Write the failing tests** in `test/test_notes/test_main.cpp`:

```cpp
void test_note_list_parse();          // 2 notes + 1 deleted; ids, titles, updatedMs, revision; cursor
void test_note_list_missing_fields(); // no updatedAt/revisionNumber -> -1 / 0, still parsed
void test_note_list_drops_bodies();   // title only, sizes <= kNoteTitleBytes for a 32 KB note
void test_note_parse();               // content, revision 3, updatedMs; no noteId -> false
void test_note_id_parse();            // {"data":{"noteId":"n9"}} -> "n9"
void test_note_first_line_rules();    // "\n\n# Shopping\nmilk" -> "Shopping"; "   \n" -> ""; 200 x "é" cut <=120 bytes, valid UTF-8
void test_note_body_escapes();        // quotes, backslash, \n, \t, \x01 round-trip via ArduinoJson
void test_note_error();               // "" and " \n" empty; 32768 x "a" ok; 32769 too long; 32768 x "🙂" ok
```

- [x] **Step 2: Run** `pio test -e native -f test_notes` — Expected: FAIL to compile (`notes/notes.h` missing).
- [x] **Step 3: Implement** `notes.cpp` (ArduinoJson; times with `parseIsoMs` from `feed/feed_parse.h`).
- [x] **Step 4: Run** `pio test -e native -f test_notes` — Expected: PASS.
- [x] **Step 5: Commit** `git commit -m "Notes: model, parsers and the write body"`

### Task 3: The Notes flag and Home item (lib/core + envs)

**Files:**
- Modify: `lib/core/src/feature_flags.h`, `lib/core/src/ui/home_items.{h,cpp}`, `test/test_home_items/test_main.cpp`, `platformio.ini` (`cardputer_adv`, `x4` build flags)

**Interfaces:**
- Produces: `RC_FEAT_NOTES` (default `RC_FEAT_FEED`); `Features` gains `bool notes = true;` after `circ` (existing brace-initialisers keep compiling); `kBuiltFeatures` sets it from `RC_FEAT_NOTES`; `HomeItem::Notes` appended to the enum's end (values are not saved, but append anyway), listed after Following and before Guilds when `f.notes`.

- [x] **Step 1: Write the failing tests**: update `test_all_features_keeps_todays_order` to expect `… Following, Notes, Guilds, Profile, Settings`; add `test_notes_off_drops_notes` (`Features f{true,true,true,true,true}; f.notes=false;` -> today's nine) and `test_span_first_with_notes` (all on: 10 items, `homeSpanFirst` false; Unread off: 9, false).
- [x] **Step 2: Run** `pio test -e native -f test_home_items` — Expected: FAIL (`HomeItem::Notes` undeclared).
- [x] **Step 3: Implement**; add `-DRC_FEAT_NOTES=0` to the `cardputer_adv` and `x4` envs' `build_flags`.
- [x] **Step 4: Run** `pio test -e native -f test_home_items` — Expected: PASS. Then `pio test -e native` — Expected: all pass.
- [x] **Step 5: Commit** `git commit -m "Notes: feature flag and Home item"`

### Task 4: Profile network side (CsApi + PeopleWorker)

**Files:**
- Modify: `src/cs/cs_api.{h,cpp}`, `src/app/messages.h`, `src/app/people_worker.{h,cpp}`, `src/store/locked_people.h`

**Interfaces:**
- Consumes: Task 1 parsers and `profilePatchJson`.
- Produces:
  - `ApiError CsApi::userReplies(const std::string& username, const std::string& cursor, int limit, std::vector<UserReply>&, std::string& next);` (`GET /v1/users/:u/replies`)
  - `ApiError CsApi::userGuilds(const std::string& username, std::vector<UserGuild>&);`
  - `ApiError CsApi::updateProfile(const std::string& json);` (`PATCH /v1/users/me`)
  - `CmdType::OpenUserReplies` (a = username, n = 1 append), `CmdType::OpenUserGuilds` (a = username), `CmdType::SaveProfile` (a = JSON), all in `isPeopleCommand`.
  - `EvtType::UserRepliesUpdated` (ok=false on failure), `EvtType::UserGuildsUpdated` (ok=false on failure), `EvtType::ProfileSaved` (ok; text = error when !ok).
  - `PeopleState` gains `std::string listUser; std::vector<UserReply> replies; std::string repliesCursor; std::vector<UserGuild> guilds;`
  - SaveProfile success re-runs `openProfile(app().user)` so `ProfileUpdated` follows. 429 text: `"Profile can be saved 2 times a minute, 15 a day"`. URL-escape usernames as the existing `profile()` does. Under `RC_FAKE_API` the commands answer with empty lists / ok=false "Not in the demo".

- [x] **Step 1: Implement** the Produces (limit 20 for replies, as `kPostsLimit` pattern).
- [x] **Step 2: Build** `pio run -e m9` — Expected: SUCCESS.
- [x] **Step 3: Run** `pio test -e native` — Expected: all pass.
- [x] **Step 4: Commit** `git commit -m "People: load a user's replies and guilds, save your profile"`

### Task 5: Profile screens (Replies, Guilds, Edit)

**Files:**
- Modify: `src/screens/profile.cpp`, `src/screens/screens.h`, `src/screens/router.{h,cpp}`
- Create: `src/screens/user_replies.cpp`, `src/screens/user_guilds.cpp`, `src/screens/profile_edit.cpp`

**Interfaces:**
- Consumes: Task 4 commands, events and `PeopleState` fields; Task 1 `ProfileFields`, `profileFields`, `profilePatchJson`, `profileFieldError`.
- Produces: `router::Id::UserReplies`, `UserGuilds`, `ProfileEdit` (appended to `Id`); `UserRepliesScreen`, `UserGuildsScreen`, `ProfileEditScreen` (`#if RC_FEAT_PEOPLE`; Replies also `RC_FEAT_ENTRIES`).

- [x] **Step 1: Profile links**: `ProfileAct` gains `kReplies, kGuilds, kEdit`; `buildLinks` order Entries, Replies, Bookmarks (self), Edit (self), Guilds, Guild, Website, Refresh. Replies -> `router::go(Id::UserReplies, user_)`; Guilds -> `Id::UserGuilds`; Edit -> `Id::ProfileEdit`.
- [x] **Step 2: `UserRepliesScreen`**, modelled on `MembersScreen` (paging on the last row, `asked_`/`failed_`, side nav, hints). Status bar `"replies · @<user>"`. Row: `ListItem{<first line of content, markdown via feed/markdown_text plain text>, <date as feed rows show it>}`. Empty `"no replies yet"`. Select: `router::go(Id::Entry, postId + "\n\n" + replyId)`. Back: `Id::Profile` with the username.
- [x] **Step 3: `UserGuildsScreen`**: status `"guilds · @<user>"`; rows `ListItem{icon + " " + name, role}`; empty `"not in a guild"`; Select opens `Id::Guild` with the slug when `RC_FEAT_GUILDS`; Back to the profile.
- [x] **Step 4: `ProfileEditScreen`**: rows Display name, Bio, Website URL, Website name, Location (value or dim `"not set"`), then `Save`. Select on a field opens an `InputField` overlay filled with the value (Bio a multi-line `TextArea` box as the Composer's body); Enter keeps, Back drops. Save: `profileFieldError` -> banner; `profilePatchJson` `""` -> back to the profile; else `SaveProfile`, hint `"saving…"`, ignore keys but Back until `ProfileSaved`. `ProfileSaved` ok -> banner `"Profile saved"`, back to the profile; !ok -> banner `e.text`, edits kept. Back with changes -> confirm `"Discard changes?"`.
- [x] **Step 5: Build** `pio run -e m9` and `pio run -e wio_l2` — Expected: SUCCESS. `pio test -e native` and `pio test -e native_lv` — Expected: all pass.
- [x] **Step 6: Commit** `git commit -m "Profiles: Replies, Guilds and Edit profile"`

### Task 6: Notes network side (CsApi + FeedWorker)

**Files:**
- Modify: `src/cs/cs_api.{h,cpp}`, `src/app/messages.h`, `src/app/feed_worker.{h,cpp}`
- Create: `src/store/locked_notes.{h,cpp}`

**Interfaces:**
- Consumes: Task 2 parsers, `noteBodyJson`.
- Produces:
  - `CsApi::notes(cursor, limit, std::vector<NoteRow>&, std::string& next)`, `note(id, int revision /*0 latest*/, Note&)`, `noteRevisions(id, cursor, limit, std::vector<NoteRow>&, std::string& next)`, `createNote(json, std::string& id)`, `updateNote(id, json)`, `deleteNote(id)`.
  - `CmdType::OpenNotes` (n = 1 append), `OpenNote` (a = id, n = revision, 0 latest), `OpenNoteHistory` (a = id, n = 1 append), `SaveNote` (a = id or "", b = JSON), `DeleteNote` (a = id); in `isFeedCommand`, all `#if RC_FEAT_NOTES`.
  - `EvtType::NotesUpdated`, `NoteUpdated`, `NoteHistoryUpdated` (ok=false on failure), `NoteSaved` (ok; text = noteId, or error when !ok), `NoteDeleted` (ok; text = error when !ok).
  - `struct NotesState { std::vector<NoteRow> list; std::string listCursor; Note open; int latestRevision = 0; std::vector<NoteRow> history; std::string historyCursor; std::string historyId; bool loading = false; };` behind `LockedNotes lockedNotes()` (same shape as `LockedPeople`); cleared by `CmdType::CloseNotes` sent when Notes is left for Home, and on sign-out.
  - 429 on create: `"Notes can be created 3 times a minute, 30 a day"`; otherwise `errorText` as `PeopleWorker`. No retries. `RC_FAKE_API`: empty list, writes refused "Not in the demo".

- [x] **Step 1: Implement** the Produces (limit 20).
- [x] **Step 2: Build** `pio run -e m9` and `pio run -e cardputer_adv` — Expected: SUCCESS both (Notes compiled out on the latter).
- [x] **Step 3: Run** `pio test -e native` — Expected: all pass.
- [x] **Step 4: Commit** `git commit -m "Notes: API calls and FeedWorker commands"`

### Task 7: Notes screens and the Composer's Note mode

**Files:**
- Create: `src/screens/notes.cpp` (`NotesScreen`, `NoteScreen`, `NoteHistoryScreen`)
- Modify: `src/screens/screens.h`, `src/screens/router.{h,cpp}`, `src/screens/home.cpp`, `src/screens/composer.{h,cpp}`

**Interfaces:**
- Consumes: Task 3 `HomeItem::Notes`; Task 6 commands, events, `lockedNotes()`; Task 2 `noteError`, `noteBodyJson`, `kNoteMaxChars`.
- Produces: `router::Id::Notes`, `Note` (arg `id` or `"id\nN"`), `NoteHistory` (arg id); Composer keys `"note:"` (new) and `"note:<id>"` (edit).

- [x] **Step 1: Home**: tile and row label `Notes` for `HomeItem::Notes` opening `Id::Notes` (icon/glyph as its neighbours use).
- [x] **Step 2: `NotesScreen`** (as `MembersScreen`): row 0 pinned `"+ New note"` -> Composer `"note:"`; rows `ListItem{title or "(empty)", date}`; empty (besides the pinned row) shows `"no notes yet"`; paging; Select opens `Id::Note` with the id; Back -> Home and sends `CloseNotes`.
- [x] **Step 3: `NoteScreen`**: `OpenNote`; `TextBox` with an `rc::TextView` of the content; status `"note · rev N"` or `"note · rev N of M"` for an old revision. Options/Palette on the latest revision: menu Edit (Composer `"note:<id>"`), History (`Id::NoteHistory`), Delete (confirm `"Delete this note and all its revisions?"` -> `DeleteNote`; `NoteDeleted` ok -> `Id::Notes` refreshed, !ok -> banner). Old revision: no menu. Back: latest -> `Id::Notes`; old -> `Id::NoteHistory`.
- [x] **Step 4: `NoteHistoryScreen`**: rows `ListItem{"rev N", date}`, paging; Select -> `Id::Note` with `"id\nN"`; Back -> `Id::Note` with the id.
- [x] **Step 5: Composer Note mode**: in `enter`, key `"note:"…` sets a note mode: only the Body field (Title, Topics, Public, NSFW, CW hidden and skipped), status `"new note"` / `"edit note"`, Send labelled `Save`, body prefilled from `lockedNotes().open.content` when editing, its own draft (never the Feed draft). Save: `noteError` -> banner; else `SaveNote` with `noteBodyJson`. `NoteSaved` ok -> `Id::Note` with the id; !ok -> banner `e.text`, text kept. Discard confirm as today.
- [x] **Step 6: Build** `pio run -e m9`, `pio run -e wio_l2`, `pio run -e cardputer_adv` — Expected: SUCCESS. `pio test -e native` and `pio test -e native_lv` — Expected: all pass.
- [x] **Step 7: Commit** `git commit -m "Notes: list, read, write, history and delete"`

### Task 8: Docs

**Files:**
- Modify: `docs/USE.md` (Profile: Replies, Guilds, Edit; a Notes section), `docs/TESTING.md` (open hardware checks for both), `AGENTS.md` (spec/plan links; Progress line), `RELEASE_NOTES.md` (next version's notes, under a new heading; no version number until release)

- [x] **Step 1: Write** the docs.
- [x] **Step 2: Run** `pio test -e native` — Expected: all pass (no code change).
- [x] **Step 3: Commit** `git commit -m "Docs: fuller profiles and Notes"`
