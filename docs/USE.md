# Using Rhino Cyberspace on the ThinkNode M9

## First setup

1. Power on. With no Wi-Fi saved, the device starts its setup access point,
   **`Rhino-Cyberspace-M9-Setup`**, and the screen shows its name and the
   address to browse.
2. Join that network from a phone or laptop. It is open unless you set a
   setup-AP password earlier, in which case the screen says a password is
   required.
3. Browse to **`http://192.168.4.1`**. Nothing pops up on its own: there is no
   captive portal, so type the address in.
4. Under **Wi-Fi**, scan or type your network name, enter its password and
   save. The device restarts and joins that network.
5. On your LAN, open **`http://rhino-cyberspace.local`** (or the IP shown on the
   Home screen). Under **Cyberspace account**, sign in with your email and
   password. The device signs in, keeps only a refresh token, and discards the
   password.

If you can't use the web page, the setup screen also has **Log in on this device**,
which takes your email and password on the M9 keyboard.

## Buttons

Dedicated buttons work from anywhere and close whatever is open first.

| Button | Action |
|---|---|
| Messages (far left) | cIRC: the last room. Press it again in a room for the room list |
| Home | Home dashboard |
| Fn under Home | C-Mail: the conversation list. Press it again in a conversation for the list |
| Fn under Back | Feed: the entry list. Press it again anywhere in the Feed for the list |
| Map | Unassigned |
| Back | Up one level, or closes a menu or drawer. It never deletes text |
| Ctrl | In a room or conversation: the command palette (slash commands and text size) |
| Hold Back/Del | Clear the input line |
| Hold d-pad centre | Screen off; with the screen off, screen on |

**Screen off:** only holding the d-pad centre wakes the screen. A centre tap
while it's dark shows the last few messages for 10 s, then goes dark again.
Every other key is ignored while the screen is off, so pocket presses can't
send anything.

**Lists:** Up/Down select and wrap round at the ends, Centre or Right open,
Left goes back.

## In a room

The keyboard always types into the input line at the bottom.

- **Send:** Centre or Enter with text in the line. The text shows dimmed as
  *sending…* until the server echoes it back. If the send fails, the text
  returns to the line and a banner explains why.
- **Room menu:** Centre on an empty line. It has Who's here, Mute…, Unmute…,
  Mark read, Text size and Leave room.
- **Scroll:** Up/Down highlight messages. Up at the top loads older history.
  Down past the newest message returns you to typing.
- **Message menu:** Centre on a highlighted message. It has Reply (`@name `),
  Reveal (spoilers), Show full (long or art messages), Show link (as text and
  a QR code), Delete (your own, with confirmation) and Report (with
  confirmation and an optional reason).
- **Cursor:** Left/Right move the text cursor.
- **Who's here drawer:** Right with the cursor at the end of the line opens
  it. Up/Down pick a person, Centre opens their menu (mention, mute or
  unmute), and Left or Back closes it. The drawer stays open or closed as you
  move between rooms.
- **Commands:** type slash commands such as `/me`, `/dice` and `/mute name`,
  or pick them from the Ctrl palette.

Sending is disabled while offline; nothing is queued or retried for you.

## C-Mail

C-Mail is private, one-to-one messaging. Open it with Fn under Home, or
**C-Mail** on the Home screen.

- **The list** shows **New conversation…** first, then your conversations,
  unread first. Unread ones are bold with a count, followed by a preview of
  the last message.
- **New conversation…:** Centre, type a username (no `@`), then Centre. You
  land in that conversation, or a banner explains why not (for example, no
  such user). Back cancels.
- **A conversation** works like a room, titled `@name`: type and press Centre
  to send. Scrolling, the reader and links work the same way. There is no
  drawer. Its menu (Centre on an empty line) has Mark read, Text size and
  Close conversation. The message menu has Reveal, Show full and Show link;
  C-Mail has no reply, delete or report.
- **Typing:** while you type, the other person sees "…is typing". The device
  tells the server only as you press keys, and clears it when you empty the
  line or leave. When they type, "name is typing…" shows above your input
  line.

## Feed

The Feed is the Cyberspace timeline of entries and their replies. Open it
with Fn under Back, or **Feed** on the Home screen. Home shows the unread
Feed alerts; with one waiting, **Feed** opens its entry instead of the list.

**The list:**

- Each entry takes two lines: `nick · 3h · ↩5` (replies), plus `★` if you
  bookmarked it, then the title or the first line of the body, dimmed. An
  entry with a content warning or the NSFW flag shows `CW: …` instead.
- Up/Down select. Down on the last entry loads the next page; once every page
  is loaded it wraps to the top, and Up on the first wraps to the last.
- Centre or Right opens the entry. Left or Back goes Home.
- **Ctrl** opens the palette: **New entry**, **Refresh**, **Watched
  threads**, **Bookmarks** and **Text size**.
- **Auto-refresh:** while the list is on screen and the screen is on, it
  re-fetches the first page every 2 minutes (the title reads `Feed ↻ 2m`).
  New entries appear at the top and your selection stays put. Refresh in the
  palette does it at once.

**An entry:**

- A header (author, time, topics, `public`/`NSFW`), the body, a
  `── N replies ──` divider, then the replies. A reply to a reply has a dim
  `↳ @parent` line above it.
- Up/Down select the body or a reply. Down past the last reply loads more.
  Back with something selected returns to typing; Back again goes to the list.
- A content warning hides the body until you press Centre on it.
- **Reply:** type, then Centre or Enter. With a reply selected, you answer it
  and `↳ @name` shows above the input line; otherwise you answer the entry.
  Nothing is retried: a failed reply stays in the line.
- **Entry menu** (Centre on an empty line): Bookmark/Unbookmark,
  Watch/Unwatch, Show link (text and a QR code), Delete… on your own entry or
  Report… on others' (both confirm; a report takes an optional reason), and
  Text size.
- **Reply menu** (Centre on a selected reply): Reply, Bookmark/Unbookmark,
  Show link, and Delete… (yours) or Report… (others').

**New entry** (palette → New entry):

- Fields: Title, Body, Topics (comma-separated, up to 3), Public [ ],
  NSFW [ ] and CW.
- Up/Down move between fields. In Body they move by line first, and Centre
  adds a new line; the counter shows `n/4096`. On a checkbox, Centre toggles
  it.
- **Ctrl** opens **Post…** and **Discard…**. Post checks the draft, then asks
  "Post entry? (2 per minute, 24 per day)". A posted entry opens at once; a
  failed one keeps the draft.
- Back (or Fn under Back) with any text asks "Discard draft?". The draft
  stays in memory until you post or discard it.

**Watched threads and Bookmarks** are lists in the same style, from the
palette. Centre opens the entry; a bookmarked reply opens its entry with that
reply selected. Left or Back returns to the Feed list.

## Text sizes

Switch from the palette, the room menu or the web page. The choice is saved.

- **Comfortable:** larger 8×16 text, `nick message`, with a time divider when
  the minute changes.
- **Compact:** 6×12 text, `14:02 nick: message`, with a hint row at the
  bottom.

## Themes

Pick a theme on the device (**Home** → **Settings** → **Theme**, each Centre
press moves to the next one) or in web config under Display. The choice is
saved.

- **Default:** the terminal look, with coloured nicks.
- **Black & white:** white text on black, in greys only.
- **White & black:** black text on white, in greys only.

In the two mono themes nicks are drawn in the text colour. QR codes stay
black on white in every theme.

## Alerts

While you're signed in, the device checks every minute, even with the screen
off. Rooms with new messages are marked in the room list. A new mention of
you shows a banner and sounds the buzzer, if the buzzer is turned on in web
config. Mentions are marked read when you open that room and scroll to the
bottom.

A new C-Mail does the same: a banner (`C-Mail from @name: …`) and the buzzer,
unless that conversation is open. Home shows the total unread C-Mail.

Feed alerts come from the same check: a reply to your entry or reply, or a
mention in one, shows a banner (`@name replied: …`) and sounds the buzzer if
it's on. If that entry is open, its replies just refresh instead. Home counts
them as Feed unread. An entry held for review shows a banner only.

## Web config

At `http://rhino-cyberspace.local`, or the device IP, on your LAN:

- Status
- Wi-Fi: scan, add, and remove saved networks (up to 5)
- Cyberspace account: sign in, sign out, resend verification
- Display: text size, theme, buzzer
- Time: zone mode, zone and NTP server (see Time and time zone below)
- Security: page password and setup-AP password
- Restart and factory reset

The page footer carries the attribution for the bundled zone data:

Time zone boundaries © OpenStreetMap contributors (ODbL), from timezone-boundary-builder.

### Security notes

The page is plain HTTP on your LAN, so set a page password on shared networks
(user `admin`). The setup AP is open unless a setup-AP password is set, so
anyone in radio range can join it while it runs. It runs with no saved
Wi-Fi (first boot or after a factory reset), when you turn it on in Settings,
or after the saved networks fail for 5 minutes, and anything entered on it
(Wi-Fi password, Cyberspace password) crosses that link unencrypted. Setting a
setup-AP password closes that gap for later setups. The refresh token can't be
read back through the page.

## Time and time zone

The clock sets itself from the best source it has:

- **NTP** over Wi-Fi, best. It syncs each time Wi-Fi connects, from the NTP
  server set in web config (default `pool.ntp.org`), then `time.google.com`.
- **GPS**, when there's no Wi-Fi. A GPS time is used only when the clock is
  unset or hasn't synced in the last hour, so it never overrides a fresh NTP
  time.
- **RTC** (M9 only; its PCF8563 chip keeps time while the device is off). The
  device writes it after an NTP or GPS sync, at most once an hour, and reads
  it at boot, so the clock is right before Wi-Fi or GPS. The Wio Tracker L2
  has no RTC, so its clock is unset until NTP or GPS.

The GPS powers itself on only while it's needed: when the clock is unset or
its last sync is over 6 hours old, or, in Automatic zone mode, when there's
no position from the last 6 hours. It turns off once those needs are met. A
search with no fix gives up after 15 minutes and tries again an hour later.
For a first fix, take the device outdoors with a view of the sky.

Pick the zone mode in web config under **Time**:

- **Automatic, from GPS** (the default on boards with GPS). The zone comes
  from the GPS position, looked up offline in zone boundaries stored on the
  device. It's found on the first fix after boot and again after moving more
  than 10 km, and it's saved, so the clock shows local time after a reboot
  even before a fix. Until the first fix, the **Zone** below is used. Out at
  sea, with no zone, the clock uses whole hours from the longitude
  (`Etc/GMT±n`).
- **Manual.** The **Zone** you pick (or the POSIX TZ string you type) applies
  at once.

**NTP server** takes a host name (1–63 characters). **Save time** saves all
three fields.

**Home** → **Settings** and the web Status page show three lines:

- **gps:** `none` (no GPS on this board), `off`, `searching m:ss`, or
  `fix, N of M satellites`.
- **time:** the source and age of the last sync, such as `NTP, just now` or
  `GPS, 12 min ago`; `RTC, at boot`; or `not set`.
- **zone:** the zone name with `(auto)` or `(manual)`, or the manual zone with
  `(auto, no fix yet)` before the first fix.

Privacy: your position is never sent anywhere, never stored and never logged.
The serial log says only whether there's a fix, satellite counts and the zone
name. Only the zone name and its POSIX string are saved.

## Settings

**Home** → **Settings** shows the account, the Wi-Fi network and IP, the web
config address, the battery, the setup AP state and the gps, time and zone
lines (see Time and time zone), with four actions:

- **Theme: …** Centre moves to the next theme (Default, Black & white, White &
  black) and saves it.
- **Turn setup AP on / off.** Brings up the setup AP (its name, password state
  and `http://192.168.4.1` are shown) while staying on your Wi-Fi, so you can
  reach web config when your network is unreachable. It stays on until you
  turn it off or restart.
- **Log out** (confirm with **Log out**). Signs out of Cyberspace and goes to
  the Setup screen. Wi-Fi and preferences are kept.
- **Factory reset…** (confirm with **Erase everything**). See below.

If the saved networks fail for 5 minutes, the setup AP comes up by itself.
It goes away again once Wi-Fi is back and nobody is joined to it.

## Back to setup mode

A **factory reset** erases saved Wi-Fi, the account token, preferences and
both passwords, then restarts into setup mode. You can do it from either
place:

- **Home** → **Settings** → **Factory reset…** → select **Erase everything**.
- Web config → **Factory reset…** (you confirm twice).
