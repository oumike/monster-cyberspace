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
| Fn under Back | Feed (coming soon) |
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

## Text sizes

Switch from the palette, the room menu or the web page. The choice is saved.

- **Comfortable:** larger 8×16 text, `nick message`, with a time divider when
  the minute changes.
- **Compact:** 6×12 text, `14:02 nick: message`, with a hint row at the
  bottom.

## Alerts

While you're signed in, the device checks every minute, even with the screen
off. Rooms with new messages are marked in the room list. A new mention of
you shows a banner and sounds the buzzer, if the buzzer is turned on in web
config. Mentions are marked read when you open that room and scroll to the
bottom.

A new C-Mail does the same: a banner (`C-Mail from @name: …`) and the buzzer,
unless that conversation is open. Home shows the total unread C-Mail.

## Web config

At `http://rhino-cyberspace.local`, or the device IP, on your LAN:

- Status
- Wi-Fi: scan, add, and remove saved networks (up to 5)
- Cyberspace account: sign in, sign out, resend verification
- Display: text size, buzzer, time zone
- Security: page password and setup-AP password
- Restart and factory reset

### Security notes

The page is plain HTTP on your LAN, so set a page password on shared networks
(user `admin`). The setup AP is open unless a setup-AP password is set, so
anyone in radio range can join it while it runs. It runs with no saved
Wi-Fi (first boot or after a factory reset), when you turn it on in Settings,
or after the saved networks fail for 5 minutes, and anything entered on it
(Wi-Fi password, Cyberspace password) crosses that link unencrypted. Setting a
setup-AP password closes that gap for later setups. The refresh token can't be
read back through the page.

## Settings

**Home** → **Settings** shows the account, the Wi-Fi network and IP, the web
config address, the battery and the setup AP state, with three actions:

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
