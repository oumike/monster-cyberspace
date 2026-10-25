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
scripts/build-upload-monitor.sh m9_fake
```

The network task is replaced by a scripted source, so no account or Wi-Fi is
needed. The web server does not run in this build.

The fake source provides:

- the rooms `general`, `linux` and `music`, with fixture history and a new
  message every 4 s;
- 12 people, 3 of them idle;
- sends that echo back after 300 ms;
- a `400` for `/dice x`;
- the real local send limit, so the 16th fast send shows the rate-limit
  banner.

Check every screen and key:

- both text sizes, and the drawer in each
- the palette and the room, message and person menus
- spoilers and art
- a QR code on a link
- the error banners
- screen off, the screensaver and wake

## Open hardware checks

These were built without flashing and still need a run on the device.

**Room screen (`m9_fake`):**

- [ ] Typing never stalls or freezes, including fast typing while messages arrive
- [ ] Comfortable and Compact sizes; the drawer opens in each, and the log doesn't reflow
- [ ] Ctrl palette: pick a command; pick a text size
- [ ] Room menu: Who's here, Mute…, Unmute…, Mark read, Text size, Leave room
- [ ] Muting a nick hides their messages; unmuting brings them back
- [ ] Message menu: Reply inserts `@name `, Reveal unmasks a spoiler, Show full opens art
- [ ] Show link displays the URL and a scannable QR code (try a GIF link)
- [ ] Delete own message (with confirm) → `[deleted]`
- [ ] Report (with confirm), with a reason typed in the input line
- [ ] `/dice x` → error banner, and the text is back in the input line
- [ ] The 16th fast send → "Slow down" banner, and the text stays
- [ ] Up at the top loads older history; Down past the newest returns to typing
- [ ] Screen off (hold centre), a tap shows the screensaver for 10 s, a hold wakes; other keys are ignored

**Web config (`m9`):**

- [ ] With no Wi-Fi saved: the setup AP appears, `http://192.168.4.1` loads, and nothing pops up
- [ ] Scan lists networks; saving one restarts the device onto it
- [ ] `http://rhino-cyberspace.local` and the IP both load the page
- [ ] Status shows the version, IP, signal, battery and chat state
- [ ] Removing a saved network works; with five saved, adding a sixth is refused
- [ ] Display: text size and buzzer change on the device at once; the time zone changes the clock
- [ ] Setting a page password → the browser asks for Basic auth (user `admin`); clearing it removes the prompt
- [ ] Setting an AP password → WPA2 after a factory reset; clearing it → open again
- [ ] Restart works; Factory reset (two confirms) → setup mode with everything erased
- [ ] Home screen → Factory reset… → Erase everything does the same

**API assumptions to confirm (watch the serial log during the live checklist):**

- [ ] `GET /v1/users/me` shape: Home shows the right username and verified state
- [ ] `mutedUsersByRoom` shape: mutes made on the website hide messages on the device
- [ ] An expired or revoked token signs the device out cleanly, without a loop
- [ ] Rooms open whether the list gives an id or a slug

## Live checklist (human only)

Agents and scripts must never call the live Cyberspace API. A person runs
this checklist on the `m9` build with their own account.

- [ ] Web login: sign in from the web page; Home shows the account
- [ ] Join a room; history loads and new messages stream in
- [ ] Send a plain message, a `/me` action, and a bad `/dice` (an error banner, and the text stays in the input line)
- [ ] Scroll back past the first page of history
- [ ] Open the drawer and see yourself in it
- [ ] Turn the screen off, mention yourself from the website, and hear the buzzer
- [ ] Delete one of your own messages; the website shows the tombstone
- [ ] Turn Wi-Fi off and back on: ✕, then ● again, with no lost or duplicated messages
- [ ] Leave it running more than 70 minutes so it crosses a token refresh; chat keeps working
