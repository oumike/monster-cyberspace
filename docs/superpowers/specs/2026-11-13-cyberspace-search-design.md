# Search — design

Issue #9, item 1, the last one open. Approved in chat on 2026-11-13. API
reference:
`https://api.cyberspace.online/docs.md`, section Search.

## Goals

- A **Search** item on Home opens a screen where you type a query and press
  Enter to run one search across people, entries and replies.
- Results show grouped: **People**, **Entries**, **Replies**, up to 8 each
  (the API's `type=all` preview). Each group with 8 hits ends with
  **More…**, which opens that type's own paged list.
- Picking a person opens their profile; an entry opens it; a reply opens its
  entry at that reply.

## Non-goals

- No search as you type, no saved or recent searches, no search on a timer.
- No `limit` tuning: paged lists use the API's default of 20.
- Not on boards without PSRAM (Cardputer ADV, X4): a `type=all` reply can
  hold 16 entry and reply bodies, too much beside TLS there.

## Global rules (from AGENTS.md)

- Every request comes from a key press: one search per Enter, one page per
  **More…** or end-of-list press. Nothing retried; a failure shows a banner.
- Search bills a site-wide monthly quota and is limited to 30 a minute, 60 an
  hour and 200 a day. The device adds no limit of its own (user decision): a
  429 from the server shows its message as a banner, and is not retried.
- Tests use host fixtures; never the live API.
- Portable logic in `lib/core`, hardware only through `src/hal/board.h`.
  `src/grid_ui/` is frozen.

## API

`GET /v1/search?q=<query>&type=all` → `{ data: { users: [...], posts: [...],
replies: [...] } }`, up to 8 each, no cursor.

`GET /v1/search?q=<query>&type=posts|replies|users&page=N` → `{ data: [...],
cursor: "N+1" | null }`. Each hit has `type` (`user`, `post`, `reply`).

Fields kept:
- user: `username`, `displayName`.
- post: `postId`, `authorUsername`, `title`, `content` (first line only),
  `repliesCount`, `createdAt`.
- reply: `replyId`, `postId`, `authorUsername`, `content` (first line only),
  `createdAt`.

The query is 1–512 characters after trimming; an empty one is refused on the
device before any request.

## Core (`lib/core/src/search/`)

- `SearchHit { Kind kind; std::string id, postId, user, title, line; int64_t
  createdMs; int replies; }`, `Kind` one of `User`, `Post`, `Reply`.
- `parseSearchAll(json, out users, posts, replies) -> bool`.
- `parseSearchPage(json, out hits, out cursor) -> bool` (cursor empty when
  null).
- `searchQueryOk(q) -> bool` (1–512 characters after trimming) and
  `searchPath(q, type, page) -> std::string` (URL-encodes `q`).
- Each hit keeps only the first non-empty line of `content`, as plain text,
  cut to 160 characters, so a page holds little.

## Firmware

- `CsApi::search(q, type, page, ...)` in `src/cs/cs_api.{h,cpp}`.
- `SearchState` in `src/store/locked_search` (query, the three groups, the
  open paged list and its cursor), cleared on leaving Search.
- Commands on `FeedWorker`: `CmdType::Search` (a = query), `SearchMore`
  (a = type, n = 0 first page / 1 next page), `CloseSearch`; events
  `EvtType::SearchUpdated` (ok, text = error).
- `RC_FEAT_SEARCH` in `feature_flags.h`: default `RC_FEAT_PEOPLE &&
  RC_FEAT_ENTRIES && !RC_LOW_MEM`.

## Screens (`src/screens/search.cpp`)

- `SearchScreen` (`router::Id::Search`): the query line at the top (an
  `InputField`, as C-Mail's "New conversation…"), open for typing on entry
  with the last query kept. Enter runs the search; Down from the line moves
  into the results. A group's header row (`People`, `Entries`, `Replies`)
  is dim and skipped by the cursor; empty groups are left out. Status line
  "searching…", "no results", "offline", or the error. Back leaves for Home.
  Touch: tapping the line types; a tap on a hit opens it.
- `SearchListScreen` (`router::Id::SearchList`, arg the type): one type's
  paged list, as the profile's Replies list: Down on the last row asks for
  the next page while `cursor` is set; Back returns to Search with its
  results still there.
- Rows: a person `@user  Display Name`; an entry `title or first line` with
  `@author · age` right; a reply `first line` with `@author · age` right.
- Opening: a person → `Id::Profile`; an entry → `Id::Entry` with its id; a
  reply → `Id::Entry` with `postId + "\n\n" + replyId` (as the profile's
  Replies list), each through `router::goFrom` so Back comes back here.

## Home

**Search** goes on Home just above Settings (moved there after review; first placed after Unread), on every board
with `RC_FEAT_SEARCH`.

## Tests

- `test/test_search/`: `parseSearchAll` and `parseSearchPage` from fixtures
  (all three kinds, a null cursor, a missing group, bad JSON), the content cut
  to its first line, `searchQueryOk` (empty, spaces only, 512 and 513
  characters) and `searchPath` encoding (`&`, space, `#`, UTF-8).
- `test/test_home_items/`: Search just before Settings only with the flag.

## Docs

`docs/USE.md` (Search), `docs/TESTING.md` (a Search hardware check),
`AGENTS.md` (the spec link and progress).
