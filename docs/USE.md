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
| Fn under Home | C-Mail (coming soon) |
| Fn under Back | Feed (coming soon) |
| Map | Unassigned |
| Back | Up one level, or closes a menu or drawer. It never deletes text |
| Ctrl | In a room: the command palette (slash commands and text size) |
| Hold Back/Del | Clear the input line |
| Hold d-pad centre | Screen off; with the screen off, screen on |

**Screen off:** only holding the d-pad centre wakes the screen. A centre tap
while it's dark shows the last few messages for 10 s, then goes dark again.
Every other key is ignored while the screen is off, so pocket presses can't
send anything.

**Lists:** Up/Down select, Centre or Right open, Left goes back.

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
anyone in radio range can join it while it runs. It runs only with no saved
Wi-Fi (first boot or after a factory reset), and anything entered on it
(Wi-Fi password, Cyberspace password) crosses that link unencrypted. Setting a
setup-AP password closes that gap for later setups. The refresh token can't be
read back through the page.

## Back to setup mode

A **factory reset** erases saved Wi-Fi, the account token, preferences and
both passwords, then restarts into setup mode. You can do it from either
place:

- **Home** screen → **Factory reset…** → select **Erase everything**.
- Web config → **Factory reset…** (you confirm twice).
