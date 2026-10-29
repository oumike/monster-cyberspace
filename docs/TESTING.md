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

## Open hardware checks

These were built without flashing and still need a run on the device. The
scripted fake-API builds (`m9_fake`, `wio_l2_fake`, `tdeck_fake`) were removed
on 2026-10-27; the sections marked `m9_fake` passed on it before then.

**Wi-Fi on the device (any board):**

- [ ] Settings → Wi-Fi lists the networks in range with bars, the joined one
  first; Scan again refreshes it.
- [ ] A new network with the right password joins and is saved; with a wrong
  one it fails after about 12 s, isn't kept, and the old network comes back.
- [ ] Other network… joins a hidden network by name.
- [ ] A saved network's Connect, New password and Forget work; forgetting the
  last one brings the setup AP up.
- [ ] After a factory reset, Join Wi-Fi on this device joins, returns to the
  setup screen, and the setup AP goes down.
- [ ] With 5 saved, a sixth is refused.

**Spell check (any board):**

- [ ] In a room, `teh ` opens the strip; Left/Right and Up/Down (wheel, side buttons) swap
  the word in the input; Centre keeps it without sending; a tap takes one.
- [ ] Centre on the untouched strip sends; typing on closes it.
- [ ] Works in a C-Mail, a Feed reply, and a new entry's title and body.
- [ ] Settings → Spell check off (and web config) stops it; survives a restart
  and a backup and restore.
- [ ] Suggestions appear without a noticeable pause after the space.

**Keyboard light (`m9`, `tdeck`, `tdeck_pro`, `pager`):**

- [ ] Settings → Keyboard light: Low, Medium and High are visibly different
  steps, and Off darkens the keys.
- [ ] The keys go dark with the screen and come back when it wakes
  (not the M9, whose keyboard lights itself on a press).
- [ ] The level survives a restart and a backup and restore.

**Themes (`m9`):**

- [x] Settings → Theme cycles Default → Black & white → White & black → Default; each redraws at once and survives a restart
- [ ] Web config Display → Theme changes the device at once; the select shows the saved theme on reload
- [ ] In both mono themes: Home, room, drawer, menus, palette, Feed list, Entry, Composer, login and Settings are readable; nicks use the text colour
- [ ] Link view QR stays black on white

**Room list filter (`m9`, `tdeck`, `pager`):**

- [ ] Typing on the room list narrows it to matching rooms (any case), the filter shows on the bottom line, Backspace erases a letter, Back clears it, and Enter opens the selected match
- [ ] With the filter empty, Back still goes Home (and Backspace does on the T-Deck and Pager); leaving and returning clears the filter

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
- [x] `http://monster-cyberspace.local` and the IP both load the page
- [x] Status shows the version, IP, signal, battery and chat state
- [x] Removing a saved network works; with five saved, adding a sixth is refused
- [x] Display: text size and buzzer change on the device at once; the time zone changes the clock
- [x] Setting a page password → the browser asks for Basic auth (user `admin`); clearing it removes the prompt
- [x] Setting an AP password → WPA2 after a factory reset; clearing it → open again
- [x] Restart works; Factory reset (two confirms) → setup mode with everything erased
- [x] Home screen → Factory reset… → Erase everything does the same (now under Home → Settings)

**Device Settings (`m9`):**

- [x] Home → Settings: Turn setup AP on → the AP appears with `http://192.168.4.1` while Wi-Fi stays up; Turn off removes it
- [x] Log out → confirm → Setup screen; Wi-Fi kept; signing in again works
- [x] Factory reset… → Erase everything → setup mode with everything erased
- [x] With the saved network switched off for 5 minutes, the setup AP comes up; when the network returns (nobody joined) it goes away

**Touch UI regression (`m9`):**

- [ ] Every menu and confirm in Room, Entry, Composer, Feed and Settings behaves as before the touch UI: Up/Down wrap, Centre/Right pick, Back/Left close, Cancel preselected
- [ ] Settings and web config show no Orientation; Home is the usual list

**Open hardware checks (Wio Tracker L2, `wio_l2`):**

Buttons: BOOT tap = Back, BOOT hold (0.6 s) = Home; Wake tap = screen off
(while dark: the 10 s screensaver), Wake hold (1 s) = screen off / wake.

- [x] Boots: the serial log shows `[boot] … on WioL2`, `[wio-io] ready`, and `[touch] GT911 … at 0x5D`
- [x] Splash shows `· WioL2`
- [x] Display: landscape, right way up, nothing cut off at the edges; colours right (red is red, not blue; no inverted black)
- [ ] Backlight comes on at boot, and goes off and back on with Wake hold
- [ ] Battery %: Settings and the web Status page show a sensible value; it's `-1`/blank with no cell
- [ ] Beep: a mention or C-Mail alert plays a tone with the buzzer on, with no click between sounds
- [ ] Buttons: BOOT tap goes back, BOOT hold goes Home; Wake tap turns the screen off, a Wake tap while dark shows the screensaver for 10 s, Wake hold wakes it
- [ ] Tap accuracy: in each of the four orientations, the `‹` (top-left), `⋯` (top-right, in a room) and the bottom-left and bottom-right Home tiles or keyboard keys respond where tapped (this confirms the Portrait and Portrait 180 touch direction)
- [ ] Every screen by touch: Home tiles, Rooms, Room (tap a message, hold for the menu, the drawer, the menus), C-Mail, Feed (drag, swipe a page, load more at the end), Entry, Composer (fields, checkboxes, `⋯` Post/Discard), Settings, Login
- [ ] Keyboard: tapping a field docks it and hides it again with `▼`; all three pages (`?123`, `#+=`, `ABC`); `⇧` one-shot and double-tap caps lock; `⌫`; enter sends in a room and makes a new line in the Composer Body; keys are about 32 px wide in landscape and 24 px in portrait
- [ ] Keyboard goes away on Back out of a room, on a screen change and when the screen turns off
- [ ] `‹` tap goes back and hold goes Home; `⋯` opens the screen menu; a swipe right from the left edge goes back; a tap on a banner dismisses it; tapping outside a menu closes it
- [ ] Touch never wakes a dark screen, and a finger left down across a screen change fires nothing on the new screen
- [ ] Orientation from Settings and from web config (Display) applies at once and survives a reboot; in portrait no screen has text cut or overlapping (Settings and Setup lines wrap)
- [ ] Wi-Fi setup (`wio_l2`, nothing saved): the AP is named `Monster-Cyberspace-WioL2-Setup` and `http://192.168.4.1` loads

**T-Deck Plus (`tdeck`):**

- [ ] Boots: the serial log shows `[boot] … on TDeck`, `[kb] keyboard found` and `[touch] GT911 … at 0x5D`
- [ ] Splash shows `· TDeck`
- [ ] Display: landscape with the keyboard below, right way up, nothing cut off; colours right. If it's mirrored or upside down, change `PANEL_MADCTL` in `src/hal/tdeck/pins_tdeck.h` (0x60 now; try 0xA0, 0x00 or 0xC0 with MV for landscape)
- [ ] Touch: the `‹` (top-left), `⋯` (top-right, in a room) and the four corner Home tiles respond where tapped. If taps land mirrored or a half turn off, change `TOUCH_ROT` in `pins_tdeck.h` (2 now)
- [ ] Every key types the right character, with Shift, Alt and Sym
- [ ] Backspace deletes while typing; with nothing to delete it goes back; holding it in a room clears the input and then stays in the room
- [ ] Trackball: each roll moves the matching way (lists and Home tiles); a click selects; rolling then clicking opens the tile it stopped on
- [ ] Holding the click 2 s turns the screen off, and releasing it selects nothing
- [ ] With the screen off, a key, a roll, a click and a touch each wake it, and none of them types, moves, selects or taps
- [ ] Tapping a text field focuses it without the on-screen keyboard; there's no Orientation item in Settings
- [ ] `⋯` and a long-press open the palette in a room
- [ ] Beep: a mention or C-Mail alert plays a tone with the buzzer on
- [ ] Battery %: Settings shows a sensible value; the charging state follows the USB cable
- [ ] GPS: outdoors with Wi-Fi off, `[gps] fix, …` and `[time] set from GPS` within 15 min, then `[gps] off`; no log line shows coordinates
- [ ] SD (FAT32 card): Settings → Back up to SD card, then Restore from SD card…, both work
- [ ] Pulling the card mid-backup gives "SD write failed", and the panel keeps drawing
- [ ] Wi-Fi setup (`tdeck`, nothing saved): the AP is named `Monster-Cyberspace-TDeck-Setup` and `http://192.168.4.1` loads
- [ ] Regression after the shared-driver change: the M9 panel, the Wio touch and both boards' battery % still work as before

**CrowPanel Advance 3.5" (`crowpanel35`) and 5.0" (`crowpanel50`):**

- [ ] Boots: the serial log (115200, the CH340 port) shows `[boot] … on CrowPanel35` / `CrowPanel50` and `[touch] GT911 … at 0x5D` (or 0x14)
- [ ] 5.0": the log names the board revision (`V1.0 board (PCA9557)` or `V1.1+ board (MCU)`) and the backlight comes on
- [ ] Display: landscape, right way up, colours right, nothing cut off. On the 3.5" a wrong half turn is `offset_rotation` in `src/hal/crowpanel/panel_ili9488.cpp` (3 now)
- [ ] Touch: `‹`, `⋯` and the four corner Home tiles respond where tapped, in every orientation. 3.5": `readTouch` in `board_crowpanel35.cpp`; 5.0": `readTouch` in `board_crowpanel50.cpp` and the rotate-copy in `panel_rgb800.cpp`
- [ ] Settings → Orientation: all four turns draw upright and full size (on the 5.0", note which portrait is the right way up)
- [ ] The status bar is taller, with bigger text and icons, and easy to tap
- [ ] The on-screen keyboard docks and types in every orientation
- [ ] 5.0": scrolling a long room log redraws without tearing or stale rows
- [ ] 3.5" SD (FAT32 card): Back up to SD card, then Restore from SD card…, both work
- [ ] 3.5" GPS (module on UART1, optional): `[time] set from GPS` outdoors with Wi-Fi off
- [ ] Wi-Fi setup (nothing saved): the AP is named `Monster-Cyberspace-CrowPanel35` / `…-CrowPanel50` and `http://192.168.4.1` loads

**T-LoRa Pager (`pager`):**

- [ ] Boots: the serial log (USB) shows `[boot] … on Pager`, `[pager-io] 0x…` (the expander) and `[kb] keyboard found`
- [ ] Display: landscape, right way up, colours right, nothing cut off or shifted. Mirrored or upside down: `PANEL_MADCTL` in `src/hal/pager/pins_pager.h` (0xB8 now); a band of noise or a cut edge: `PANEL_ROW_OFFSET` (49 now)
- [ ] Wheel: turning it up moves the selection up, one row per detent, in lists and on Home. If reversed, flip `kPositiveIsUp` in `src/hal/pager/wheel_pager.cpp`
- [ ] Wheel click selects; holding it 2 s turns the screen off and selects nothing
- [ ] Every key types the right character, with Shift and Sym; Space, Enter and Backspace work; holding Backspace repeats
- [ ] Sym + Backspace goes Home; Sym + Enter opens the palette on the Feed list
- [ ] While typing, Sym opens the symbol table: the wheel steps one symbol at a time and wraps, a click inserts, a letter types its Sym character and closes it; Sym again switches to the emoji tray, where a click inserts; Backspace or a BOOT tap closes either
- [ ] BOOT: a tap goes back, a hold goes Home
- [ ] With the screen off, a key, a wheel turn and a click each wake it, and none of them types, moves or selects
- [ ] Beep: a mention or C-Mail alert plays a tone on the speaker with the buzzer on
- [ ] Battery %: Settings shows a sensible value; the charging state follows the USB cable
- [ ] RTC: after a reboot with Wi-Fi off, the clock is right at once (`[time] set from RTC`)
- [ ] GPS: outdoors with Wi-Fi off, `[time] set from GPS` within 15 min, then `[gps] off`; no log line shows coordinates
- [ ] SD (FAT32 card): Back up to SD card, then Restore from SD card…, both work
- [ ] Wi-Fi setup (`pager`, nothing saved): the AP is named `Monster-Cyberspace-Pager-Setup` and `http://192.168.4.1` loads
- [ ] Regression after the shared-driver change: the Wio L2 speaker and the M9/T-Deck panels still work as before

**T-Deck Pro (`tdeck_pro`):**

- [ ] Boots: the serial log (USB) shows `[boot] … on TDeckPro`, `[kb] keyboard found` and `[touch] CST328` or `[touch] CST3530`
- [ ] E-paper: portrait, black on white, nothing cut off; the first frame is a full refresh, typing updates with partial refreshes once you pause, and ghosting clears every so often
- [ ] Selections, the Home tiles, the symbol table and error banners are outlined, not filled; the splash cursor doesn't blink
- [ ] Settings and web config show no Theme item
- [ ] Every key types the right character, with Shift and Sym; holding Backspace repeats; Alt + W/A/S/D move, and repeat when held
- [ ] Sym/Alt + Backspace go back, Sym/Alt + Enter open the palette, Sym + Space and Alt + H go Home, Alt + C/M/F open cIRC, C-Mail and Feed
- [ ] Keys typed quickly during a refresh are not lost
- [ ] Touch: a tap lands where the finger is, and a swipe scrolls
- [ ] The setup screen shows "Log in on device" and "Restore from SD" (with a backup on the card) as buttons, side by side or stacked at bigger text sizes, and each works
- [ ] Confirms (Restore, Log out, Factory reset…) show Cancel and the action as two buttons above the hint; each works, and a tap outside cancels
- [ ] The room list and Settings are buttons; the status bar shows `‹` and `⋯`; holding a log or Feed row opens its menu
- [ ] Nicks in the room log and entry replies are bold and still legible at every text size
- [ ] Side strip (room, conversation, C-Mail, Feed, entry): ▲/▼ move the selection, ● sends or opens the room menu when nothing is selected and opens the reply/report menu on a selected message; it steps aside when the member drawer is open and nothing on the screen is cut off
- [ ] Mic key while typing opens the emoji tray; Sym + Mic types `0`; elsewhere the Mic key does nothing
- [ ] Side button: turns the screen off (the panel keeps the still splash), and on again; keys don't wake it
- [ ] Battery %: Settings shows a sensible value; the charging state follows the USB cable
- [ ] GPS: outdoors with Wi-Fi off, `[time] set from GPS` within 15 min, then `[gps] off`
- [ ] SD (FAT32 card): Back up to SD card, then Restore from SD card…, both work
- [ ] Wi-Fi setup (nothing saved): the AP is named `Monster-Cyberspace-TDeckPro` and `http://192.168.4.1` loads

**GPS time, RTC and time zone (`m9`):**

- [ ] With Wi-Fi in range, `[time] NTP sync` and `[time] RTC written` appear, then `[gps] off`
- [ ] A Wi-Fi drop and rejoin logs a fresh `[time] NTP sync`
- [ ] After a reboot with the saved network switched off, the status-bar clock is right at once (`[time] set from RTC`), and `[gps] on` follows
- [ ] Outdoors with Wi-Fi off, a GPS fix sets the clock: `[gps] fix, …` and `[time] set from GPS` within 15 min
- [ ] Setup page: the Wi-Fi section's Time zone starts on the browser's zone; saving Wi-Fi saves it, and the clock shows local time after the restart
- [ ] Settings → Time zone: the list opens on the current zone; Centre picks another and the status-bar clock changes at once; Back leaves it unchanged
- [ ] A Wi-Fi network that only appears after boot (switch it on 2 min in): `[wifi] connected` is followed by `[time] NTP sync`
- [ ] The GPS turns off afterwards: `[gps] off` once the time is set
- [ ] No log line shows coordinates
- [ ] The Settings screen shows gps, time and zone in both text sizes; with the setup AP on at Comfortable size nothing overlaps the menu
- [ ] The web Status page shows the same three lines
- [ ] Web config Time: picking a zone applies it at once; Custom shows the POSIX field and a typed string applies
- [ ] NTP server field: a bad server shows the error; `time.cloudflare.com` logs `[time] NTP sync` after a Wi-Fi rejoin

**GPS time and time zone (`wio_l2`, no RTC):**

- [ ] Outdoors with Wi-Fi off, a GPS fix sets the clock: `[gps] fix, …` and `[time] set from GPS` within 15 min
- [ ] The GPS turns off afterwards: `[gps] off` once the time is set
- [ ] No log line shows coordinates

**Settings backup (`m9`, then `wio_l2`; FAT32 card):**

- [ ] Settings → Back up to SD card shows `Saved rc-backup-….yaml`, and the file is in `/monster-cyberspace/backups/`
- [ ] The file opens on a computer and holds every setting (Wi-Fi, time zone, NTP server, passwords, preferences, account)
- [ ] Settings → Restore from SD card… lists backups newest first, Cancel is preselected, and Restore restarts with the same settings
- [ ] After a factory reset, with the card holding backups, Setup offers Restore from SD card and the device comes back signed in
- [ ] Web config, with a page password set: Download backup, then Restore from file… with that file restarts with the same settings
- [ ] Web config: Save backup to SD card, then the list's restore, download and delete each work (restore and delete confirm)
- [ ] `curl -u admin:<pw> -X POST 'http://<ip>/api/backup/sd/delete?name=..%2Fx.yaml'` gives `Bad file name`
- [ ] Uploading a 17 KB file gives `Backup is over 16 KB`
- [ ] With the card out, every path (both Settings items and the web SD block) says `No SD card`; putting it back in works without a restart

**OTA updates and the flash layout (`m9`, then each board):**

- [ ] A board on the old layout (flashed with `pio run -t upload` before this change) keeps Wi-Fi, sign-in and preferences after `pio run -t upload` of this build; the log shows `[nvs] moved N settings`, and the next boot does not
- [ ] The same with a release's factory image through `flash.sh`
- [ ] A factory image through `flash.sh` on a board already on the new layout keeps the settings
- [ ] Settings → Firmware: `checking…`, then `vX up to date` (or `vX available` with a newer release), and the log shows `[ota] latest …`
- [ ] With a newer release published: Install shows `installing N%`, restarts into it, settings kept, and the log shows `[ota] new firmware is running fine` after 30 s
- [ ] A release whose `.sig` does not match (edit one byte locally and host it, or test against a release with the `.sig` removed) fails with `signature check failed` / `signature: HTTP 404`, and the device keeps running the old firmware
- [ ] Auto-update on: the daily check (or 20 s after joining Wi-Fi) installs by itself; off: it only shows `available`
- [ ] Web config Firmware: Check, Install and the auto-update checkbox work and match the device
- [ ] Off Wi-Fi, Settings → Firmware says `Can't check now: is Wi-Fi up?`

**CrowPanel double-tap wake (`crowpanel35`, `crowpanel50`):**

- [ ] With the screen locked or off, one tap does nothing; two taps within half a second wake it, and neither tap acts on the screen

**Touch emoji button (`wio_l2`, `tdeck`, `crowpanel35`, `crowpanel50`):**

- [ ] The room, conversation and Feed reply inputs show an emoji button left of the prompt; tapping it opens the emoji tray
- [ ] Tapping an emoji inserts it and the tray stays open; tapping outside the tray closes it; the C-Mail "to>" line and the M9/Pager inputs show no button

**T-Deck Plus mic key (`tdeck`):**

- [ ] While typing, the mic key opens the emoji tray once per press; outside text entry it does nothing, and the other keys still type normally (it reads the keyboard's raw matrix only while typing; `micPressed` in `src/hal/tdeck/keyboard_tdeck.cpp`)

**Room wrapping (every board):**

- [ ] At Comfortable and Large, a long room message's second and later lines start under the nick; at Compact they start under the text

**Large emoji tray (every board):**

- [ ] Where there's room (portrait panels, the CrowPanels, Compact text) the tray draws emoji four cells by two, legible and evenly spaced; the selection outline wraps the whole emoji and a tap anywhere on it inserts; on short screens it falls back to two-cell emoji

**M9 emoji tray (`m9`):**

- [ ] Sym opens the command palette everywhere, as before
- [ ] While typing, Ctrl opens the emoji tray (six or more a row, one blank cell apart, a blank row between rows); on the Feed list Ctrl opens the palette
- [ ] Emoji tray: Centre inserts and the tray stays open, Back closes it, other keys do nothing; emoji are legible at each text size and fill two cells in the input line, the log and the composer
- [ ] A received message with tray emoji (and with `❤️`, which carries a variation selector) shows them; so do others (🚀 🎉 🐱 🍕), and skin-toned ones (👍🏽) show plain
- [ ] Received Greek, Cyrillic, accented Latin and symbols (α Ж ő ↔ ♥ ☺ ≈) show; CJK shows as `�`

**Screen timeout and screensaver (each board):**

- [ ] With a 15 s timeout and no input, the backlight dims at 5 s and the screensaver (bouncing monster, dim) starts at 15 s; a key or touch during the dim brings full brightness back
- [ ] Input in the first 3 s after the lock is ignored; after that the board's wake input wakes, and does nothing else
- [ ] On the M9 and Wio L2, a centre/Wake tap on the screensaver shows the last messages for 10 s, then the screensaver comes back
- [ ] At timeout: screen off goes dark instead; Never keeps the screen on
- [ ] An incoming message does not wake the screen or restart the timeout
- [ ] Settings and web config show and save both settings; a backup holds `screenOnSecs` and `screenSaver`
- [ ] Dimming works on each board (the CrowPanel 5.0" V1.0 PCA9557 is on/off only, so it doesn't dim)

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
