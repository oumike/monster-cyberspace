# Platy Cyberspace — milestone 3: Feed on the ThinkNode M9

Date: 2026-10-26 · Status: approved design ("feed"), builds on milestones 1 and 2

Milestone 1's spec (`2026-10-25-cyberspace-m9-circ-design.md`) still applies:
the constraints in §2, the architecture in §4, and the error handling in §9.
This document only covers what the Feed adds. Every write still comes from a
key press, and nothing is retried automatically.

## 1. Intent

The Feed is Cyberspace's stream of entries (posts) and their replies. The
device can:

1. Show the feed, newest first, paged, and refresh it automatically while it
   is on screen.
2. Open an entry: its body as plain text, then its replies, oldest first,
   paged.
3. Reply to the entry or to a reply.
4. Write a new entry: title, body, topics, public, NSFW and content warning.
5. Bookmark and unbookmark entries and replies; watch and unwatch threads.
6. List watched threads and bookmarks.
7. Delete your own entries and replies; report other people's.
8. Alert you (banner and buzzer) to replies and mentions, even with the
   screen off.

Out of scope: editing (supporters only, 5-minute window), topic browsing,
profiles, follows, images (shown as text), and audio attachments (shown as
text, never posted).

## 2. API (from api.cyberspace.online/docs, v0.9.0)

Timestamps here are ISO 8601 strings, not epoch ms as in cIRC and C-Mail.

| Use | Call |
|---|---|
| Feed | `GET /v1/posts?limit=10&cursor=<postId>`: `data[]` of Entry, `cursor` (null at the end) |
| Entry | `GET /v1/posts/:id`: Entry |
| Replies | `GET /v1/posts/:id/replies?limit=20&cursor=<replyId>`: Reply[], oldest first |
| Reply by id | `GET /v1/replies/:id`: Reply (gives `postId`) |
| New entry | `POST /v1/posts {content, title?, topics?, isPublic?, isNSFW?, contentWarning?}`: `{postId, slug, title?}` (201) |
| Reply | `POST /v1/replies {postId, content, parentReplyId?}`: `{replyId}` (201) |
| Delete | `DELETE /v1/posts/:id`, `DELETE /v1/replies/:id` (author only) |
| Report | `POST /v1/posts/:id/flag`, `POST /v1/replies/:id/flag` `{reason?}`: 201, or 200 with `alreadyFlagged` |
| Watch | `GET` / `POST` / `DELETE /v1/posts/:id/watch`: `{watching}` |
| Watched | `GET /v1/watches?limit=20&cursor=`: `[{id, postId, createdAt}]` |
| Bookmarks | `GET /v1/bookmarks?limit=50&cursor=`: `[{bookmarkId, postId?, replyId?, type}]` |
| Bookmark | `POST /v1/bookmarks {postId, type:"post"}` or `{replyId, type:"reply"}`: `{bookmarkId}` |
| Unbookmark | `DELETE /v1/bookmarks/:bookmarkId` |

**Entry fields used:**

- `postId`, `authorUsername`, `content`, `title?`, `slug?`, `topics?`
- `repliesCount`, `isPublic?`, `isNSFW?`, `contentWarning?`
- `createdAt`, `editedAt?`, `deleted`, `attachments?`, `isGuildThread?`, `guildSlug?`

**Reply fields used:**

- `replyId`, `postId`, `parentReplyId?`, `parentReplyAuthor?`
- `authorUsername`, `content`, `createdAt`, `editedAt?`, `deleted`, `attachments?`

Unknown fields are ignored and every optional field may be missing.

**Content limits:**

- Body: 32,768 characters on the server, 4,096 on the device.
- Title: 100. Topics: at most 3, lowercase. Content warning: 60. Report reason: 500.

**Client-side limits** (the existing `RateLimiter`):

- Entries: 2/min, 24/day.
- Replies: 3/min, 48/day.
- Bookmarks: 5/min, 75/day.
- Watch: 10/min, 100/day.
- Flags: 5/min, 20/h, 50/day, shared with cIRC flags.

The reads stay far below their limits: 45/min for lists and replies, 30/min
for bookmarks, watches and notifications.

## 3. Architecture

This follows the C-Mail pattern: a separate module behind the same queues.

**Core** (`lib/core/src/feed/`, Arduino-free, with native tests):

- `entry.h`: `Entry` and `Reply` structs, with only the fields above.
- `feed_parse`: parses the list, entry, replies, reply, create, bookmarks,
  watches and watch-status responses.
- `markdown_text`: converts Markdown to plain text lines for the cell grid.
  - Headings lose their `#` markers.
  - Emphasis markers are dropped.
  - `[text](url)` becomes `text <url>`.
  - `![alt](url)` becomes `[img: alt]`.
  - Code spans and fences are kept verbatim.
  - List bullets become `•`, and blockquotes become `│ `.
  - UTF-8 is kept intact.
- `compose`: builds and validates the entry and reply request JSON. A
  validation error returns a message, and nothing is sent.
- `feed_store`: holds:
  - the feed list and its cursor;
  - the open entry, its replies and the replies cursor;
  - the bookmark map (post or reply id to `bookmarkId`);
  - the watched and bookmark lists, plus the open entry's watch flag.

  It applies the memory caps (§4) and de-duplicates by id when pages
  overlap.

**Device** (`src/`):

- `cs/cs_api` gains the Feed calls.
- `feed/feed_source.h` is the interface the screens use.
  - `net_feed_source` runs its calls in the net worker task and posts events.
  - `fake_feed_source` (in `m9_fake`) serves a scripted feed and applies
    writes locally.
- `store/locked_feed` guards the store with a mutex, like `locked_chat`.
  - Never call `feedSource()` commands inside `lockedFeed().with(...)`. The
    fake source would deadlock, as with chat.
- `screens/`: `FeedList`, `Entry`, `Composer`, `Watched`, `Bookmarks`.
- `messages.h` gains:
  - **commands:** `OpenFeed`, `CloseFeed`, `FeedMore`, `FeedRefresh`,
    `OpenEntry`, `MoreReplies`, `PostEntry`, `PostReply`, `DeleteEntry`,
    `DeleteReply`, `FlagEntry`, `FlagReply`, `Bookmark`, `Unbookmark`,
    `Watch`, `Unwatch`, `OpenWatched`, `OpenBookmarks`;
  - **events:** `FeedUpdated`, `EntryUpdated`, `RepliesLoaded`,
    `FeedWriteDone(ok, text)`, `EntryPosted(text = postId)`, `FeedAlert`.

  `PostEntry` and `PostReply` carry the JSON body that `compose` built.

## 4. Memory

- The feed list asks for `limit=10` and keeps the first 280 characters of
  each body as a preview.
- The open entry keeps up to 16 KB of body. Past that it ends with
  "… read the rest on the web".
- Replies come in pages of 20, with at most 100 held (the oldest are dropped
  first). Each reply is capped at 4 KB.
- Strings longer than 4 KB land in PSRAM (`SPIRAM_MALLOC_ALWAYSINTERNAL`),
  and so does the 256 KB HTTP body buffer.
- If a response exceeds that buffer ("body too large"), the device asks again
  with a smaller `limit`: 10, then 5, then 2. This is the only automatic
  repeat, and it is a read.
- Leaving the Feed screens frees the open entry and its replies. The feed list
  and the bookmark map stay until sign-out.

## 5. Controls

**Fn under Back** opens the **FeedList**. Pressed again inside the Feed, it
returns to the FeedList.

**FeedList:**

- Each entry takes two lines:
  - `nick · 3h · ↩5`, plus `★` if it is bookmarked;
  - the title, or else the first line of the body, dimmed. An entry with a
    content warning or the NSFW flag shows `CW: <warning>` (or `CW: NSFW`)
    instead.
- Up/Down select; the selected entry is outlined. Down at the end loads the
  next page; once every page is loaded it wraps to the top, and Up on the
  first entry wraps to the last. Centre or Right opens the entry, and Left or Back goes Home.
- Ctrl opens the palette: **New entry**, **Refresh**, **Watched threads**,
  **Bookmarks**, and **Text size**.
- The status bar reads `Feed`, with `↻ 2m` while auto-refresh is on.

**Entry:**

- At the top is a header: the author, time, topics and the `public`/`NSFW`
  marks. Below it come the body, a `── N replies ──` divider, and then the
  replies in the room log style. A reply to a reply has a dim `↳ @parent`
  line above it.
- Up/Down move a selection over the blocks (body or reply), outlined as in
  rooms. Down past the last loaded reply loads more, and past the end it
  returns to typing.
- With a content warning or NSFW flag, the body shows `CW: …` until Centre
  reveals it.
- The keyboard always types into the reply line, and Centre or Enter sends.
  - If a reply is selected, the reply answers it (`parentReplyId`), and
    `↳ @name` shows above the input line.
  - Otherwise it answers the entry.
- Centre on an empty line opens the entry menu:
  - Bookmark/Unbookmark, Watch/Unwatch;
  - Show link (text plus a QR code of
    `https://cyberspace.online/<author>/<slug>`);
  - Report (with confirmation and an optional reason);
  - Delete (your own entry only, with confirmation);
  - Text size.
- Centre on a selected reply opens the reply menu: Reply, Bookmark/Unbookmark,
  Show link, Report, and Delete (your own).

**Composer** (full screen, from the palette's New entry):

- The fields are Title, Body, Topics (comma-separated), Public [ ], NSFW [ ],
  and CW.
- Up/Down move between fields. On the M9, Enter and Centre are the same key
  (Select).
  - In Body, Select adds a new line, Up/Down move by line, and a counter shows
    `n/4096`. Up on the first line moves to Title, and Down on the last line
    moves to Topics.
  - On a checkbox, Select toggles it.
- Ctrl opens **Post** and **Discard**. Post first asks for confirmation and
  shows the limit ("2 per minute, 24 per day").
- Back with any text in the fields asks "Discard draft?".
- The draft lives in RAM until you post or discard it. A failed post keeps
  it. A successful post opens the new entry.

**Watched / Bookmarks:** plain lists in the FeedList row style. Centre opens
the entry; for a bookmarked reply, it opens the parent entry with that reply
selected.

## 6. Data flow

- **Opening the Feed:**
  - The device fetches the first page.
  - If the bookmark map hasn't been loaded since sign-in, it loads it: up to
    4 pages of 50, so 200 bookmarks at most.
- **Auto-refresh:** while the FeedList is the current screen and the screen
  is on, the device re-fetches the first page every 2 minutes. New entries are
  merged at the top, and the selection stays on the same entry. Turning the
  screen off or leaving the FeedList stops it. Refresh in the palette
  re-fetches the first page and reloads the bookmark map.
- **Opening an entry:** `GET /v1/posts/:id`, then the first page of replies,
  then the watch status. That is three reads.
- **After a reply is sent:** the device re-fetches the replies from the
  current cursor start, so the new reply appears with its server id.
- **Watched and Bookmarks lists:** the lists return only ids, so the device
  fetches each listed entry with `GET /v1/posts/:id`, 10 per page. For a
  reply bookmark, `GET /v1/replies/:id` gives the `postId` first.
- **Bookmark and unbookmark** update the map locally once they succeed.
- **Watcher** (the existing 60-second loop, which keeps running with the
  screen off): the notification poll's `type` filter widens to
  `chat_mention,reply,thread_reply,post_mention,reply_mention,post_cooldown`.
  This is the same single request as before.
  - Each new Feed notification raises a `FeedAlert`: a banner ("@actor
    replied: preview") and an 1800 Hz beep if the buzzer is on.
    `post_cooldown` raises a banner only: "Entry held for review (saved as a
    note)".
  - Home's dashboard gains "feed N unread". Selecting it opens the entry from
    the newest unread notification's `targetId` and marks that notification
    read with `PATCH /v1/notifications/:id`.
  - A notification about the entry that is already open only marks the
    notification read and refreshes the replies.

## 7. Errors

The M1 §9 rules apply. In addition:

- **401:** as in M1, `CsApi::call` refreshes the token and repeats that one
  request once. The server rejected the first attempt, so this never
  duplicates a write. Nothing else is retried.
- **403:** shows "Not allowed" for delete or report, and the server message
  otherwise. **404 on an entry:** "Entry gone", and the device goes back.
- **429:** "Slow down", with the limit that was hit.
- **400:** shows the server's `message`.
- **Blocked locally:** the client-side limiter blocks a send before the
  request, with the same banner.
- `deleted: true` shows as a dim `[deleted]`, for entries and replies alike.
- When a reply or post fails, its text stays in the input line or the draft,
  so you can send it again by hand.

## 8. Testing

**Host** (`test/test_feed/`; tests written first, fixtures written by hand
from the documented shapes):

- Parsing every response in §2, including missing optional fields, `deleted`,
  and a null cursor.
- `markdown_text`: every rule in §3, plus UTF-8 and empty input.
- The caps in §4.
- `compose`: each limit, lowercase topics, an empty CW or title left out,
  `parentReplyId` included only when set.
- `feed_store`: page cursors, de-duplication, the reply cap, the bookmark map,
  and keeping the selection on merge.
- The new `RateLimiter` rules.
- Layout of the two-line rows, the entry view and the `↳ @parent` marker.

**`m9_fake`:** a scripted feed of about 15 entries. It includes:

- one with a CW, one NSFW, and one long Markdown entry;
- threaded replies.

Posts, replies, bookmarks and watches are applied locally. A fake `reply`
alert arrives every 90 s.

**Live (human):** in `docs/TESTING.md`:

- Read the feed.
- Post a test entry, then delete it.
- Reply, and reply to a reply.
- Bookmark and watch.
- Get a reply alert from a second account.

## 9. Documentation

- `docs/USE.md` gains a Feed section.
- `docs/TESTING.md` gains Feed hardware checks and live checklist items.
- The progress section in `AGENTS.md` gets updated.
