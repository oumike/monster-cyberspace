# Task 7 report: demo fake source

Changed `src/feed/fake_feed_source.cpp` (+ plan boxes ticked).
- Seeds Night Owls (user member) and Deep Divers (user not in), 3 threads and 3 members each; threads carry `guildSlug` (no `isGuildThread` field exists, none added). `findEntry` also searches guild threads so OpenEntry works.
- Handles OpenGuilds, OpenGuild (404 -> "Guild not found" banner + GuildUpdated code 404), GuildThreadsMore (single page), CloseGuild, OpenMembers, PostGuildThread (entryL_, EntryPosted), Join/Leave/Promote (membershipL_ via `limited()`, same GuildWriteDone texts, then refresh of mine/open guild + GuildsUpdated/GuildUpdated).
- Join -> apprentice; leave removes; promote -> member, previous member -> apprentice. Errors: 409 already in, 403 not in guild on leave/promote.
- Commands reach the fake via `command()` directly (no isFeedCommand in the demo path).
## Verification
- Demo build `PLATFORMIO_BUILD_DIR=.pio/demo PLATFORMIO_BUILD_FLAGS=-DRC_FAKE_API=1 pio run -e m9`: SUCCESS. `pio run -e m9`: SUCCESS. No unit tests (device-only code).
## Concerns
- The guild_new_thread notice is not simulated; member lists don't gain the user on join (counts do).
