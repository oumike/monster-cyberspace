# Using Monster for Cyberspace

On the Elecrow ThinkNode M9 (keyboard and d-pad), the Seeed Wio Tracker L2
(touch screen; see Touch below) and the LilyGO T-Deck Plus (keyboard,
trackball and touch; see T-Deck Plus below), and the Elecrow CrowPanel
Advance 3.5" and 5.0" (touch only; see CrowPanel below), and the Attaky Mesh
Deck (d-pad, touch and keyboard; see Mesh Deck below), and the Xteink X4
(e-paper and six buttons, a mostly passive client; see Xteink X4 below). Key
names in this guide are the M9's.

## First setup

1. Power on. With no Wi-Fi saved, the device starts its setup access point,
   **`Monster-Cyberspace-M9-Setup`** (`…-WioL2-Setup` on the Wio Tracker L2,
   `…-TDeck-Setup` on the T-Deck Plus, `Monster-Cyberspace-CrowPanel35`
   and `Monster-Cyberspace-CrowPanel50` on the CrowPanels, whose names would
   pass Wi-Fi's 32-character limit with `-Setup`), and the screen shows its name and the
   address to browse.
2. Join that network from a phone or laptop. It is open unless you set a
   setup-AP password earlier, in which case the screen says a password is
   required.
3. Browse to **`http://192.168.4.1`**. Nothing pops up on its own: there is no
   captive portal, so type the address in.
4. Under **Wi-Fi**, scan or type your network name, enter its password,
   check the **Time zone** (it starts on your browser's zone) and save. The device restarts and joins that network.
5. On your LAN, open **`http://monster-cyberspace.local`** (or the IP shown on the
   Home screen). Under **Cyberspace account**, sign in with your email and
   password. The device signs in, keeps only a refresh token, and discards the
   password.

If you can't use the web page, the setup screen also has **Join Wi-Fi on this
device** (see Wi-Fi on the device below) and **Log in on this device**, which
takes your email and password on the keyboard or the on-screen keyboard. A
network joined there takes you back to the setup screen, and the setup AP goes
down once nobody is joined to it.

## Wi-Fi on the device

**Home** → **Settings** → **Wi-Fi** (or **Join Wi-Fi on this device** during
setup) scans and lists the networks in range, strongest first, with signal
bars. The connected one comes first, and saved networks out of range are
listed as `away`. Below them are **Other network…**, for a hidden network you
name yourself, and **Scan again**.

- A new network asks for its password (an open one joins at once). Leave the
  password empty for an open hidden network. The password is shown as you
  type.
- The device tries it at once, for up to 12 seconds, and says whether it
  joined. A new network that fails isn't kept. If it fails, the device goes
  back to the other saved networks.
- A saved network offers **Connect**, **New password** and **Forget**.
- Up to 5 networks are saved, as in web config: with 5 saved, forget one
  before adding another.

## Buttons

Dedicated buttons work from anywhere and close whatever is open first.

| Button | Action |
|---|---|
| Messages (far left) | cIRC: the last room. Press it again in a room for the room list |
| Home | Home dashboard |
| Fn under Home | C-Mail: the conversation list. Press it again in a conversation for the list |
| Fn under Back | Feed: the entry list. Press it again anywhere in the Feed for the list |
| Map | Guilds: the guild list, once you are signed in. In the composer it asks "Discard draft?" first |
| Back | Up one level, or closes a menu or drawer. It never deletes text |
| Sym | The command palette (slash commands in a room or conversation) |
| Ctrl | While typing: the emoji tray. Elsewhere (the Feed list): the palette |
| Hold Back/Del | Clear the input line |
| Hold d-pad centre | Screen off; with the screen off, screen on |

**Emoji tray:** Ctrl, while you're typing, opens 40 emoji (camillia-mt's tray):
the d-pad moves, Centre inserts one and keeps the tray open for more, and Back
closes it; other keys do nothing while it's open. Emoji take two cells.

**Screen off:** only holding the d-pad centre wakes the screen. A centre tap
while it's dark shows the last few messages for 10 s, then goes dark again.
Every other key is ignored while the screen is off, so pocket presses can't
send anything.

**Lists:** Up/Down select and wrap round at the ends, Centre or Right open,
Left goes back.

## Touch (Seeed Wio Tracker L2)

Every screen works by touch. Touch never wakes a dark screen.

- **Tap** a row to open it, or a field to type in it. **Hold** a row (half a
  second) to select it and open the screen's menu, as `⋯` does.
- **Scrolling views** (room and conversation messages, an entry and its
  replies) on boards with no keys (the Wio Tracker L2 and the CrowPanels): a
  tap does nothing, so scrolling never acts by accident. Hold an item still
  for **half a second** to open its menu. A tap on a Feed row opens it.
- **Status bar:** `‹` at the left goes back; holding it goes Home. `⋯` at the
  right opens the screen's menu (the palette in a room, Post/Discard in the
  Composer).
  On boards without a keyboard the status bar is 1.5 times a row's height,
  in a bigger font, so it's easier to tap.
- **Edge swipes:** a swipe towards the centre from the left eighth of the
  screen goes back. One from the right eighth brings up options: in a room it
  slides in the who's here list (or the room menu when the list is already
  showing), elsewhere the screen's menu. Edge swipes may be slower than other
  swipes (up to 0.8 s). Tapping
  outside an open menu or confirm closes it. A tap on a banner dismisses it.
- **Scroll:** drag a list or log to move line by line; swipe up or down to
  move a page. Dragging past the end of a list stops there.
- **Home** shows seven large tiles: **unread** across the top, then messages,
  rooms, c-mail, feed, guilds and settings two to a row. The unread tile shows
  how many things are unread (see [Unread](#unread)), in the accent colour
  while there are any. The messages tile shows the last room, or how many
  mentions are unread (in the accent colour) while there are any. If the tiles
  don't fit (a small screen at a large text size), Home is the list the
  keyed boards use. They fill about 90% of the space below the status bar, centred
  and spaced apart, with their labels in a larger font. The selected tile has
  a highlighted outline.

**Keyboard.** Tapping a text field docks a keyboard at the bottom; the screen
above it gets shorter. Picking Reply in a chat message's, feed entry's or
reply's menu, or Mention in a person's, docks it too.

- `⇧` shifts the next letter; tap it twice quickly to lock caps, and once
  more to unlock.
- `?123` shows digits and common symbols, `#+=` the rest, and `ABC` returns to
  letters. The emoji key beside `?123` opens the emoji tray in the
  keyboard's place; the keyboard returns when the tray closes.
- `⌫` deletes, and enter sends or moves on, as Enter does on a keyboard. In the
  Composer Body enter starts a new line.
- `▼` hides the keyboard; tap the field to bring it back. It also goes away
  when you leave the screen or the screen turns off.

**Emoji.** On touch screens (Wio Tracker L2, T-Deck Plus, CrowPanels) the
message input in rooms, C-Mail conversations and Feed replies has an emoji
button to the left of the prompt. Tap it for the emoji tray, tap an emoji to
insert it (the tray stays open), and tap outside the tray to close it.

**Buttons.**

| Button | Tap | Hold |
|---|---|---|
| BOOT | Back | Home (0.6 s) |
| Wake | Screen off; with the screen off, shows the last few messages for 10 s | Screen off; with the screen off, wakes it (1 s) |

**Orientation.** **Settings** → **Orientation** (or Display in web config)
turns the screen: Landscape, Landscape 180, Portrait or Portrait 180. It
applies at once and is kept across restarts and in backups. In portrait, long
lines on the Settings and Setup screens wrap.

## T-Deck Plus

The keyboard types, the trackball moves, and the touch screen works as on the
Wio Tracker L2 (taps, `‹`, `⋯`, the edge swipe and the Home tiles), except
that tapping a text field doesn't bring up the on-screen keyboard: you type
on the real one.

| Input | Action |
|---|---|
| Keys | Type. The keyboard handles Shift, Alt and Sym itself |
| Enter, or a trackball click | Select (send, in a room) |
| Backspace | Deletes while there's text to delete; otherwise Back. Holding it to clear a line stops at the empty line instead of leaving the screen |
| Trackball roll | Up, Down, Left, Right (moves between Home tiles too) |
| Hold the trackball click 2 s | Screen off |
| `⋯`, or a long-press | The screen's menu or the command palette (there's no Sym key) |
| Mic key (left of Space), while typing | The emoji tray |

**Screen off:** any key, trackball roll or touch wakes the screen. The press
that wakes it does nothing else, so it never types, selects or taps.

The screen stays landscape (there is no Orientation item). The setup AP is
**`Monster-Cyberspace-TDeck-Setup`**.

## T-LoRa Pager

The Pager has a keyboard and a scroll wheel on its side, and no touch screen,
so it works like the M9 with a keyboard: Home is a menu, and lists move with
the wheel.

| Input | Action |
|---|---|
| Keys | Type. Shift and Sym apply to the next key (press twice to cancel; they lapse after 1.5 s). While typing, Sym also opens the symbol table: turn the wheel to step through it and click or Enter to insert, or type a key to get its Sym character. Sym again switches to the emoji tray. Backspace closes either |
| Wheel turn | Up and Down |
| Wheel click, or Enter | Select (send, in a room) |
| Backspace | Deletes while there's text to delete; otherwise Back. Hold it to repeat |
| Sym + Backspace, or hold BOOT | Home |
| Sym + Enter | The command palette (while typing: Sym, then **commands…**) |
| BOOT tap | Back |
| Hold the wheel click 2 s | Screen off |

**Screen off:** any key, wheel turn or click wakes the screen. The input that
wakes it does nothing else.

The screen stays landscape (there is no Orientation item). The Pager has an
RTC, a GPS that powers off fully once the time is set, a microSD slot for
backups and a speaker for beeps. The setup AP is
**`Monster-Cyberspace-Pager-Setup`**.

## T-Deck Pro

The T-Deck Pro has a keyboard, a touch screen and a side button. Its screen is
e-paper, so the firmware keeps it calm: one black-on-white theme (no Theme
item, on the device or in web config), selections drawn as outlines rather
than filled bars, no blinking cursor and no moving screensaver. The screen
updates a moment after you stop typing, and redraws fully now and then to
clear ghosting.

Navigation works as on the touch-only boards: the room list and Settings are
buttons, the status bar has `‹` and `⋯`, log rows act on a hold rather
than a tap, and a tap on a Feed row opens it. A message's, entry's or
reply's menu (Reply, Report…) and its confirms are buttons, two a row at
the bottom left, drawn over the conversation. Screens with a list (a room or conversation, C-Mail, the
Feed and an entry) also show a narrow strip of three buttons on the right: ▲ and ▼
move up and down, and ● between them acts as Enter on the selected item, so
on a message in a room it opens the reply/report menu. Nicks in the room
log and replies are drawn bold, since the panel has no colours.

| Input | Action |
|---|---|
| Keys | Type. Shift and Sym apply to the next key (press twice to cancel; they lapse after 1.5 s). While typing, Sym also opens the symbol table; Sym again switches to the emoji tray. Backspace closes either |
| Mic key, while typing | The emoji tray (Sym + Mic still types `0`) |
| ▲ ● ▼ strip | Up, Enter, Down |
| Alt + W / A / S / D | Up, Left, Down, Right (hold to repeat) |
| Enter | Select (send, in a room) |
| Backspace | Deletes while there's text to delete; otherwise Back. Hold it to repeat |
| Sym + Backspace, or Alt + Backspace | Back |
| Sym + Enter, or Alt + Enter | The command palette |
| Sym + Space, or Alt + H | Home |
| Alt + C / M / F | cIRC, C-Mail, Feed |
| Touch | Tap and swipe as on the other touch boards |
| Side button | Screen off, and on again |

**Screen off:** the panel keeps showing the Monster for Cyberspace splash. Only
the side button wakes it.

The screen stays portrait (there is no Orientation item). The frontlight and
the keyboard light stay off. There is no RTC or speaker: the clock comes from
Wi-Fi or the GPS, which powers off once the time is set. Backups go to the
microSD slot. The setup AP is **`Monster-Cyberspace-TDeckPro`**.

## Mesh Deck

The Attaky Mesh Deck has a d-pad with Select, two shoulder buttons (L1, R2),
a touch screen and, with the keyboard module attached, a 48-key keyboard.
Without the keyboard module it works as a touch board, with the on-screen
keyboard; the module can be plugged in while it runs.

| Input | Action |
|---|---|
| D-pad | Up, Down, Left, Right (hold to repeat) |
| Select | Select; hold it to turn the screen off, and on again |
| L1 | Back; hold it for Home |
| R2 or BOOT | Screen off, and on again |
| Keys | Type. Shift, Sym and Alt work held, or tapped for the next key (they lapse after 1.5 s). Shift gives capitals and the symbols on the number row; Sym gives `? / - _ = + [ ] { } : ; ' " ` ~ \ | & < > * @ # % ^ ! ?` on q … m , . |
| Enter | Select (send, in a room) |
| Del | Deletes while there's text to delete; otherwise Back. Hold it to repeat |
| Tab | The command palette |
| Mic, while typing | The emoji tray |
| Sym, while typing | The symbol table: pick with the d-pad and Enter, or type a key for its Sym character. Sym again switches to the emoji tray |
| Alt (or Fn) + W / A / S / D | Up, Left, Down, Right (hold to repeat) |
| Alt + Del, Alt + Enter | Back, the command palette |
| Alt + H / C / M / F | Home, cIRC, C-Mail, Feed |
| Touch | Tap and swipe as on the other touch boards |

The power button is not used by the firmware: holding it cuts the power.

**Notification light:** the front LED and the keyboard's LEDs blink twice every
second while a mention or a C-Mail is unread, for the light timeout after the
last one came in. Settings has **Notify light** (the colour, or Off; Blue by
default) and **Light timeout** (15 s by default, up to Until read); picking a
colour blinks it once. Web config has both under Display.

There is no RTC, speaker or SD slot: the clock comes from Wi-Fi or the GPS,
and backups go through web config. The setup AP is
**`Monster-Cyberspace-MeshDeck`**. The serial console is the USB-C port
(a CH340). Right after flashing, if the screen stays dark, switch the
Mesh Deck off and on.

## Xteink X4

The Xteink X4 is an e-paper reader with no keyboard or touch: it shows cIRC,
the Feed, C-Mail, Guilds and Notifications, and replies with canned phrases.
Longer text is written in web config and sent to it.

| Button | Action |
|---|---|
| Up, Down (right edge) | Move the selection or scroll (hold to repeat) |
| Left, Right (below the screen) | Previous or next tab, room or page |
| Confirm | Open, select or send; hold it for the item's menu |
| Back | Back; hold it for Home |
| Power | Tap: screen off, and any button turns it on again. Hold 2 s: power off |

**Canned replies:** where another board would type, the X4 opens a list of
your phrases, then the 40 tray emoji. Confirm sends the one picked; Back
cancels. Up to 10 phrases of up to 80 characters each are edited in web
config (Canned replies) and kept in backups. Starting a C-Mail, a new thread
or adding a Wi-Fi network says to use web config instead.

**Sending from web config:** the page's Send box (at `http://<device IP>/`,
no password needed) shows what is open on the X4. Text sent there goes to the
open room or C-Mail as if typed, or starts a C-Mail to a user, or posts a new
thread in a guild. Each Send is one message; nothing is retried. With a page
password set, `/` shows only the Send box, and Settings (`/config`) asks for
the password. The Web config item in Settings, or the box in web config's
Security section, turns the page off altogether (the setup AP still serves it).

**Notifications** on Home lists unread cIRC mentions, then Feed and guild
notices, newest first; Confirm opens the room or entry.

**Power:** on USB it stays awake. On battery it sleeps after the idle time
(Settings → **Sleep after**, 5 min by default), with "Sleeping — updated
hh:mm" at the bottom of the screen, and wakes every **Wake every** minutes
(15 by default; Never: only Power wakes it) to rejoin Wi-Fi and redraw, then
sleeps again. Any button press during that makes it a full wake. Under 3.40 V
it shows "Battery empty" and powers off. Both settings are in web config too
(Power).

The panel refreshes at most every 2 s for new content, with a full refresh
from time to time to clear ghosting. Screenshots aren't available (there is
no full frame in memory). Backups go to the microSD slot. The setup AP is
**`Monster-Cyberspace-X4-Setup`**. The serial console is the USB-C port.

## CrowPanel

The CrowPanel Advance 3.5" and 5.0" work by touch alone, as described under
Touch above, but with no buttons:

- **Screen off:** there's no button to turn the screen off; the screen
  timeout (see Screen timeout) locks it. A double tap wakes it (those taps
  do nothing else), so brushing the panel doesn't.
- **Orientation:** pick it in Settings or web config, as on the Wio Tracker L2.
- **No cursor:** as on the Wio Tracker L2, lists and Home tiles draw no
  selection outline; a tap opens what's under the finger. A long press that
  opens a menu outlines the message or entry it's for, until the menu closes.
- **Rooms:** on the Wio Tracker L2 and the CrowPanels the room list is a
  grid of small buttons, three across on a screen at least 800 px wide (the
  5.0" in landscape) and two across otherwise, each with the room's name and
  how many are online (the name in the accent colour when unread). Tap one to
  open it; drag up or down to scroll.
- **Settings:** the actions are buttons in the same grid, each showing its
  current value at the right (Setup AP shows on or off; tap to switch), under
  the same status lines as other boards (account, Wi-Fi, battery, time),
  which scroll away with the buttons.
- **3.5":** the microSD slot takes settings backups; a GPS module on the UART1
  connector, if fitted, sets the clock.
- **5.0":** no SD slot (its pins belong to the wireless-module slot) and no GPS.

Neither has a battery gauge or an RTC; the clock is unset until NTP or GPS.
The setup AP is **`Monster-Cyberspace-CrowPanel35`** or
**`Monster-Cyberspace-CrowPanel50`** (no `-Setup`: Wi-Fi names stop at 32
characters).

## Room list

On boards with a keyboard (M9, T-Deck Plus, Pager), typing on the room list
filters it to rooms whose names contain what you typed, ignoring case. The
filter shows on the bottom line; Backspace erases a letter, Back clears it,
and it clears whenever you return to the list.

On touch boards the rooms are buttons. With keys too (T-Deck Plus), the
trackball or arrows move the selection through them and Centre opens one.

## In a room

The keyboard always types into the input line at the bottom.

- **Send:** Centre or Enter with text in the line. The text shows dimmed as
  *sending…* until the server echoes it back. If the send fails, the text
  returns to the line and a banner explains why.
- **Room menu:** Centre on an empty line. It has Who's here, Mute…, Unmute…,
  Mark read and Leave room.
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
  move between rooms. On the CrowPanels in landscape the list is always docked
  on the right; tap a name for their menu.
- **Commands:** type slash commands such as `/me`, `/dice` and `/mute name`,
  or pick them from the palette (on the M9, Sym).

Sending is disabled while offline; nothing is queued or retried for you.

## C-Mail

C-Mail is private, one-to-one messaging. Open it with Fn under Home, or
**C-Mail** on the Home screen.

- **The list** shows **New conversation…** first, then your conversations,
  unread first. Unread ones are bold with a count, followed by a preview of
  the last message. On touch-only boards each is a full-width button: the
  name (and unread count) on the first line, and the last message, cut to
  one line, under it.
- **New conversation…:** Centre, type a username (no `@`), then Centre. You
  land in that conversation, or a banner explains why not (for example, no
  such user). Back cancels.
- **A conversation** works like a room, titled `@name`: type and press Centre
  to send. Scrolling, the reader and links work the same way. There is no
  drawer. Its menu (Centre on an empty line) has Mark read and Close
  conversation. The message menu has Reveal, Show full and Show link;
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
  bookmarked it, one font size larger, then the title or the first line of
  the body, dimmed. While the command palette is open both lines are the
  same size. An
  entry with a content warning or the NSFW flag shows `CW: …` instead.
- Up/Down select. Down on the last entry loads the next page; once every page
  is loaded it wraps to the top, and Up on the first wraps to the last.
- Centre or Right opens the entry. Left or Back goes Home.
- **Sym** (on the M9) opens the palette: **New entry**, **Refresh**, **Watched
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
- **Sym** (on the M9) opens **Post…** and **Discard…**. Post checks the draft, then asks
  "Post entry? (2 per minute, 24 per day)". A posted entry opens at once; a
  failed one keeps the draft.
- Back (or Fn under Back) with any text asks "Discard draft?". The draft
  stays in memory until you post or discard it.

**Watched threads and Bookmarks** are lists in the same style, from the
palette. Centre opens the entry; a bookmarked reply opens its entry with that
reply selected. Left or Back returns to the Feed list.

## Guilds

Guilds are Cyberspace member groups, each with its own forum of threads. A
guild thread is an ordinary Feed entry. Open the list with the Map button
(far right on the M9) from anywhere, or **Guilds** on the Home screen. You
must be signed in.

**The list:**

- Each guild takes two lines: `🦉 Night Owls · 42+7` (members, then
  apprentices when there are any), plus your role (`founder`, `member` or
  `apprentice`) if you belong to it, then the bio, dimmed.
- Your own guilds come first under `── my guilds ──`; the rest follow under
  `── all guilds ──`, most populated first.
- Up/Down select. Down on the last guild loads the next page; once every page
  is loaded it wraps to the top. Centre or Right opens the guild. Left or Back
  goes Home.
- **Ctrl** (on the M9) opens the palette: **Refresh** and **Text size**.

**A guild:**

- The threads are listed like the Feed, most recently active first. Centre or
  Right opens a thread. Back from the thread returns to the guild.
- **Ctrl** opens the palette: **New thread**, **Info**, **Members**, **Join**
  (if you are not in it), **Make my guild** (apprentices only), **Leave**
  (members and apprentices; founders have none), **Refresh** and **Text size**.
- Join, Leave and Make my guild ask first, with the limit: "Join Night Owls?
  (3 per minute, 15 per day)". Leaving your own guild adds "Your badge will be
  cleared." Nothing is retried.
- Left, Back or Map returns to the guild list.
- If the guild is gone (also when a join, leave or Make my guild finds it
  gone), you get "Guild not found" and the list. If you hit a
  limit, you get "Slow down: try in Ns" and nothing is sent. Leave and Make my
  guild say "Manage this on the web" when the server refuses them.

**Info:** a read-only page with the founder, member counts, founding date,
your role, the bio and the link. Centre on it shows the link in full with its
QR code. Back closes it.

**Members:** `@name · role · 3mo` per row, paged like the list. Back returns
to the guild.

**New thread:** like New entry in the Feed, with Title, Body and Topics only.
Post asks "Post thread? (2 per minute, 24 per day)", then opens the new
thread. Back with text asks "Discard draft?" and returns to the guild; the
Feed key asks the same and goes to the Feed, Map to the guild list.

**Alerts:** a new thread in a guild you belong to shows a banner
(`@name in Night Owls: …`) and sounds the buzzer if it's on.

## People

A profile shows someone's name, guild, Cyberspace number, join date, when
they were last active, follower and entry counts, location, website and bio.
You must be signed in. Open one from:

- a room: the person menu (in Who's here) or a message's menu → **Profile**
- a conversation: the room menu → **Profile**
- an entry or a reply: its menu → **Author's profile**
- a guild's **Members**: Centre or Right on a member (tap on touch boards)

**On a profile:** Up/Down scroll. Centre, Right or **Ctrl** (⋯ on touch)
opens the actions:

- **Follow** / **Unfollow** ask first, with the limit: "Follow @rain? (3 per
  minute, 15 per day)". The profile then says `you follow them`.
- **Message** opens a C-Mail conversation with them.
- **Poke** asks first: "Poke @rain? (1 an hour, 8 a day)". A refused poke
  ("Can't poke @rain") doesn't count against the limit.
- **Entries** lists their public entries, paged like the Feed; Back returns
  to the profile.
- **Guild** opens their guild, **Website** shows the link with its QR code,
  **Refresh** reloads the profile.

Back returns to where you opened the profile. Nothing is retried; over a
limit you get "Slow down: try in Ns" and nothing is sent.

**Mute and block:** muting (`/mute name` in a room, or the person menu) hides
someone in cIRC. Blocking is done on the website only; the API has no block
call. A profile you blocked says `you blocked them`, and has no Follow,
Message or Poke. Whether you follow someone is only known once your follow
list has loaded (up to 500 follows).


Every board draws all of Terminus's characters (Latin with accents, Greek,
Cyrillic, arrows, maths, box drawing, card suits and the like) and every
emoji in Noto Emoji, one colour and two cells wide. Skin tones are dropped,
showing the plain emoji; a flag shows as its two letters; a combined emoji
(family, profession) shows its parts side by side. Anything else, such as
Chinese or Japanese, shows as `�`.

The emoji tray draws its 40 emoji large (four cells by two) wherever the
screen has room, and two cells wide where it doesn't.

## Spell check

In a room, a C-Mail, a Feed reply, and a new entry's title and body, ending a
word with a space or punctuation checks it against a 25,000-word English list.
If it isn't there but something close is, a strip opens above the input: the
word as you typed it (dim), then up to three suggestions, likeliest first.

- **Left / Right** (or Up / Down: the Pager's wheel, the T-Deck Pro's side
  buttons) swap the word in your text for the next or previous option as you
  go.
- **Centre / Enter** keeps the one shown. On the word as typed, it does what
  it always does (sends).
- **Tap** an option to take it.
- Typing anything else closes the strip and leaves the word alone.

Words in capitals (acronyms), with a capital inside (LongFast), digits or
non-English letters, and the tail of a link, @name or #room are never checked.
Turn it off in Settings → Spell check or web config. Nothing is ever changed
without a key press.

## Text sizes

Pick one in **Settings** → **Text size** (a box with all three), or switch
in the Feed palette, an entry's menu or the web page, where each switch steps
Compact → Comfortable → Large. The choice is saved. Settings itself always
uses Compact, so its items fit.

- **Comfortable:** larger 8×16 text.
- **Compact:** 6×12 text, with a hint row at the bottom.
- **Large:** like Comfortable, one size up: 10×18 text and 12×24 titles.

Room messages read `14:02 nick: message`, with a divider when the day
changes, at every size. A long message wraps under the nick at every size.

## Themes

Pick a theme on the device (**Home** → **Settings** → **Theme** opens a box
with every theme) or in web config under Display. The choice is saved.

- **Default:** the terminal look, with coloured nicks.
- **Black & white:** white text on black, in greys only.
- **White & black:** black text on white, in greys only.
- **cyberspace.online's themes:** Dark, Light, LCD, C64, VT320, Matrix,
  Poetry, Brutalist, GRiD, Crypt, Bubblegum and Top8, in the site's colours.
  The status bar takes the site's border colour and the input line and
  member list its code-block colour.
- **Bubblegum Dark, Top8 Dark:** our dark takes on those two, pink on plum
  and pale blue on navy.

Bubblegum and Top8 colour nicks like Default, in darker shades for their
light backgrounds, and their dark takes in lighter ones; yours is in the dim
colour. The other themes draw nicks
in the text colour, and yours in the dim one. QR codes stay
black on white in every theme. The T-Deck Pro's e-paper has one theme of its
own, black on white with outlined selections, and no Theme item.

## Unread

**Unread** lists everything waiting for you in one place, newest first:
unread C-Mail conversations, cIRC mentions of you, and Feed and guild alerts.
Open it from Home on every board: it is the second item in the Home list,
after Messages (`Unread  N`, marked while N is above 0), and the full-width
tile at the top of the Home tiles on touch boards. The status bar shows the
total; with nothing waiting it says "nothing unread".

- Each row starts with what it is: `mail` (`@name: last message`, with the
  conversation's unread count on the right), `irc` (`@name in #room: …`) or
  `feed` (`@name replied: …`, or a new guild thread).
- Up/Down pick, Centre or Right opens (tap on touch boards); Left or Back
  goes Home.
- Opening a row opens the conversation, the room (which becomes your last
  room) or the entry, and marks it read the way opening it anywhere else
  does. Back from there returns to Unread, so you can work down the list.
- **✓ Mark all read** is the top row while anything is unread. It asks
  first, then marks every notification read on your account (mentions, Feed
  and guild alerts, and any the device doesn't list) and each unread C-Mail
  conversation, as the website would. Nothing is retried; if some C-Mail
  stays unread you get "Some C-Mail stayed unread".
- Nothing is fetched for this screen: it shows what the device already knows
  from the checks described under [Alerts](#alerts).

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

At `http://monster-cyberspace.local`, or the device IP, on your LAN:

- Status
- Wi-Fi: scan, add, and remove saved networks (up to 5)
- Cyberspace account: sign in, sign out, resend verification
- Display: text size, theme, buzzer, spell check, orientation (touch boards),
  and keyboard light (boards with a lit keyboard)
- Time: zone and NTP server (see Time and time zone below)
- Security: page password and setup-AP password
- Restart and factory reset

### Security notes

The page is plain HTTP on your LAN, so set a page password on shared networks
(user `admin`). The setup AP is open unless a setup-AP password is set, so
anyone in radio range can join it while it runs. It runs with no saved
Wi-Fi (first boot or after a factory reset), when you turn it on in Settings,
or after the saved networks fail for 5 minutes, and anything entered on it
(Wi-Fi password, Cyberspace password) crosses that link unencrypted. Setting a
setup-AP password closes that gap for later setups. A backup download (see
Backup and restore) contains the refresh token and every password, so anyone
who can open the page can take them: set a page password.

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
  and the T-Deck Plus have no RTC, so their clocks are unset until NTP or GPS.

The GPS powers itself on only while it's needed: when the clock is unset or
its last sync is over 6 hours old. It turns off once the clock is set. A
search with no fix gives up after 15 minutes and tries again an hour later.
On the T-Deck Plus "off" only stops listening: the receiver shares the main
power rail and stays powered (about 20 mA).
For a first fix, take the device outdoors with a view of the sky.

You set the time zone yourself; it never changes on its own. Pick it in
any of three places:

- the setup page's **Wi-Fi** section, during first setup;
- web config under **Time**: pick a **Zone**, or **Custom** to type a POSIX
  TZ string (1–63 characters);
- **Home** → **Settings** → **Time zone: …**, which opens the list of zones.

The zone applies at once. **NTP server** takes a host name (1–63
characters). **Save time** saves the zone and the server.

**Home** → **Settings** and the web Status page show three lines:

- **gps:** `none` (no GPS on this board), `off`, `searching m:ss`, or
  `fix, N of M satellites`.
- **time:** the source and age of the last sync, such as `NTP, just now` or
  `GPS, 12 min ago`; `RTC, at boot`; or `not set`.
- **zone:** the zone name, or the POSIX string with `(custom)`.

Privacy: your position is never sent anywhere, never stored and never logged.
The serial log says only whether there's a fix and satellite counts.

## Screen timeout

After the screen timeout (Settings or web config, 30 s by default) with no
key press or touch, the backlight dims for the last 10 seconds and then the
screen locks: either the **screensaver**, the monster bouncing slowly round a
dim screen, or **screen off**, as set by **At timeout**. Any input during the
dim wakes it back to full. Incoming messages neither wake the screen nor
restart the timeout.

For 3 s after the screen locks every input is ignored. After that the screen
wakes just as it does from screen off on that board (hold centre on the M9,
Wake hold on the Wio Tracker L2, any input on the T-Deck Plus and the Pager,
a double tap on the CrowPanels), and the waking input does nothing else. A peek (a centre or
Wake tap) shows the last few messages for 10 s and then returns to the
screensaver. **Never** keeps the screen on.

## Settings

**Home** → **Settings** shows the account, the Wi-Fi network and IP, the web
config address, the battery, the setup AP state and the gps, time and zone
lines (see Time and time zone), with ten actions, plus Orientation on a
touch board and two more on a board with an SD card slot:

On a touch board the actions are a grid of buttons, each with its current
value at the right. On the T-Deck Plus and the Mesh Deck the arrows move a
highlighted outline between them and Select opens one; Left from the first
column, or Back, returns Home. Dragging or tapping hides the outline (on the
Home tiles too); the next arrow or Select brings it back on the first button
shown without moving or opening anything.

- **Theme: …** Opens a box with every theme (see Themes); picking one
  applies and saves it.
- **Text size: …** Opens a box with the three sizes (Compact, Comfortable,
  Large); picking one applies and saves it.
- **Time zone: …** Opens the zone list; Centre picks one and saves it, Back
  closes the list unchanged.
- **Orientation: …** (touch boards) Opens a box with the four orientations; picking one
  turns the screen at once and saves it.
- **Screen timeout: …** Opens a box with 15 s, 30 s, 1, 2, 5, 10, 30 and
  60 min, and Never; picking one saves it. 30 s by default. See Screen timeout.
- **At timeout: screensaver / screen off.** Centre switches what the screen
  timeout does and saves it.
- **Keyboard light: …** (M9, T-Deck Plus, T-Deck Pro, Pager) Opens a box with
  Low, Medium, High and Off; picking one saves it (also under Display in web
  config). The keys are lit only while
  the screen is on. Off by default, except on the M9, where it is High and its
  keyboard lights the keys itself after a press; Off there switches that light
  off until the M9 is power cycled.
- **Notify light: …** and **Light timeout: …** (Mesh Deck) Open a box with
  the colours (or Off) and the timeouts; see Mesh Deck.
- **Spell check: on / off.** Centre switches suggestions while typing (see
  Spell check). On by default.
- **Wi-Fi: …** Shows the joined network and opens Wi-Fi on the device (see
  above).
- **Turn setup AP on / off.** Brings up the setup AP (its name, password state
  and `http://192.168.4.1` are shown) while staying on your Wi-Fi, so you can
  reach web config when your network is unreachable. It stays on until you
  turn it off or restart.
- **Firmware: …** Shows the running version. Select it to check for an update;
  when one is found it reads `vX.Y.Z available`, and selecting it again asks
  **Install vX.Y.Z? The device restarts.** See Firmware updates.
- **Auto-update: on / off.** When on, an update found by the daily check
  installs and restarts by itself. Off by default.
- **Back up to SD card** and **Restore from SD card…** (boards with a card
  slot). See Backup and restore.
- **Log out** (confirm with **Log out**). Signs out of Cyberspace and goes to
  the Setup screen. Wi-Fi and preferences are kept.
- **Factory reset…** (confirm with **Erase everything**). See below.

If the saved networks fail for 5 minutes, the setup AP comes up by itself.
It goes away again once Wi-Fi is back and nobody is joined to it.

## Firmware updates

The device looks for a new release 20 seconds after joining Wi-Fi and then
once a day, straight from this project's GitHub releases over HTTPS. It only
asks GitHub which release is newest; nothing else is sent. When one is
found, **Settings → Firmware** says so. Installing downloads the image,
checks its signature, and restarts into it (`installing N%` shows progress).
Your settings are kept. If the new firmware fails to start, the device goes
back to the previous one by itself.

Web config's **Firmware** section does the same: **Check for updates**,
**Install**, and **Install updates automatically**.

A board installed with an image from before over-the-air updates arrived
needs one USB install of a release first (see the README); until then
Firmware says updates need it.

## Backup and restore

A backup is one YAML file holding every saved setting: Wi-Fi networks and
their passwords, the time zone and NTP server, the page and setup-AP
passwords, theme, text size and other preferences, and your Cyberspace email
and sign-in token. Restoring it on a reset or new device brings it back
signed in.

**The file holds your passwords and a sign-in token.** Anyone who has it can
use your Cyberspace account, so keep it private.

- **On the device** (boards with an SD card slot): **Home** → **Settings** →
  **Back up to SD card** writes `rc-backup-YYYYMMDD-HHMMSS.yaml` (or
  `rc-backup-N.yaml` before the clock is set) to `/monster-cyberspace/backups/`
  on the card. **Restore from SD card…** lists the backups newest first;
  pick one and confirm **Restore**. Backups made under the earlier names sit in
  `/platy-cyberspace/backups/` or `/rhino-cyberspace/backups/`; move them
  across to restore them.
- **During setup**, when the card holds backups, the Setup screen offers
  **Restore from SD card** below **Log in on this device**.
- **In web config**, the **Backup** section has **Download backup**,
  **Restore from file…** (an uploaded `.yaml`, at most 16 KB), and, on boards
  with a slot, **Save backup to SD card** plus a list of the card's backups,
  each with restore, download and delete.

You can edit a backup in a text editor. Keys missing from a file keep the
device's current values, and a file with any error is refused as a whole
with the line number. A restore saves the settings and restarts the device.

The card can be pulled out at any time between operations; with no card,
each action says **No SD card**.

## Back to setup mode

A **factory reset** erases saved Wi-Fi, the account token, preferences and
both passwords, then restarts into setup mode. You can do it from either
place:

- **Home** → **Settings** → **Factory reset…** → select **Erase everything**.
- Web config → **Factory reset…** (you confirm twice).
