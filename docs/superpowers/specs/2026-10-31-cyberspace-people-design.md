# People: profiles, follow, poke — design

Issue #2. Built on API docs v0.9.0 (`/docs`, `/types.d.ts`). Every write comes
from a key press and a confirm; nothing is retried.

## API

| Call | Use | Notes |
|---|---|---|
| `GET /v1/users/:username` | the profile | no "am I following" field |
| `GET /v1/users/me` | `blockedUsers` | usernames; read when a profile opens |
| `GET /v1/follows?type=following&limit=50&cursor` | who I follow | IDs only; up to 10 pages (500), loaded once per sign-in |
| `POST /v1/follows {followedId}` | follow | `{data:{followId}}`; 409 already following; 3/min, 15/day |
| `DELETE /v1/follows/:followId` | unfollow | 3/min, 15/day |
| `POST /v1/users/:username/poke` | poke | 201; 400 self, 403 blocked, 404 unknown; 1/h, 8/day, refused pokes don't count |
| `GET /v1/users/:username/posts` | Entries | paged like the Feed's side lists, with the `TOO_LARGE` retry |

There is no block or unblock call, and follower/following lists hold IDs
only, so the device shows no follower names. Blocking stays website-only.

## Design

- `lib/core/src/people/`: `Profile`, `parseProfile` (bio cut to 1 KB),
  `parseBlockedUsers`, `parseFollowPage`, `parseFollowId`, and `FollowIndex`
  (followed user ID → follow ID, capped at 500; `complete()` is false when
  the cap was hit). `ui/profile_layout` turns a profile into text.
- Client limiters `makeFollowLimiter` (follow and unfollow each) and
  `makePokeLimiter`; poke takes a slot only on success or 429.
- `src/app/people_worker`: `OpenProfile`, `Follow`, `Unfollow`, `Poke`,
  `OpenUserPosts`; events `ProfileUpdated` and `PeopleWriteDone`. State in
  `store/locked_people`.
- Screens: `Id::Profile` (`ProfileScreen`) and `Id::UserPosts` (a
  `FeedListScreen` of `Kind::User`). `screens/people_nav` remembers where a
  profile was opened (up to 8 places), so Back returns there, Entry's base
  list included.
- Menu: Follow/Unfollow, Message, Poke, Entries, Guild, Website, Refresh.
  Self: no Follow, Message or Poke. Blocked: no Follow, Message or Poke.
- Entry points: room person menu, room message menu, conversation menu,
  entry and reply menus, guild members.
- The demo build (`RC_FAKE_API`) has made-up profiles; `nobody` is 404, the
  user follows `rain`, `troll` is blocked.

## Not done (follow-ups)

- Poke and new-follower notifications.
- A Home tile or a "find user" box (Home is left alone to avoid clashing
  with the Unread view, issue #3).
