# Chat Style (IRC or Bubbles) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a "Chat style" setting. IRC is today's log, and Bubbles is a messenger look. Bubbles applies to rooms, C-Mail conversations, and Feed and guild-thread replies.

**Architecture:**
- Bubbles are rows of the existing room log. `layoutRoomEach` and `layoutEntry` tag each `DisplayLine` with its bubble's side and column range.
- `lvui::LogBox` fills a rounded rectangle behind those rows, in both object and painted (lean) modes.
- The setting is saved like `textSize`: by index in NVS, by key in backups and web config.

**Tech Stack:** C++17, PlatformIO, Unity tests (`native`, `native_lv`), LVGL 9.5, ArduinoJson.

**Spec:** `docs/superpowers/specs/2026-11-09-cyberspace-chat-style-design.md`

## Global Constraints

- The default is `ChatStyle::Irc`. With Irc, layout output is byte-for-byte unchanged, and every existing test passes untouched.
- `enum class ChatStyle : uint8_t { Irc = 0, Bubbles = 1 }` is saved by index. Append new styles; never reorder.
- The keys are `"irc"` and `"bubbles"`; the labels are `"IRC"` and `"Bubbles"`. The NVS key is `cstyle`. The backup key is `chatStyle`.
- Bubble text wraps at `inner = max(8, cols * 3 / 4) - 2` columns. Bubble width is `longest wrapped line + 2`. Others' bubbles start at col 0; yours at `cols - width`.
- Colours: others' bubbles are `blend(bg, fg, 15%)`; yours are `blend(bg, selfNick, 30%)`. With `Theme::outlined`, bubbles get a 1 px `fg` outline and no fill.
- Corner radius is `min(cellH / 2, 6)`. Only the top corners are rounded on a `bubbleTop` row, and only the bottom corners on a `bubbleBottom` row.
- `src/grid_ui/` (the X4) is never modified.
- Never call the live Cyberspace API. Never log passwords or tokens.
- This ships in **2.1.0**. Do not release, and do not push, until the user says so.
- Commit after every task, with the trailer `Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>`.
- Hardware builds: only `m9` and `wio_l2`. Never flash unless the user asks.

## Review Focus

1. **A one-word message.** It gives a bubble of width `textWidth + 2`, one row, with both `bubbleTop` and `bubbleBottom` set. Test in Task 3.
2. **Wide emoji (two cells) and a very narrow log.** At `cols < 12`, `inner` is still at least 6. Bubble rows never exceed `cols`, and a self bubble's `bubbleCol` is never negative. Test in Task 3.
3. **The selected message scrolled half off screen.** The outline covers only the bubble rows that are shown, and never the meta line. Test in Task 5.
4. **Muted users and deleted messages.** A muted user's message gives no lines, as today. A deleted message is a bubble holding a dim `[deleted]`. Test in Task 3.
5. **Switching style while a room is open.** The log re-lays out on the next frame, and the selection is kept, because ids are unchanged. Covered in Task 6 by the screens' relayout check (same mechanism as text size) and the hardware check.

---

### Task 1: The ChatStyle setting in lib/core

**Files:**
- Modify: `lib/core/src/ui/grid.h` (after `TextSize` and its helpers, ~line 166-172) and `lib/core/src/ui/grid.cpp`
- Modify: `lib/core/src/config/settings_model.h` (add `ChatStyle chatStyle = ChatStyle::Irc;` after `textSize`)
- Modify: `lib/core/src/config/backup_yaml.cpp` (follow `textSize` at ~67, 279, 421, 530, 602)
- Test: `test/test_layout/test_main.cpp` (helpers) and `test/test_backup_yaml/test_main.cpp`

**Interfaces:**
- Produces, all in namespace `rc` in `ui/grid.h`:
  - `enum class ChatStyle : uint8_t { Irc = 0, Bubbles = 1 };`
  - `ChatStyle nextChatStyle(ChatStyle)`: wraps around.
  - `ChatStyle chatStyleFromIndex(int)`: out of range gives Irc.
  - `const char* chatStyleKey(ChatStyle)`
  - `bool parseChatStyle(std::string_view, ChatStyle&)`: leaves `out` alone on failure.
  - `std::u32string chatStyleLabel(ChatStyle)`: match the type `textSizeLabel` returns.
  - `Settings::chatStyle`

- [ ] **Step 1: Write the failing tests.**
  - `test_chat_style_helpers`:
    - `nextChatStyle(Irc) == Bubbles` and `nextChatStyle(Bubbles) == Irc`.
    - `chatStyleFromIndex(1) == Bubbles` and `chatStyleFromIndex(7) == Irc`.
    - The keys are `"irc"` and `"bubbles"`.
    - Parsing `"bubbles"` succeeds; parsing `"x"` fails and leaves the value unchanged.
    - The labels are `U"IRC"` and `U"Bubbles"`.
  - In test_backup_yaml:
    - `test_chat_style_round_trip`: a Settings with Bubbles writes `chatStyle: bubbles`, and parses back to Bubbles.
    - `test_chat_style_bad_value`: `chatStyle: fancy` gives an error naming its line, with the text `chat style: irc or bubbles`.
    - `test_chat_style_missing_keeps`: a backup without the key leaves Bubbles as Bubbles.
- [ ] **Step 2: Run them and check they fail.** Run `pio test -e native -f test_layout -f test_backup_yaml`. Expected: a compile error, because ChatStyle is undeclared.
- [ ] **Step 3: Implement the helpers, the field and the YAML.**
  - YAML: add `chatStyle` to the `Str` list.
  - Write it after `textSize` as `chatStyle: <key>  # irc or bubbles`.
  - Validate and apply it like textSize.
- [ ] **Step 4: Run all native tests and check they pass.** Run `pio test -e native`. Expected: everything passes (759+ cases).
- [ ] **Step 5: Commit** with the message "Chat style setting: type, helpers, backup".

### Task 2: Bubble colours

**Files:**
- Modify: `lib/core/src/ui/theme.h` and `lib/core/src/ui/theme.cpp`
- Test: `test/test_theme/test_main.cpp`

**Interfaces:**
- Produces:
  - `uint16_t blend565(uint16_t a, uint16_t b, int pctB)` in `ui/theme.h`. It mixes each channel as `a + (b - a) * pctB / 100`, rounded toward a.
  - `uint16_t bubbleFill(const Theme& t, bool self)`: `blend565(t.bg, self ? t.selfNick : t.fg, self ? 30 : 15)`.

- [ ] **Step 1: Write the failing tests.**
  - `test_blend565`:
    - `blend565(0x0000, 0xFFFF, 0) == 0x0000`
    - `blend565(0x0000, 0xFFFF, 100) == 0xFFFF`
    - `blend565(0x0000, 0xF800, 50)` has red 15 and green 0.
  - `test_bubble_fill`:
    - For `defaultTheme()`, `bubbleFill(t, false) == blend565(t.bg, t.fg, 15)` and `bubbleFill(t, true) == blend565(t.bg, t.selfNick, 30)`.
    - Both differ from `t.bg`, and so for `theme(ThemeId::Light)`.
- [ ] **Step 2: Run it and check it fails.** Run `pio test -e native -f test_theme`.
- [ ] **Step 3: Implement both functions.**
- [ ] **Step 4: Run it and check it passes.** Run `pio test -e native -f test_theme`.
- [ ] **Step 5: Commit** with the message "Theme: bubble fill colours".

### Task 3: Bubbles in the room layout

**Files:**
- Modify: `lib/core/src/ui/room_layout.h` and `lib/core/src/ui/room_layout.cpp`
- Test: `test/test_layout/test_main.cpp`

**Interfaces:**
- Consumes: `ChatStyle` (Task 1).
- Produces:
  - `DisplayLine` gains:
    ```cpp
    enum class Bubble : uint8_t { None, Other, Self };
    Bubble bubble = Bubble::None;
    uint8_t bubbleCol = 0, bubbleCols = 0;
    bool bubbleTop = false, bubbleBottom = false;
    ```
    These are members of `DisplayLine`, so callers write `DisplayLine::Bubble::Self`.
  - `LayoutOptions::style = ChatStyle::Irc`.

- [ ] **Step 1: Write the failing tests.** Each uses `opts(TextSize::Compact, 40)` with `style = Bubbles`; the self username is "me".
  - `test_bubbles_other`: "bob" says "hello" at 14:02.
    - Line 0 is the day divider, unchanged.
    - Line 1 is the meta line, with `bubble == None` and text `"bob 14:02"`. Its spans: the nick in `nickColor`, then `" 14:02"` in `th.time`.
    - Line 2 has `bubble == Other`, `bubbleCol == 0`, `bubbleCols == 7`, both top and bottom set, and text `" hello"`.
    - Both lines carry the message id.
  - `test_bubbles_self`: "me" says "hi".
    - The meta line is `"14:02"` right-aligned: its text width is 40, and it ends with `"14:02"`.
    - The bubble row has `bubble == Self`, `bubbleCols == 4` and `bubbleCol == 36`, and its text is 36 spaces then `" hi"`.
  - `test_bubbles_wrap`: a 60-character message at cols 40.
    - `inner == 28`.
    - Every bubble row's text, after `bubbleCol`, is at most 30 wide.
    - Only the first row is top; only the last is bottom.
    - All rows share `bubbleCols` = longest + 2.
  - `test_bubbles_narrow`:
    - At cols 10, a message from "me" with the text "abcdefghijkl": every row's `bubbleCol + bubbleCols <= 10`, and `bubbleCol >= 0`.
    - A wide emoji (U+1F600) never splits across rows.
  - `test_bubbles_deleted_and_muted`:
    - A deleted message gives a bubble row with the text `" [deleted]"`, whose span is in `th.dim`.
    - A muted user gives no lines.
  - `test_bubbles_action`: the action "/me waves" from bob gives no meta nick, and its bubble text is `" * bob waves"` in `th.sys`.
  - `test_bubbles_each_matches_layout`: for 5 mixed messages, `layoutRoomEach(…, 2, 5, …)` gives exactly `layoutRoom`'s lines for messages 2-4, including the divider rule.
  - `test_irc_unchanged`: `opts(...)` with the default style equals today's output. This is already covered by the existing tests; just make sure none change.
- [ ] **Step 2: Run them and check they fail.** Run `pio test -e native -f test_layout`.
- [ ] **Step 3: Implement it in `layoutRoomEach`.**
  - With `o.style == ChatStyle::Bubbles`, branch after the body blocks are built and `artLines` are computed. The header and indent logic are not used.
  - Emit the meta line:
    - Others: the nick (cut with `cutNick(nick, cols - 7)`), then `" " + HH:MM`.
    - Yours: spaces to `cols - 5`, then HH:MM.
    - For an action, the meta line is the time only, on the action's side.
  - Wrap each block with `wrapText(text, inner, inner)`. Art lines go in as they are, clipped to `inner`.
  - Width = the max `textWidth` + 2, capped at `cols`.
  - Each row: if `bubbleCol > 0`, a pad span of `bubbleCol` spaces; then `" " + text` in the block's colour.
  - Set the bubble fields, `mentionTint` and `messageId` on every row.
  - Dividers are unchanged. Clip every row to `cols`.
- [ ] **Step 4: Run it and check it passes.** Run `pio test -e native`. Expected: all pass, the old layout tests included.
- [ ] **Step 5: Commit** with the message "Room layout: Bubbles chat style".

### Task 4: Bubbles for Feed and guild replies

**Files:**
- Modify: `lib/core/src/ui/feed_layout.h` (`EntryLayoutOptions` gets `ChatStyle style = ChatStyle::Irc;`) and `lib/core/src/ui/feed_layout.cpp` (reply loop, ~145-185)
- Test: `test/test_layout/test_main.cpp` (beside the existing `layoutEntry` tests, ~587)

**Interfaces:**
- Consumes: `LayoutOptions::style` and the `DisplayLine` bubble fields (Task 3).

- [ ] **Step 1: Write the failing test.** `test_entry_bubbles_replies_only`:
  - With `style = Bubbles`, the header, body and `── N replies ──` lines equal the Irc output's, and all have `bubble == None`.
  - A reply from "ann" to parent "bob" gives:
    - a meta line whose text starts `"ann \u21b3 @bob "` and ends in the time;
    - then bubble rows with `bubble == Other`.
  - With Bubbles there is no separate `↳` line.
- [ ] **Step 2: Run it and check it fails.** Run `pio test -e native -f test_layout`.
- [ ] **Step 3: Implement it.**
  - Set `lo.style = o.style`.
  - With Bubbles, skip the separate parent line. Instead, put the parent in the meta line: add a `std::string metaExtra` to `LayoutOptions`, used only by Bubbles. When it's set, the meta line becomes `nick + " ↳ @parent " + time`.
- [ ] **Step 4: Run it and check it passes.** Run `pio test -e native`.
- [ ] **Step 5: Commit** with the message "Entry layout: Bubbles for replies".

### Task 5: LogBox draws bubbles

**Files:**
- Modify: `lib/lvui/src/lvui/views/log_box.h` and `lib/lvui/src/lvui/views/log_box.cpp`
- Test: `test/test_lv_views_text/test_main.cpp` (beside `test_log_nick_colour` and `test_log_painted_*`)

**Interfaces:**
- Consumes: the `DisplayLine` bubble fields (Task 3), `bubbleFill` (Task 2) and the theme lvui already holds (see `styles`/`applyTheme` for how LogBox reaches the current `rc::Theme`).
- Produces test accessors on LogBox:
  - `struct BubbleShown { int col, cols; uint16_t fill; bool top, bottom, outlined; }`
  - `bool bubbleAt(int r, BubbleShown& out) const`: false when row r has no bubble.
  - `lv_area_t selectionArea() const`: in cells, for tests.

- [ ] **Step 1: Write the failing tests.** Build a `LogView` from `layoutRoom` with Bubbles, at the box's columns.
  - `test_log_bubble_rows`:
    - For an "other" message, the bubble row reports `col 0`, the layout's `bubbleCols`, `fill == bubbleFill(theme, false)` and `top`/`bottom` as laid out.
    - A self bubble reports `fill == bubbleFill(theme, true)`.
    - A meta row reports no bubble.
  - `test_log_bubble_painted`: the same assertions with `LogBox::setPainted(true)`.
  - `test_log_bubble_outlined`: with `paperTheme()` applied, `outlined == true`.
  - `test_log_bubble_selection`:
    - With the message selected, `selectionArea()` spans exactly the bubble's columns and rows. The meta row is excluded.
    - Scroll so the bubble's first row is off screen: the area starts at row 0.
- [ ] **Step 2: Run them and check they fail.** Run `pio test -e native_lv -f test_lv_views_text`.
- [ ] **Step 3: Implement it.**
  - **Object mode:** each `Row` gains `lv_obj_t* bubble` (a `plainBox` child of `lines_`, created before the span labels so it sits behind them). It is hidden when there is no bubble. Otherwise it is placed at `bubbleCol * cell.w` with width `bubbleCols * cell.w`, at the row's full height.
  - **Background:** `bubbleFill`, or transparent with a 1 px `fg` border when `theme.outlined`.
  - **Radius:** `min(cell.h / 2, 6)`. For a middle row, radius 0. For a top-only or bottom-only row, give the box a height of `cell.h + radius` (extending under the next or previous row), and clip it with the row's area (`LV_OBJ_FLAG_OVERFLOW_VISIBLE` off on the row). Then only the outer corners round.
  - **Painted mode:** in `paint`, before the spans, draw `lv_draw_rect` with the same geometry, clipped to the row's area. Record the bubble in `Row::Painted`, so `samePainted` sees changes.
  - **Selection:** while rows with a bubble are selected, set `sel.top`/`sel.bottom` from bubble rows only, and the x range from `bubbleCol`/`bubbleCols`. Otherwise, keep today's full-width rule.
  - Leave the mention tint as it is: it fills the row, and the bubble sits on top of it.
- [ ] **Step 4: Run it and check it passes.** Run `pio test -e native_lv`, then `PLATFORMIO_BUILD_FLAGS="-DRC_LV_LEAN=1 -DRC_LOW_MEM=1" pio test -e native_lv`. Expected: all pass, both builds.
- [ ] **Step 5: Run it under ASan.** Run `PLATFORMIO_BUILD_DIR=/tmp/asanlv PLATFORMIO_BUILD_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g" pio test -e native_lv`. Expected: all pass, with no ASan report. Then `rm -rf /tmp/asanlv`.
- [ ] **Step 6: Commit** with the message "LogBox: draw chat bubbles".

### Task 6: Wire the setting into the firmware

**Files:**
- Modify: `src/store/settings.h` and `src/store/settings.cpp`. Load it with `cfg.chatStyle = rc::chatStyleFromIndex(p.getUChar("cstyle", 0))`. Add `void saveChatStyle(rc::ChatStyle)`, and include it in the save-all path near `saveSpellCheck` at ~301.
- Modify: `src/app/ui_task.cpp` and its header (where `setTextSize` is declared). Add `void setChatStyle(rc::ChatStyle)`: it sets cfg, saves, and marks the screens for relayout.
- Modify: `src/screens/room.cpp` (`logOptions`, ~672: `o.style = app().cfg.chatStyle;`) and `src/screens/room.h`. Keep a `style_`, and relayout when `app().cfg.chatStyle != style_`, as Entry does with `size_`.
- Modify: `src/screens/entry.cpp` (~477: `o.style = app().cfg.chatStyle;`, and add `|| app().cfg.chatStyle != style_` to the dirty check at ~527) and `src/screens/entry.h`.
- Modify: `src/screens/settings.cpp`. Add an item `kChatStyle` after `kTextSize`, shown on every board. The list row is `ListItem{U"Chat style", chatStyleLabel(cfg.chatStyle)}`. The button cycles it with `ui::setChatStyle(nextChatStyle(...))`, the way `kSpellCheck` toggles (~680, ~831).
- Modify: `src/web/web_server.cpp`. getPrefs sends `d["chatStyle"] = rc::chatStyleKey(s.chatStyle)`. parsePrefs reads `chatStyle` with `parseChatStyle`; an invalid value is a 400, like textSize. Count it in `uiPrefs`. Apply and save it.
- Modify: `src/web/page.h`. A row `chatrow` with a select `chat` (options irc/IRC and bubbles/Bubbles), next to the Text size row. Its put sets `P(b).chatStyle`, and its fill selects `p.chatStyle`. Follow the `spell` entry at ~353 and ~473.

**Interfaces:**
- Consumes: Tasks 1-5.
- Produces: `ui::setChatStyle(rc::ChatStyle)` and `settings::saveChatStyle(rc::ChatStyle)`.

- [ ] **Step 1: Implement the wiring above.** For how a web pref reaches the UI task, follow the existing `textSize` path (`uiPrefs`, the UI event at ui_task ~728). If that path carries only textSize, add a `ChatStyle` event type alongside it.
- [ ] **Step 2: Check the site and native tests.** Run `node --test site/*.test.mjs && pio test -e native`. Expected: all pass.
- [ ] **Step 3: Build the boards.** Run `pio run -e m9 && pio run -e wio_l2 && git checkout -- sdkconfig.x4`. Expected: both report SUCCESS.
- [ ] **Step 4: Commit** with the message "Chat style in Settings, web config and the logs".

### Task 7: Docs and 2.1.0 notes

**Files:**
- Modify: `docs/USE.md`. Under Settings, add Chat style: what IRC and Bubbles look like, and where it applies.
- Modify: `docs/TESTING.md`. Under "Open hardware checks", add the item "Chat style: Bubbles on the M9 and Wio L2 — room, C-Mail, entry with replies; three themes; each text size; switch style with a room open."
- Modify: `AGENTS.md`. Add one line to Progress: "Chat style (IRC or Bubbles) added on 2026-11-09 for 2.1.0 (built without flashing)", with the spec and plan links in the spec list.
- Modify: `RELEASE_NOTES.md`. Replace it with `### Monster for Cyberspace 2.1.0` and a "Chat style" section (two bullets: the setting, and where it applies).

- [ ] **Step 1: Write the four edits.**
- [ ] **Step 2: Commit** with the message "Docs and 2.1.0 notes: Chat style". Do not push.
