# Slash command picker and Programs gallery — design

Issue #9, items 6 (slash command picker and command replies) and 8 (Programs
gallery). Approved in chat on 2026-11-12. API reference:
`https://api.cyberspace.online/docs.md` (v0.9.2), sections Commands and
Programs.

Item 7 (choose your guild badge) needs no work: a guild's menu already has
**Make my guild** for apprentices (confirm first, `CmdType::PromoteGuild`,
`POST /v1/guilds/:slug/promote`, which the docs call "Change Your Guild
Badge"), and a profile's Guilds list shows each role, `member` or `founder`
marking the badge.

## Goals

- Typing `/` into an empty room or C-Mail input opens a list of the slash
  commands, each with a one-line hint, filtered as you type. Picking one fills
  in the input; it never sends.
- The commands button in rooms and C-Mail opens the room's menu instead.
- `/help`'s reply shows in C-Mail as it already does in rooms.
- A read-only **Programs** gallery from Home: browse published programs and
  read their source and earlier releases.

## Non-goals

- No running, publishing, recalling or installing programs, and no `?mine=1`
  list.
- No runtime filter on the gallery: every kind is listed, labelled.
- No picker on the X4 (frozen grid UI, and it cannot type) or in other text
  fields (Composer, entry replies, notes).
- No change to how the server's command results are drawn in the log.

## Global rules (from AGENTS.md)

- Every write comes from a key press; no automatic retries of a write.
  Picking a command writes nothing. Programs only reads.
- Tests use host fixtures; never the live API.
- Portable logic in `lib/core`, hardware only through `src/hal/board.h`.
- `src/grid_ui/` is frozen.

## Item 6: the slash command picker

### Command table (`lib/core/src/ui/widgets/palette`)

`PaletteEntry` gains `hint` (a short line, ASCII). The table, in this order,
with the cIRC-only ones marked:

| Command | Hint |
|---------|------|
| `/me` | an action: /me waves |
| `/poke` `/hug` `/hi5` `/slap` | at @user, or alone |
| `/dice` | roll: 2d6+3, 4d6kh3, adv |
| `/coin` | heads or tails |
| `/8ball` | ask a question |
| `/fortune` | a fortune cookie |
| `/gif` | a GIF by https link |
| `/song` | YouTube link \| artist \| title (supporters) |
| `/art` (cIRC) | ASCII art on the lines after |
| `/spoiler` `/blink` `/l33t` `/comic` `/cursive` `/times` `/rainbow` `/flip` `/quiet` `/slow` `/glitch` `/wave` | text style; chain with + |
| `/localtime` (cIRC) | your time, from the clock |
| `/mute` `/unmute` (cIRC) | mute @user here / undo |
| `/muted` `/unmuteall` (cIRC) | list mutes / clear them |
| `/help` | the server's command list |

`PaletteKind::Cmail` leaves out the cIRC-only rows (today it keeps
`/localtime`; the docs say cIRC only).

New, pure and tested:

- `paletteMatches(kind, typed)`: the entries whose command starts with
  `typed` (which begins with `/`), case-insensitive, in table order. `typed`
  of `/` alone matches all.
- `paletteInsert(entry, clock)`: the text to put in the input. Normally
  `entry.insert` (`"/cmd "`). For `/localtime`, `"/localtime HH:MM "` from a
  `struct tm` when the clock is set, else `"/localtime "`.

### Behaviour (`RoomScreen`)

- **Opens** when a `/` is typed into an empty input line (rooms and C-Mail,
  `board::canType()`), and stays open while the input is a single word
  starting with `/` that matches at least one command.
- **Typing** goes to the input as usual and refilters; the highlight returns
  to the first match. A space, a non-matching word or an emptied input closes
  the list.
- **Up/Down** (and the wheel, trackball, side keys) move the highlight while
  it is open, instead of scrolling the log.
- **Select / Enter**, the commands button (`Action::Palette`, e.g. Tab) or a
  tap on a row replaces the input with
  `paletteInsert` of the highlighted command and closes the list. It never
  sends; the next Enter sends.
- **Back / Escape** closes the list and keeps the text.
- It is non-modal: `view_` stays `View::None`, the input keeps focus, and
  the list is drawn with the existing `MenuOverlay` above the input (title
  `commands`, rows `"/cmd  hint"`), cut to fit the space above the input.
- The touch keyboard's `/` key opens it like any other.

### The commands button

- In a room or C-Mail (`board::canType()`), `Action::Palette` opens the
  room's menu (`openRoomMenu`) instead of the command list. The M9's Ctrl
  therefore opens the room menu while typing, and Ctrl again the emoji tray
  (`ui_task`'s existing "again" rule).
- The symbol tray's `commands…` entry (`SymbolTable::Kind::Commands`) types
  a `/` into a screen that takes text, opening the picker, instead of sending
  `Action::Palette`.
- Every other screen's Palette is unchanged.

### Replies

- `CsApi::cmailSend` reads `data.reply` like `send` does (`parseSendResult`);
  `NetWorker`'s `CmdType::Send` shows it as a banner for C-Mail too.
- Error banners (`400 VALIDATION_ERROR` text, `403` for `/song`) already come
  through `SendDone` with the server's message; unchanged.

## Item 8: the Programs gallery

### Feature flag

`RC_FEAT_PROGRAMS` in `lib/core/src/feature_flags.h`, default
`RC_FEAT_FEED && !RC_LOW_MEM`; 0 on the Cardputer ADV and X4.

### Model and parsing (`lib/core/src/programs/`)

```cpp
struct ProgramInfo {
  std::string id, name, owner, description, runtime;  // runtime "" reads as "web"
  int release = 0;
  int64_t publishedMs = 0;
};
struct ProgramSource {
  ProgramInfo info;
  bool binary = false;      // encoding base64 (wasm)
  size_t bytes = 0;         // decoded size for binary, else source length
  std::string text;         // the first kSourceShowMax bytes of utf8 source
  bool cut = false;         // text was cut at kSourceShowMax
};
constexpr size_t kSourceShowMax = 32 * 1024;
bool parseProgramPage(std::string_view body, std::vector<ProgramInfo>& out, std::string& cursor);
bool parseProgramSource(std::string_view body, ProgramSource& out);
```

- `runtime` missing reads as `web`.
- The cut lands on a UTF-8 boundary.
- `bytes` for base64 is computed from the encoded length and padding, without
  decoding.

### API (`src/cs/cs_api`)

- `programs(before, out, cursor)`: `GET /v1/programs?limit=30[&before=]`.
- `programSource(id, release, out)`: `GET /v1/programs/:id/source[?release=N]`
  (release 0 = current).
- A body over the client's cap (`HttpResult::overflow`) returns an error
  whose message is `Too large to show here`.

### Worker and state

On `FeedWorker` (`isFeedCommand`): `CmdType::OpenPrograms` (first page),
`MorePrograms` (next page), `OpenProgram` (`a` = id, `n` = release or 0).
`MorePrograms` follows a non-null cursor past empty pages, at most 5 requests
for one key press. State in `lockedPrograms()` (`src/store/locked_programs`): the list, the cursor,
`loading`, and the open `ProgramSource`. Events `ProgramsLoaded`,
`ProgramLoaded`, with errors as banners. Nothing is polled or retried.

### Screens (`src/screens/programs.cpp`)

- **Programs list** (`router::Id::Programs`, from Home's **Programs**
  item/tile): a `ListBox`, each row `name  @owner` with the runtime on the
  right, newest first, and `Older…` last while there is a cursor. Select
  opens the program; Back goes Home. Menu: Refresh, Text size.
- **Program** (`router::Id::Program`, arg `id` or `id@release`): a
  `TextBox` with a header (name, `@owner`, runtime, `release N`, date,
  description), a rule, then the source verbatim (no wrap, Left/Right scroll
  as the room reader) or `wasm binary, N bytes: can't be shown here`, and
  `… cut off at 32 KB` when cut. Menu: **Releases** (N down to 1; picking one
  opens that release), **Author's profile** (`router::Id::Profile`),
  **Refresh**.

### Home

`HomeItem::Programs` after Guilds, shown when `RC_FEAT_PROGRAMS`; label
`Programs`, tile glyph from the existing set.

## Testing

- Native: `paletteMatches` (prefix, case, `/` alone, C-Mail leaves cIRC-only
  rows out), `paletteInsert` (`/localtime` with and without a clock),
  `parseProgramPage` (rows, missing runtime, null and string cursor),
  `parseProgramSource` (utf8, base64 size, the 32 KB cut on a UTF-8
  boundary), `homeItems` with programs on and off.
- `native_lv` stays green; every firmware env builds.
- Hardware (docs/TESTING.md, open): the picker on a keyboard board and a
  touch board, the commands button opening the room menu, `/help` in C-Mail,
  the gallery list, Older…, a source, a wasm program, Releases.
