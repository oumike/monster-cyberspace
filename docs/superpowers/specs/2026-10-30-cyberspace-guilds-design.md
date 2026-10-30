# Monster Cyberspace — Guilds

Date: 2026-10-30 · Status: approved design ("guilds"), builds on milestones 1–3

Milestone 1's spec (`2026-10-25-cyberspace-m9-circ-design.md`) still applies:
the constraints in §2, the architecture in §4 and the error handling in §9.
The Feed spec (`2026-10-26-cyberspace-m9-feed-design.md`) covers the Entry
and Composer screens this reuses. This document only covers what Guilds add.
Every write still comes from a key press, asks for confirmation, and is never
retried automatically.

## 1. Intent

Guilds are Cyberspace member groups, each with its own forum of threads. A
guild thread is an ordinary entry. The device can:

1. Browse every guild (most populated first), with the user's own guilds
   pinned at the top.
2. Open a guild's forum: its threads, most recently active first, paged.
   Threads open in the existing Entry screen (read, reply, bookmark, watch,
   report, delete).
3. Start a new thread in any guild (membership is not required).
4. Show a guild's info (founder, counts, bio, link) and its member list.
5. Join a guild, leave it, and make an apprenticeship "my guild" (promote).
6. Alert (banner and buzzer) to new threads in guilds the user belongs to.

The M9's far-right button (Map, raw `0x85`, `Action::OpenMap`) opens Guilds
from anywhere. Every board reaches it from Home → guilds.

Out of scope: founding a guild and editing its profile (web only), guild
chat, guild profile pictures (not drawn), and handing a guild over.

## 2. API (from api.cyberspace.online/docs, v0.9.0)

Guilds are identified by their **slug**. A user has at most one guild with the
role `founder` or `member` (the badge on their profile) and up to five more as
`apprentice`.

| Use | Call |
|---|---|
| Browse | `GET /v1/guilds?limit=20&cursor=<guildId>`: `data[]` of Guild, `cursor` |
| Guild | `GET /v1/guilds/:slug`: Guild plus `isMember`, `role` (or null); 404 unknown |
| Members | `GET /v1/guilds/:slug/members?limit=20&cursor=<membershipId>`: Membership[], oldest-joined first |
| Threads | `GET /v1/guilds/:slug/posts?limit=10&cursor=<postId>`: Entry[] (with `guildSlug`, `isGuildThread`) |
| My guilds | `GET /v1/users/me/guilds`: at most six, the member guild first; `cursor` always null |
| New thread | `POST /v1/guilds/:slug/posts {content, title?, topics?}`: `{postId, slug, title?}` (201) |
| Join | `POST /v1/guilds/:slug/join`: `{guildId, role}` (201); 409 already in it or five apprenticeships |
| Promote | `POST /v1/guilds/:slug/promote`: `{guildId, role}`; 404 not in it, 403 founder of current |
| Leave | `POST /v1/guilds/:slug/leave`: `{guildId}`; 403 founder, 404 not in it |

**Guild fields used:** `id`, `slug`, `name`, `icon`, `founderUsername`, `bio`,
`link`, `linkText`, `memberCount`, `apprenticeCount` (missing → 0),
`createdAt`; from Get Guild also `isMember` and `role`.

**My-guild fields used:** `slug`, `name`, `icon`, `role`.

**Membership fields used:** `membershipId`, `username`, `role`, `joinedAt`.

**Notification:** `guild_new_thread`, with `metadata.guildSlug`,
`metadata.guildName` and `metadata.postContent`; `targetId` is the thread's
post id.

Unknown fields are ignored and every optional field may be missing.

**Content limits:** thread body 32,768 on the server and 4,096 on the device;
title 100; topics at most 3, lowercase (the entry rules).

**Client-side limits** (the existing `RateLimiter`):

- New threads: 2/min, 24/day, shared with new entries.
- Membership (join, promote and leave together): 3/min, 15/day.

Reads stay below their limits: 30/min for guilds and members, 45/min for
threads.

## 3. Architecture

A small guild module for guild data; threads ride the Feed's existing
machinery, since a thread is an entry.

**Core** (`lib/core/src/guild/`, Arduino-free, with native tests):

- `guild.h`: `Guild` (the fields above plus `role`, "" when not in it),
  `MyGuild` (slug, name, icon, role) and `Member` (membershipId, username,
  role, joinedAt).
- `guild_parse`: parses the browse list, a guild, members, my guilds, and the
  join/promote/leave responses (the returned `role`).
- `guild_store` (`GuildStore`) holds:
  - my guilds (at most six);
  - the browse list and its cursor, without the guilds already in my guilds;
  - the open guild;
  - its members and their cursor.

  Caps: 200 browse guilds and 200 members (the newest page past the cap is
  dropped and the list ends); bios cut to 280 bytes. `clear()` on sign-out.
- `feed/compose` gains `guildThreadJson(title, body, topics)`, validated like
  an entry.
- `cs/api_parse`: `Mention` gains `guildName` (`metadata.guildName`).

**Device** (`src/`):

- `cs/cs_api` gains `listGuilds`, `getGuild`, `guildMembers`,
  `guildThreads`, `myGuilds`, `postGuildThread`, `joinGuild`,
  `promoteGuild` and `leaveGuild`. `mentions()` adds `guild_new_thread` to
  its type list.
- `FeedState` (behind `lockedFeed()`) gains a `GuildStore guilds`. The
  never-call-`feedSource()`-inside-`with()` rule applies.
- `FeedWorker` handles the guild commands. Guild threads load into the
  `FeedStore` side list (`setSideList`), so the Guild screen is a
  `FeedListScreen`.
- `messages.h` gains:
  - **commands:** `OpenGuilds`, `GuildsMore`, `OpenGuild` (slug),
    `GuildThreadsMore`, `OpenMembers`, `MembersMore`, `PostGuildThread`
    (slug + JSON), `JoinGuild`, `LeaveGuild`, `PromoteGuild` (slug);
  - **events:** `GuildsUpdated`, `GuildUpdated` (`code` 404 when the guild
    is gone), `MembersUpdated`, and `GuildWriteDone(ok, text)` for join,
    leave and promote (`FeedWriteDone` stays the Entry's reply result); a new
    thread reuses `EntryPosted(text = postId)`.
- After a successful join, leave or promote, the worker re-reads my guilds
  and the open guild. That is a read following the user's write, not a
  retry.
- `onNotice` turns `guild_new_thread` into
  `@user in <guildName>: <preview>`, stored and alerted like the Feed's
  notices; selecting it opens the thread.
- `feed/fake_feed_source` (demo build) serves two scripted guilds, their
  threads and members, and applies writes locally.

**Router:** new ids `GuildList`, `Guild` (`FeedListScreen{Kind::Guild}`, arg
= slug) and `Members` (arg = slug). `handleGlobal` stops swallowing
`OpenMap`: once signed in it closes overlays and goes to `GuildList`; pressed
again on `GuildList` with an overlay open it only closes the overlay; from
the Composer it is passed to the screen, which asks "Discard draft?" first.
The M9 keymap is unchanged. Home gains a `guilds` item and tile, after feed.

## 4. Memory

- Browse and members pages are `limit=20`; thread pages are `limit=10`, with
  the side list's caps and its body-too-large step-down (10, 5, 2).
- Opening GuildList frees the open guild and its members (Entry, Composer
  and Members keep them). My guilds and the browse list
  stay until sign-out.

## 5. Controls

**GuildList** (Map, or Home → guilds):

- Each guild takes two lines:
  - `🦉 Night Owls · 42+7`, then the user's role (`founder`, `member` or
    `apprentice`) when they belong to it; `+7` (apprentices) is left out when
    0;
  - the bio, dimmed (or `no bio`).
- A `── my guilds ──` divider heads the user's guilds and `── all guilds ──`
  the rest; with no guilds of their own, only the second shows.
- Up/Down select; Down at the end loads the next page, and wraps once every
  page is loaded, like the FeedList. Centre or Right opens the guild; Left or
  Back goes Home.
- Ctrl opens the palette: **Refresh** and **Text size**.
- The status bar reads `Guilds`.

**Guild** (the thread list):

- The status bar shows the icon and name. Rows are the FeedList's; Centre or
  Right opens a thread in Entry, and Back from Entry returns here.
- Ctrl opens the palette:
  - **New thread**;
  - **Info**;
  - **Members**;
  - **Join** (not in it), or **Leave** (member or apprentice; founders have
    none);
  - **Make my guild** (apprentices only);
  - **Refresh**;
  - **Text size**.
- Join, Leave and Make my guild ask for confirmation, showing the limit
  ("3 per minute, 15 per day"). Leave on the member guild adds "Your badge
  will be cleared."
- Left or Back returns to GuildList, as does Map.

**Info** (a scrolling read-only page, like Show link): name with icon,
`founded by @user`, `42 members · 7 apprentices`, `founded Mar 2026`, the
user's role if any, the bio (as Markdown text), and the link as
`linkText <url>` when present; Centre then shows the link full screen with
its QR code (the existing `LinkView`). Back closes it.

**Members:** `@user · role · 3mo` per row, paged like the browse list,
read-only. Back returns to the Guild.

**New thread:** the Composer with Title, Body and Topics only (no Public,
NSFW or CW, which the endpoint does not take). Its status bar reads
`new thread · <name>`. Post confirms with "2 per minute, 24 per day" and, on
`EntryPosted`, opens the new thread in Entry.

**Alerts:** a `guild_new_thread` banner `@user in Night Owls: <preview>`,
with the buzzer when it's on, like mentions.

**Touch boards** follow the existing paradigm: the `⋯` status-bar button
opens the palette, rows act on a tap, confirms are buttons, and the T-Deck
Pro shows its ▲ ● ▼ side strip on GuildList, Guild and Members.

## 6. Errors

M1 §9 applies. In addition:

- Join 409: the server's message as an error banner ("Already in this guild"
  or "Leave an apprenticeship first").
- Leave or promote 403: "Manage this on the web".
- Guild 404 (open, or a write): "Guild not found", and back to GuildList.
- A rate-limited write: "Slow down: try in Ns", nothing sent.
- Offline: the hint reads "offline: reading only"; writes show
  "Offline: not sent".

## 7. Testing

Native (`pio test -e native`):

- `guild_parse`: fixtures shaped like the docs, including a missing
  `apprenticeCount`, a null `role`, six my-guilds, empty and null cursors,
  and the join/promote responses.
- `guild_store`: my guilds removed from the browse list (also when they
  arrive after it), page de-duplication, caps, bio cut and `clear()`.
- `compose`: `guildThreadJson` validation (empty body, long title, topics).
- `api_parse`: `guild_new_thread` with `guildName`.

Firmware: `scripts/build-upload-monitor.sh -B` builds every env.

`docs/TESTING.md` gains a Guilds hardware checklist on the M9 (Map button,
browse and pinning, open, thread and reply, new thread, join, leave, make my
guild, info, members, alert). `docs/USE.md` gains a Guilds section and the
Map row in the Buttons table becomes "Guilds".
