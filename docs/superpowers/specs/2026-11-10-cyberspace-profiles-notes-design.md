# Fuller profiles and Notes — design

Issue #9, items 5 (Fuller profiles) and 2 (Notes). Approved in chat on
2026-11-10. API reference: `https://api.cyberspace.online/docs.md` (v0.9.1)
and its `types.d.ts`.

## Goals

- A profile shows a user's **Replies** and **Guilds**, and your own profile
  can be **edited** (display name, bio, website URL and name, location name).
- **Notes**: your private notes on cyberspace.online, listed, read, written,
  edited, deleted, and their revisions browsed, from a Home tile.

## Non-goals

- **No Followers list.** `GET /v1/follows` returns only user ids
  (`followerId`, `followedId`) and the API cannot look a user up by id; the
  follower count already on the profile stays the only follower figure.
- No pinned entry, latitude/longitude or website button in Edit profile.
- No note topics: notes are written and shown without them, and an edit
  sends `content` only, so the server keeps any topics a note already has.
- No restoring an old revision: History is read-only.
- Notes are not on boards without PSRAM (Cardputer ADV, X4).

## Global rules (from AGENTS.md)

- Every write comes from a key press; no automatic retries of a write.
- Tests use host fixtures; never the live API.
- Portable logic in `lib/core`, hardware only through `src/hal/board.h`.
- `src/grid_ui/` is frozen: these features are not added to the X4's grid UI.

## Part A: Fuller profiles

### Profile links

`ProfileScreen::buildLinks` (`src/screens/profile.cpp`) gains, after
Entries: **Replies** (any loaded profile) and **Guilds** (any loaded
profile), and, on your own profile only, **Edit** after Bookmarks. Order:
Entries, Replies, Bookmarks (self), Edit (self), Guilds, Guild (badge, when
set), Website (when set), Refresh. All need `RC_FEAT_PEOPLE`; Replies also
needs `RC_FEAT_ENTRIES` (it opens the Entry screen).

### Replies

- `GET /v1/users/:username/replies?limit=20&cursor=<replyId>` (45/min),
  newest first, shape `ApiList<Reply>`.
- A new router place `Id::UserReplies` (arg = username): a `ListBox` of
  rows `"<date>  <first line of content>"`, content truncated to the row;
  deleted replies are skipped. Reaching the end loads the next page
  (`cursor`), as the other paged lists do. Empty: "No replies yet.".
- Select opens `Id::Entry` with arg `"<postId>\n\n<replyId>"`, the existing
  form that opens an entry scrolled to a reply.
- Parsed into `rc::UserReply { replyId, postId, content, createdMs }`.

### Guilds

- `GET /v1/users/:username/guilds` (30/min): at most six, the guild they're
  a member of first, then apprenticeships; `cursor` always null.
- A new place `Id::UserGuilds` (arg = username): rows
  `"<icon> <name>"` with the role (`founder`, `member`, `apprentice`) as the
  row's right-hand value. Empty: "Not in a guild.". Select opens `Id::Guild`
  with the slug (needs `RC_FEAT_GUILDS`; without Guilds the rows don't open).
- Parsed into `rc::UserGuild { slug, name, icon, role }`.

### Edit profile

- A new place `Id::ProfileEdit`, reached only from your own profile's Edit.
  It starts from the loaded `Profile` (`displayName`, `bio`, `websiteUrl`,
  `websiteName`, `locationName`).
- Rows: Display name, Bio, Website URL, Website name, Location, each with its
  current value (dim "not set" when empty); then **Save** and, in the menu,
  **Discard**. Select on a field opens an `InputField` (Bio: the multi-line
  text box the Composer's Body uses), filled with the value; Enter keeps the
  edit, Back drops it.
- Limits checked on the device before anything is sent, each shown as a
  banner naming the field: display name, website name and location 64
  characters; bio 640; website URL 2048 and must start with `http://` or
  `https://` (or be empty).
- **Save** sends one `PATCH /v1/users/me` with only the fields that differ
  from the loaded profile; a field emptied sends `null`. No change: Save just
  goes back. Success: the profile is fetched again and the screen returns to
  it. Failure: a banner with the server's message (429: "Profile can be
  saved 2 times a minute, 15 a day"); the edits stay so the user can try
  again by pressing Save. Back with unsaved changes asks "Discard changes?".
- Body builder in `lib/core/src/people/`:
  `std::string profilePatchJson(const ProfileFields& before, const ProfileFields& after)`
  returning `""` when nothing changed, and
  `const char* profileFieldError(const ProfileFields&)` returning nullptr or
  the banner text.

### Network side

`PeopleWorker` gains commands `OpenUserReplies` (a = username, n = 1 append),
`OpenUserGuilds` (a = username) and `SaveProfile` (a = JSON); `CsApi` gains
`userReplies`, `userGuilds` and `updateProfile`. Results go into
`PeopleState` and are announced with the existing `ProfileUpdated` /
`PeopleWriteDone` events (or new ones where a list screen needs its own).

## Part B: Notes

### Flag and Home

- `RC_FEAT_NOTES`, default `RC_FEAT_FEED`; set to 0 in the `cardputer_adv`
  and `x4` envs. `rc::Features` gains `notes`.
- `HomeItem::Notes`, after Following (before Guilds), in tiles and rows;
  `homeSpanFirst` keeps its rule (Unread spans only when the rest fill rows
  of two).

### List

- A new place `Id::Notes`: `GET /v1/notes?limit=20&cursor=` (30/min),
  `ApiList<Note>`. A pinned **+ New note** row first (as the Feed's
  "+ New entry"), then one row per note: its first non-empty line (or
  "(empty)") with the updated date as the right-hand value. Newest first as
  the server returns them; end of list loads the next page. Empty:
  "No notes yet.".
- Memory: a page can carry 20 notes of up to 32,768 characters. The parser
  keeps only `noteId`, the first line (capped at 120 bytes), `updatedMs` and
  `revisionNumber` per row; bodies are not kept.

### Reading

- A new place `Id::Note` (arg = noteId, or `"<noteId>\n<revision>"` for an old
  revision): `GET /v1/notes/:id` (or `?revision=N`), shown read-only in a
  scrolling `TextBox`, with the header "Note · rev N · <date>" (an old
  revision: "rev N of M").
- Menu on the latest revision: **Edit**, **History**, **Delete**. On an old
  revision: no menu (read-only).
- Delete asks "Delete this note and all its revisions?" then
  `DELETE /v1/notes/:id`; success returns to the list, refreshed.

### Writing

- The Composer (`src/screens/composer.cpp`) gets a Note mode: only the Body
  field, title "New note" or "Edit note", Send labelled **Save**. Arg
  selects it (e.g. `"note"` new, `"note\n<noteId>"` edit, body prefilled).
- Before sending: an empty body is refused ("Note is empty"), over 32,768
  characters is refused ("Note is too long").
- New: `POST /v1/notes {"content": …}`. Edit: `PATCH /v1/notes/:id
  {"content": …}`, which adds a revision. Success opens the note. Failure:
  banner with the server's message (429 on create: "Notes can be created 3
  times a minute, 30 a day"); the text stays, no retry. Discard asks as the
  Composer already does.

### History

- A new place `Id::NoteHistory` (arg = noteId):
  `GET /v1/notes/:id/revisions?limit=20&cursor=`, newest first; rows
  `"rev N"` with the date; end of list loads more. Select opens
  `Id::Note` with `"<noteId>\n<N>"`.

### Network side

- `lib/core/src/notes/`: `NoteRow { id, firstLine, updatedMs, revision }`,
  `Note { id, content, revision, updatedMs }`, parsers
  `parseNoteList`, `parseNote`, and `noteBodyJson(content)`,
  `noteError(content)` (nullptr or banner text).
- `CsApi`: `notes`, `note(id, revision)`, `noteRevisions`, `createNote`,
  `updateNote`, `deleteNote`.
- Commands on the existing `FeedWorker` (no new task): `OpenNotes`
  (n = 1 append), `OpenNote` (a = id, n = revision or 0), `OpenNoteHistory`
  (a = id, n = 1 append), `SaveNote` (a = id or "", b = JSON),
  `DeleteNote` (a = id). State in a `NotesState` behind its own lock, cleared
  when Notes is left for Home.

## Testing

Host tests (`pio test -e native`), fixtures only:

- Parsers: user replies (deleted skipped, cursor), user guilds (roles,
  order), note list (first-line cap, bodies dropped), note, revisions.
- `profilePatchJson`: only changed fields, `null` for a cleared one, `""`
  for no change; `profileFieldError` at each limit and the URL scheme.
- `noteBodyJson` escaping; `noteError` for empty and 32,769 characters.
- `homeItems` with and without `notes`, and `homeSpanFirst` with Notes added.

Builds: `m9` and `wio_l2` (the boards under test), plus `cardputer_adv` to
prove Notes is compiled out. A hardware check goes into `docs/TESTING.md`;
`docs/USE.md` documents both features.
