# Chat Style: IRC or Bubbles — design

## Goal

A new setting, **Chat style**, picks how messages are drawn:

- **IRC** is today's look, and stays the default: one line per message, with
  the nick in front of the text.
- **Bubbles** draws each message as a filled bubble, the way a phone messenger
  does.

It applies to cIRC rooms, C-Mail conversations, and the replies under a Feed
entry or guild thread. The post itself keeps its header-and-body look.

## Decisions (agreed with the user)

- **Alignment:** messenger style. Your own messages sit on the right; everyone
  else's sit on the left.
- **Nick and time:** on a dim line above each bubble. Your own bubbles show
  only the time.
- **Entries:** only the replies become bubbles. The entry's header, body,
  attachments and "── N replies ──" divider keep their current look.
- **Approach:** bubbles are made of rows in the existing log, not a new view.
  `LogView`'s row scrolling, selection, touch, low-memory layout
  (`layoutRoomEach`), painted (lean) drawing and screenshots all stay as
  they are.

## Setting

- **Type:** `enum class ChatStyle : uint8_t { Irc = 0, Bubbles = 1 };` in
  `lib/core/src/ui/grid.h`, beside `TextSize`. It is saved by index, so new
  styles are appended.
- **Helpers:** `nextChatStyle`, `chatStyleFromIndex`, `chatStyleKey`
  (`"irc"`, `"bubbles"`), `parseChatStyle`, and `chatStyleLabel` (`"IRC"`,
  `"Bubbles"`).
- **Settings model:** `rc::Settings::chatStyle = ChatStyle::Irc`.
- **NVS:** stored under the key `cstyle`; an out-of-range value becomes IRC.
- **Backup YAML:** `chatStyle: irc|bubbles`. An unknown value is a parse error
  with the line number, as for `textSize`. A missing key leaves the setting
  as it is.
- **Device Settings:** a "Chat style" cycle button right after Text size. Its
  label is cut like the others (the value is dropped first). Changing it
  re-lays out the open log.
- **Web config prefs:**
  - getPrefs sends `chatStyle`, and parsePrefs accepts it.
  - The page shows a two-choice select next to Text size.
  - It counts as a UI pref, so the UI task applies it live.
- **X4:** the frozen grid UI ignores the setting. Web config and Settings still
  show it there, which is harmless: the X4 is unreleased, and the setting
  round-trips through backups.

## Layout (lib/core)

`LayoutOptions` and `EntryLayoutOptions` each gain `ChatStyle style = Irc`.
With `Irc`, the output is byte-for-byte what it is today. Existing tests
must not change.

`DisplayLine` gains:

```cpp
// Bubbles: the row is part of a bubble covering columns [bubbleCol, bubbleCol + bubbleCols).
enum class Bubble : uint8_t { None, Other, Self } bubble = Bubble::None;
uint8_t bubbleCol = 0, bubbleCols = 0;
bool bubbleTop = false, bubbleBottom = false;  // first / last row: rounded corners
```

With `Bubbles`, each normal message becomes:

1. **A meta line.**
   - For others: the dim nick in its usual nick colour, a space, and the time
     in `time`, starting at column 0.
   - For you: only the time, right-aligned.
   - Its `bubble` is `None`.
2. **The text, as bubble rows.**
   - The text wraps to `inner = max(8, cols * 3 / 4) - 2` columns.
   - The bubble is `w = longest wrapped line + 2` columns wide (one column of
     padding on each side).
   - Bubbles sit one column in from their edge (none below 8 columns): others' start at column 1; yours at `cols - w - 1`.
   - Each row's spans start with a padding space, so the text sits one cell
     inside the bubble.
   - The first row has `bubbleTop` set and the last has `bubbleBottom`; a
     one-row bubble has both.

Every row of a message still carries the message's `messageId`, so selection
and touch work as before.

Lines that stay plain in both styles:

- Day dividers and system lines (joins, notices).
- Muted users' placeholders.
- Spoilers keep their hidden form inside the bubble.
- `/me` actions are drawn as a bubble whose text is `* nick text`, with no
  meta-line nick.

Mentions keep `mentionTint` on the bubble rows. The tint fills the row behind
the bubble, so the bubble still stands out.

A reply's "↳ @parent" line becomes part of the meta line:
`nick ↳ @parent HH:MM`.

`layoutRoomEach` gives the same lines one at a time, so the X4-style
low-memory path keeps working.

## Colours

The bubble colours are worked out from the theme, so all 19 themes work with
no new fields:

- **Others:** `blend(bg, fg, 15%)`.
- **Yours:** `blend(bg, selfNick, 30%)`.
- **Text:** the theme's `fg`. Nicks inside the meta line keep their hues.

A helper `rc::bubbleFill(const Theme&, Bubble)` in `ui/theme` returns the fill
colour, with tests.

With `Theme::outlined` (e-paper), bubbles are not filled. They get a 1 px
outline in `fg` instead, and the text stays black on white.

## Drawing (lib/lvui `LogBox`)

For a row whose `bubble != None`, the box draws a rectangle behind its spans:

- **Width:** columns `[bubbleCol, bubbleCol + bubbleCols)`.
- **Height:** the full row.
- **Colour:** `bubbleFill`.
- **Corners:** radius `min(cellH/2, 6)`, rounded only on the top corners when
  `bubbleTop` is set and only on the bottom corners when `bubbleBottom` is
  set. LVGL can't round single corners, so the row draws a rounded rectangle
  and squares off the unwanted corners with a plain one.

This works the same way in both drawing modes:

- **Object mode:** one background object per row, reused, and hidden when the
  row has no bubble.
- **Painted mode:** drawn in `paint` before the spans.

**Selection:** the selected message's outline goes around the rows of its
bubble (its column range), instead of the full width. The meta line stays
outside the outline.

## Screens

- `RoomScreen` (rooms and C-Mail) and `EntryScreen` pass
  `settings.chatStyle` into their layout options.
- A change of style marks the log for re-layout, the same way a change of
  text size does.
- Nothing else changes: keys, touch, the user drawer and the composer.

## Testing

- **`native`** (room and feed layout):
  - IRC output is unchanged.
  - Bubbles: the wrap width; the side (others left, yours right); the
    `bubbleCol`/`bubbleCols` values; the top and bottom flags on single-row
    and multi-row bubbles.
  - The meta line for others and for you; day dividers and system lines stay
    plain.
  - `layoutRoomEach` gives the same lines as `layoutRoom`.
  - The entry header and body stay unchanged; replies become bubbles.
- **`native`:** `bubbleFill` for a dark theme, a light theme and the paper
  theme; `chatStyle` round-trips through backup YAML, and a bad value is an
  error.
- **`native_lv`:** a `LogBox` given bubble rows makes a fill of the right
  column range and colour, both normal and lean (painted). Selection outlines
  the bubble.
- **Hardware:** the M9 and the Wio L2, in a room, a C-Mail conversation and an
  entry with replies, across three themes and each text size. Add this to
  "Open hardware checks" in `docs/TESTING.md`.

## Docs

- `docs/USE.md`: the Chat style setting.
- `AGENTS.md`: one line in Progress.
