# Monster Cyberspace M2 (C-Mail) Implementation Plan

> Work the tasks in order, tick the boxes, and commit once per task. Follow
> test-driven development wherever a step has host tests. M1's Global
> Constraints apply (`2026-10-25-cyberspace-m9-circ.md`): never call the live
> API from an agent, and every write must come from a key press.

**Spec:** `docs/superpowers/specs/2026-10-25-cyberspace-m9-cmail-design.md`

### Task 1: Core parsing, typing state, limits, palette

**Files:**

- `lib/core/src/cs/{api_parse,message_json,rtdb,rate_limiter}.{h,cpp}`
- `lib/core/src/chat/typing.{h,cpp}`
- `lib/core/src/ui/widgets/palette.{h,cpp}`
- `test/fixtures/{api,rtdb}_fixtures.h`
- the matching tests

- [x] **Step 1: Write the tests.**
  - `parseConversations` (unread, empty `lastMessage`, a deleted partner);
  - `parseStartConversation`;
  - `parseTypingAck`;
  - history with `senderId`/`senderUsername`;
  - `applyTypingEvent` (put window, put child, null, patch) and `typingNames` (stale, self excluded);
  - `makeCmailSendLimiter`, `makeStartConversationLimiter`, `makeTypingLimiter`;
  - `paletteEntries(PaletteKind::Cmail)` excludes `/art` and `/muted`.
- [x] **Step 2: Run** `pio test -e native` and watch the new tests fail.
- [x] **Step 3: Implement**, then run them green.
- [x] **Step 4: Commit** `feat(core): C-Mail parsing, typing state, limits`.

### Task 2: API client, commands and shared state

**Files:** `src/cs/cs_api.{h,cpp}`, `src/app/messages.h`, `src/store/locked_chat.h`

- [x] **Step 1:** Add the `CsApi` calls:
  - `conversations`, `startConversation`, `cmailHistory`, `cmailSend`;
  - `cmailRead`, `cmailTyping`, `cmailTypingClear`.

  Each write checks its local limiter first and is never retried.
- [x] **Step 2:** Add the commands `OpenConversation`, `StartConversation`,
  `RefreshConversations` and `Typing` (n = 1 on, 0 off), and the events
  `ConversationsUpdated`, `ConversationStarted`, `TypingChanged` and
  `CmailAlert`. Add `dm`, `conversations`, `typing` and `typingCadence` to
  `ChatState`.
- [x] **Step 3: Run** `pio run -e m9`. Expected: SUCCESS.

  This task is committed together with Task 3, because the commands are not
  handled until then.

### Task 3: Net worker C-Mail

**Files:** `src/app/net_worker.cpp`

- [x] **Step 1:** Open and close a conversation as a channel:
  - history and older pages;
  - the `dm_messages` stream (`limitToLast` ≤ 100);
  - the `dm_presence` stream into `typing`;
  - send and mark read;
  - no presence calls.
- [x] **Step 2:** Handle `StartConversation` (`ConversationStarted` carries
  the id, or an error), `RefreshConversations` and `Typing`.
- [x] **Step 3:** In the watcher, refresh conversations and raise
  `CmailAlert` for each new unread message (skipping the open conversation).
  Bootstrap also loads the list.
- [x] **Step 4: Run** `pio run -e m9`, then **commit**
  `feat: C-Mail API client and network worker`.

### Task 4: Fake source C-Mail

**Files:** `src/chat/fake_chat_source.cpp`

- [x] **Step 1:** Add fixture conversations: rain with 2 unread, moth, and
  kiwi with no messages. Opening one loads its history. A send echoes; the
  partner then shows as typing for 2 s and replies.
- [x] **Step 2:** Every 45 s, rain sends a DM, which raises `CmailAlert`
  unless that conversation is open. `StartConversation` creates or returns
  a conversation, and `"nobody"` returns a "User not found" error.
- [x] **Step 3: Run** `pio run -e m9_fake`, then **commit**
  `feat(fake): scripted C-Mail`.

### Task 5: Screens

**Files:**

- `src/screens/{cmail_list.cpp,screens.h,room.{h,cpp},router.{h,cpp},home.cpp}`
- `src/app/{ui_task.cpp,app_state.h}`

- [x] **Step 1:** Write `CmailListScreen`: the New conversation… entry with
  its username input, the conversation rows, and refresh on enter.
- [x] **Step 2:** Add a DM kind to `RoomScreen`:
  - `@name` title and no drawer;
  - the conversation menu and the C-Mail palette;
  - message menu without reply, delete or report;
  - the typing indicator, plus typing on/off sent from key presses.
- [x] **Step 3:** Wiring:
  - the router gets `Id::CmailList` and `Id::Conversation`, and Fn under
    Home goes to the list;
  - the UI task handles `CmailAlert` with a banner and buzzer (the unread
    count is summed from the conversation list, so there's no `unreadCmail`);
  - Home gets a "c-mail" line and a C-Mail menu entry.
- [x] **Step 4: Run** `pio test -e native && pio run -e m9 -e m9_fake`.
- [ ] **Step 5: Hardware check (human) on `m9_fake`:**
  - the list, opening, sending and the typing indicator;
  - an alert while in a room;
  - a new conversation, and the "nobody" error.
- [x] **Step 6: Commit** `feat: C-Mail screens`.

### Task 6: Docs

- [x] **Step 1:** Add C-Mail to `docs/USE.md` and `docs/TESTING.md`
  (fake-build checks and a live checklist section). Update AGENTS.md's
  hand-off and README.
- [x] **Step 2: Commit** `docs: C-Mail usage and checklist`.
