### New board: M5Stack Cardputer ADV

- First release for the Cardputer ADV (ESP32-S3, 8 MB flash, no PSRAM): cIRC rooms, C-Mail, the Feed and profiles (follow, unfollow, poke), on its 240×135 screen in the Compact font.
- Left out to fit without PSRAM: Guilds, Unread, spell check, the emoji tray (a wide emoji shows as `?`), over-the-air updates and a custom mascot. Update it by flashing a new release from the flasher page.
- Keyboard: Fn + `;` `.` `,` `/` are the arrows, and the same keys work without Fn whenever you aren't typing. In a text field, Esc (`` ` ``) leaves the field so the arrows work; elsewhere it is Back. Backspace is Back when there's nothing to delete; Fn + Backspace is Home.
- Settings backup to microSD works, also with an M5Stack LoRa cap fitted.

### Other changes

- Features can now be compiled out per board; Home, Settings, web config and backup restore leave out what a board doesn't have.
- The splash screen fits small screens.
