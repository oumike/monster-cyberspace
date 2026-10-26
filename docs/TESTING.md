# Testing

## Host unit tests

```sh
pio test -e native
```

These cover the portable logic in `lib/core`:

- response parsing, from fixtures written from the API docs
- the SSE parser and the put/patch merge
- the chat store, wrapping and the room layout
- nick colours, the rate limiter and token timing
- the keymap, art decoding, QR codes and web helpers

They never touch the network.

## Fake-API firmware (`m9_fake`)

```sh
scripts/build-upload-monitor.sh --m9 --fake
```

The network task is replaced by a scripted source, so no account or Wi-Fi is
needed. Wi-Fi, the setup AP and the web server do not run in this build; flash
`m9` for the web config checks.

The fake source provides:

- the rooms `general`, `linux` and `music`, with fixture history and a new
  message every 4 s;
- 12 people, 3 of them idle;
- sends that echo back after 300 ms;
- a `400` for `/dice x`;
- the real local send limit, so the 16th fast send shows the rate-limit
  banner;
- C-Mail with rain (2 unread), moth, and kiwi (empty). A send echoes, then
  the partner types for 2 s and replies. rain writes every 45 s. Starting a
  conversation with `nobody` fails with "User not found".
- a Feed of 15 entries in two pages: your own (with a reply alert on it
  every 90 s), a bookmarked one with 25 threaded replies, a `spiders` CW, an
  NSFW one, a watched one and a long Markdown one. Writes apply locally under
  the real limits.

Check every screen and key:

- both text sizes, and the drawer in each
- the palette and the room, message and person menus
- spoilers and art
- a QR code on a link
- the error banners
- screen off, the screensaver and wake

## Open hardware checks

These were built without flashing and still need a run on the device.

**Themes (`m9_fake`):**

- [ ] Settings → Theme cycles Default → Black & white → White & black → Default; each redraws at once and survives a restart
- [ ] Web config Display → Theme changes the device at once; the select shows the saved theme on reload
- [ ] In both mono themes: Home, room, drawer, menus, palette, Feed list, Entry, Composer, login and Settings are readable; nicks use the text colour
- [ ] Link view QR stays black on white

**Room screen (`m9_fake`):**

- [x] Typing never stalls or freezes, including fast typing while messages arrive
- [x] Comfortable and Compact sizes; the drawer opens in each, and the log doesn't reflow
- [x] Ctrl palette: pick a command; pick a text size
- [x] Room menu: Who's here, Mute…, Unmute…, Mark read, Text size, Leave room
- [x] Muting a nick hides their messages; unmuting brings them back
- [x] Message menu: Reply inserts `@name `, Reveal unmasks a spoiler, Show full opens art
- [x] Show link displays the URL and a scannable QR code (try a GIF link)
- [x] Delete own message (with confirm) → `[deleted]`
- [x] Report (with confirm), with a reason typed in the input line
- [x] `/dice x` → error banner, and the text is back in the input line
- [x] The 16th fast send → "Slow down" banner, and the text stays
- [x] Up at the top loads older history; Down past the newest returns to typing
- [x] Screen off (hold centre), a tap shows the screensaver for 10 s, a hold wakes; other keys are ignored

**C-Mail (`m9_fake`):**

- [x] Fn under Home opens the list; rain shows bold with 2; Home shows "c-mail 2 unread"
- [x] Opening rain clears its unread; the title is `@rain` and Right opens no drawer
- [x] Send → echo, then "rain is typing…" for about 2 s, then rain's reply
- [x] Conversation menu: Mark read, Text size, Close conversation; the message menu has no Reply, Delete or Report
- [x] The Ctrl palette has no `/art` or `/muted`
- [x] In a cIRC room, wait for rain's 45 s DM: a banner plus the buzzer, and the list shows it unread
- [x] New conversation… → `kiwi` opens it; `zed` creates a new one; `nobody` → "User not found" and you stay in the input
- [x] Fn in a conversation goes to the list; Back from the list goes Home

**Feed (`m9_fake`):**

- [x] The list scrolls and pages
- [x] The CW row reads `CW: spiders`
- [x] `★` follows bookmarks
- [x] The 90 s reply alert shows a banner and beeps once
- [x] Entry: Centre reveals the CW
- [x] Entry: the long Markdown entry renders and scrolls
- [x] Entry: a threaded reply shows `↳`
- [x] Entry: replying to a reply sets the `↳ @name` marker
- [x] Entry: bookmark, watch and delete (own) work; delete on others' entries is refused
- [x] Composer: a multi-line body; the `n/4096` counter
- [x] Composer: 4 topics are rejected
- [x] Composer: the third post within a minute shows "Slow down"
- [x] Composer: posting opens the new entry
- [x] Composer: Back asks to discard

**Web config (`m9`):**

- [x] With no Wi-Fi saved: the setup AP appears, `http://192.168.4.1` loads, and nothing pops up
- [x] Scan lists networks; saving one restarts the device onto it
- [x] `http://rhino-cyberspace.local` and the IP both load the page
- [x] Status shows the version, IP, signal, battery and chat state
- [x] Removing a saved network works; with five saved, adding a sixth is refused
- [x] Display: text size and buzzer change on the device at once; the time zone changes the clock
- [x] Setting a page password → the browser asks for Basic auth (user `admin`); clearing it removes the prompt
- [x] Setting an AP password → WPA2 after a factory reset; clearing it → open again
- [x] Restart works; Factory reset (two confirms) → setup mode with everything erased
- [x] Home screen → Factory reset… → Erase everything does the same (now under Home → Settings)

**Device Settings (`m9`):**

- [ ] Home → Settings: Turn setup AP on → the AP appears with `http://192.168.4.1` while Wi-Fi stays up; Turn off removes it
- [ ] Log out → confirm → Setup screen; Wi-Fi kept; signing in again works
- [ ] Factory reset… → Erase everything → setup mode with everything erased
- [ ] With the saved network switched off for 5 minutes, the setup AP comes up; when the network returns (nobody joined) it goes away

**Open hardware checks (Wio Tracker L2, `wio_l2_fake` then `wio_l2`):**

Hardware bring-up only; the touch UI is still being designed. Until it lands,
BOOT tap = Down, BOOT hold (0.7 s) = Back, Wake tap = Select, Wake hold (1 s)
= screen off / wake.

- [ ] Boots: the serial log shows `[boot] … on WioL2`, `[wio-io] ready`, and `[touch] GT911 … at 0x5D`
- [ ] Splash shows `· WioL2`
- [ ] Display: landscape, right way up, nothing cut off at the edges; colours right (red is red, not blue; no inverted black)
- [ ] Backlight comes on at boot, and goes off and back on with Wake hold
- [ ] Battery %: Settings and the web Status page show a sensible value; it's `-1`/blank with no cell
- [ ] Beep: a mention or C-Mail alert in `wio_l2_fake` plays a tone with the buzzer on, with no click between sounds
- [ ] Buttons: BOOT tap moves down, BOOT hold goes back, Wake tap opens, Wake hold turns the screen off and back on
- [ ] Touch log: tapping the four corners logs `[touch] down x,y` near (0,0), (319,0), (0,239), (319,239) for top-left, top-right, bottom-left, bottom-right; lifting logs `[touch] up`
- [ ] Wi-Fi setup (`wio_l2`, nothing saved): the AP is named `Rhino-Cyberspace-WioL2-Setup` and `http://192.168.4.1` loads

**API assumptions to confirm (watch the serial log during the live checklist):**

- [ ] `GET /v1/users/me` shape: Home shows the right username and verified state
- [ ] `mutedUsersByRoom` shape: mutes made on the website hide messages on the device
- [ ] An expired or revoked token signs the device out cleanly, without a loop
- [ ] Rooms open whether the list gives an id or a slug

## Live checklist (human only)

Agents and scripts must never call the live Cyberspace API. A person runs
this checklist on the `m9` build with their own account.

- [x] Web login: sign in from the web page; Home shows the account
- [x] Join a room; history loads and new messages stream in
- [x] Send a plain message, a `/me` action, and a bad `/dice` (an error banner, and the text stays in the input line)
- [x] Scroll back past the first page of history
- [x] Open the drawer and see yourself in it
- [ ] Turn the screen off, have another account mention you from the website, and hear the buzzer within a minute (mentions are polled every 60 s; mentioning yourself makes no notification)
- [ ] Delete one of your own messages; the website shows the tombstone
- [ ] Turn Wi-Fi off and back on: ✕, then ● again, with no lost or duplicated messages
- [ ] Leave it running more than 70 minutes so it crosses a token refresh; chat keeps working

**C-Mail (live):**

- [x] The list matches the website's C-Mail, with the right unread counts
- [ ] Open a conversation: history loads; a message sent from the website streams in
- [ ] Send from the device; the website shows it. Typing on the device shows "…is typing" on the website, and it clears when you empty the line
- [ ] Typing on the website shows "name is typing…" on the device, and it goes away within ~9 s of stopping
- [ ] Opening a conversation marks it read on the website too
- [ ] Start a conversation by username; an unknown name shows a clear error
- [ ] With the screen off, a C-Mail from another account sounds the buzzer within a minute
- [ ] Names, previews and senders are right in the list and the log (this confirms the assumed field names, such as `senderUsername`)

**Feed (live):**

- [ ] Read the feed: entries, previews and reply counts match the website
- [ ] Post a test entry, then delete it; the website shows both
- [ ] Reply to an entry, and reply to a reply; the website threads them
- [ ] Bookmark and unbookmark an entry; the website agrees
- [ ] Watch and unwatch an entry; the website agrees
- [ ] A reply from a second account shows a banner (and the buzzer) within a minute
- [ ] With the Feed list open, auto-refresh shows a new entry within 2 minutes
