### Monster for Cyberspace 1.2.3

### Xteink X4

- The Xteink X4 is out of releases and the flasher for now. It still needs work and will come back in a later release.

### Monster for Cyberspace 1.2.2

### Following

- **Following** on Home lists Feed entries by the people you follow on cyberspace.online, newest first. Press Down at the end of the list (touch: ⋯, Load older) to look further back.
- The device-only friends list (Add friend / Remove friend in a message's menu) is gone. Following uses the people you follow on the website instead.

### Alert sound

- On boards with a speaker, alert sound has a volume slider in device Settings (10% steps) and in web config. Turn it all the way down to switch alert sound off. Boards without a speaker hide the setting.

### Other changes

- New installs open **#cyberspace** first. There is no #general.

### Monster for Cyberspace 1.2.1

### T-Display P4: function keys

- With the keyboard on, the function keys work from any screen, including while typing: **F1** Home, **F2** your last room (press it again in the room for the room list, and again to go back), **F3** C-Mail, **F4** Feed, **F5** Guilds, **F6** your profile, **F7** Settings.
- **F10** shows navigational help, a table of these keys; any key or tap closes it. **F11** still steps the keyboard light.
- Settings pickers (Theme and the rest) show a cursor you can move with the arrows. Enter sets your choice and Esc cancels.

### Monster for Cyberspace 1.2.0

### New board: LilyGO T-Display P4

- First release for the LilyGO T-Display P4 with the 4.1" AMOLED. It works in portrait, in landscape, and in landscape with the clip-on keyboard. The UI stays clear of the screen's rounded corners.
- Wi-Fi goes through the on-board ESP32-C6. It also has the speaker, GPS, clock, battery gauge and microSD. Flash it from the flasher page; later updates arrive over the air.
- **Vibration:** tap and alert vibration, each switched on or off in Settings and web config.

### Other changes

- **Touch boards:** Unread's rows, Settings pickers, the time zone list, confirms and Restore from SD are now big buttons. A new entry's Title has more room around it, so it's easier to tap.
- **Who's here:** your own name in the right-hand panel now has the same colour as in the chat (every board except the T-Deck Pro).
- **T-Deck Pro:** text starts a little further from the left edge, so the bezel no longer covers it.
- **Feed:** Topics (read-only) in the Feed's commands. **Profile:** Bookmarks on your own profile.

### Monster for Cyberspace 1.0.1

### Cardputer ADV: Unread

- The Cardputer ADV now has **Unread**: the second row on Home (`Unread  N`) opens your unread C-Mail, mentions and Feed notices, newest first, with Mark all read.
- On every board, Unread's count no longer copies your whole inbox on each screen redraw. That matters most on boards without PSRAM.

### Also in this release

A new board, the Attaky Cyber Deck Plus, contributed by **@WRLS6H05T**, and your own profile one tap from Home.

### New board: Attaky Cyber Deck Plus

Thanks to **@WRLS6H05T** for porting Monster to the Cyber Deck Plus and testing it on their own deck (#6).

- First release for the Attaky Cyber Deck Plus: the Mesh Deck's Core (d-pad, touch screen, keyboard module, notification light) plus a gamepad, the Media I/O speaker and a microSD slot. It has no GPS or LoRa module.
- Gamepad: the D-pad moves; A is Select (hold it to turn the screen off and on); B and L1 are Back (hold them for Home). R2 on the Core wakes the screen.
- The speaker beeps for mentions and C-Mail when the buzzer is on. Backups go to the microSD card.
- Flash it from the flasher page; later updates arrive over the air.

### My profile

- **Profile on Home:** a new tile (touch boards) or row (the others) opens your own profile. Its **Entries** list shows the entries you've posted, so you can go back to them.
- **Profile links:** under the bio, every profile now has **Entries**, **Guild**, **Website** and **Refresh**, as buttons on touch boards and a list on the others. The actions menu keeps only Follow, Message and Poke.
- Home's tiles are now a 4×2 grid; Unread is the first tile.

### Other changes

- **Feed:** the first row is **+ New entry**, so a d-pad and Select alone can write a post. On the X4 it points you to web config.
- **No more false "offline":** with no chat room open, the Feed, entries, the composer and C-Mail no longer say "offline" on a working connection. When signed out, the status bar shows ✕.
- **Entries:** Tab opens the entry menu, also while there's text in the reply line.
- **Friends:** a message's menu has Add friend / Remove friend. The list stays on this device.
- **Mesh Deck:** shares its Core code with the Cyber Deck Plus. Nothing should look different; please report anything that does.
