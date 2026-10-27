# Rhino Cyberspace — settings backup and restore

Status: approved design (2026-10-26). Built after Feed (M3) Tasks 10–12.

## 1. Goal

Save every device setting to a YAML file, and restore it later on this or
another Rhino Cyberspace device. A backup can be kept on the board's SD card
or downloaded through web config. A restored device rejoins Wi-Fi and is
still signed in to Cyberspace, with no setup needed.

This is modelled on platy-aprs's backups, with two differences: the format is
YAML, and passwords are always included.

## 2. Decisions (user)

- **The backup always includes secrets:**
  - Wi-Fi passwords;
  - the web-config page password;
  - the setup-AP password;
  - the Cyberspace refresh token, so a restore stays signed in.

  The Cyberspace password is never stored, so it can't be backed up.
- **On the device:**
  - Settings → **Back up to SD card**;
  - Settings → **Restore from SD card…**;
  - the Setup screen offers **Restore from SD card** when the card holds
    backups.
- **In web config:**
  - download a backup;
  - restore by uploading a file;
  - save a backup to the SD card;
  - list the SD backups, each with restore, download and delete.
- **Order:** this is built after Feed Tasks 10–12.
- **Boards (2026-10-26):** every hardware build. The M9 and the T-Deck Plus
  read the card over their shared SPI bus; the Wio Tracker L2 uses SDMMC in
  1-bit mode. The T-Deck gets its SD code with the rest of its HAL.
- **Removable card (2026-10-26):** a card can be pulled or swapped at any
  time, so nothing stays mounted between operations. Each backup operation
  mounts the card, does its work and unmounts it.

### Rule change

`AGENTS.md` currently says "Never log or store passwords. Never log tokens."
It gains an exception: backup files deliberately contain the Wi-Fi, page and
setup-AP passwords and the refresh token. They are still never logged.

## 3. File format

A backup is UTF-8 YAML, and the device writes it in this form:

```yaml
# Rhino Cyberspace settings backup.
# Contains Wi-Fi and device passwords and a Cyberspace sign-in token.
# Anyone with this file can use your account: keep it private.
format: rhino-cyberspace-backup
version: 1
board: "M9"
firmware: "0.3.0"
created: "2026-10-26T18:30:00Z"
wifi:
  - ssid: "Home"
    password: "hunter22"
  - ssid: "Phone"
    password: ""
timezone: "EST5EDT,M3.2.0,M11.1.0"
tzName: "America/New_York"
ntpServer: "pool.ntp.org"
pagePassword: ""
apPassword: ""
textSize: compact
theme: default        # default | bw | wb
drawerOpen: false
buzzer: true
lastRoom: "general"
account:
  email: "me@example.com"
  refreshToken: "AMf-…"
```

### Writing

- Every string is double-quoted. `\`, `"` and control characters are escaped
  as `\\`, `\"`, `\n`, `\t` and `\uXXXX`.
- `created` is `""` when the clock isn't set.
- `wifi: []` is written when no network is saved.

### Reading

The reader accepts this subset of YAML:

- `#` comments and blank lines;
- top-level `key: value` pairs;
- the `wifi` list, with items written as `- ssid: …` then indented
  `password: …`;
- the `account` map, indented;
- scalars that are double-quoted, single-quoted (`''` stands for a `'`) or
  plain (unquoted, with any trailing ` #` comment trimmed);
- `true`/`false` for booleans.

Anything else is an error that names the line, for example `line 12: expected
"key: value"`. Unknown keys are ignored, so a later version can add fields.

### Restore rules

- **Format and version:** `format` must be `rhino-cyberspace-backup` and
  `version` must be 1. Otherwise the error reads "Not a Rhino Cyberspace
  backup" or "Backup version N is newer than this firmware".
- **Missing keys** keep the device's current value. A backup that has
  `wifi:` replaces the whole saved list.
- **Validation matches web config:**
  - at most 5 networks; extra networks are dropped, and the result message
    says so;
  - a Wi-Fi password is empty or 8–63 characters;
  - an SSID is 1–32 bytes;
  - the setup-AP password passes `validApPassword`;
  - the page password is at most 63 printable characters;
  - the text size is `compact` or `comfortable`;
  - the NTP server passes `validNtpServer` (1–63 characters, each
    `[A-Za-z0-9.-]`).

  A field that fails validation fails the whole restore, and nothing is saved.
- **Size:** files over 16 KB are refused.
- A successful restore saves every setting, then restarts the device.

## 4. Components

| Unit | Where | Does |
|---|---|---|
| Settings model | `lib/core/src/config/settings_model.h` | `rc::Settings` and `rc::WifiNet` move here from `src/store/settings.h`, which includes this header. No behaviour change. |
| Backup codec | `lib/core/src/config/backup_yaml.{h,cpp}` | `std::string backupToYaml(const Settings&, const BackupMeta&)`; `bool backupFromYaml(std::string_view, Settings& inOut, std::string& err, std::string& note)`, which merges per §3; the YAML scalar quoting and unquoting helpers. Arduino-free and host-tested. |
| Backup names | the same file | `backupFileName(epochSec, existingNames)` gives `rc-backup-YYYYMMDD-HHMMSS.yaml`, or `rc-backup-N.yaml` without a clock, with no clash. `validBackupName(name)` accepts only `[A-Za-z0-9._-]`, ending in `.yaml`, with no leading dot, at most 64 bytes. |
| SD access | `src/hal/board.h` and each board's HAL | `bool board::hasSdSlot()`. `fs::FS* board::sdMount()` mounts the card and returns its file system, or `nullptr` with no slot or no card; `board::sdUnmount()` releases it. Nothing stays mounted between operations. M9: `SD` on the shared SPI bus, CS 48, 20 MHz. Wio L2: `SD_MMC` 1-bit on CLK 2 / CMD 3 / D0 1, with the card's power switched on through expander bit 14 before mounting and off after unmounting. T-Deck Plus: `SD` on the shared SPI bus, CS 39. |
| Backup store | `src/store/backup_store.{h,cpp}` | Under one mutex: `saveToSd`, `listSd` (newest first), `readSd`, `deleteSd`, and `applyAndRestart(yaml)`. Files live in `/rhino-cyberspace/backups/`. `applyAndRestart` parses into a copy of the settings, saves it with the existing savers plus a new `settings::saveAll`, then calls `restartSoon()`. |
| Settings screen | `src/screens/settings.cpp` | Adds two items, Back up to SD card and Restore from SD card…. The restore item opens a list overlay, then the confirm "Restore <name> and restart?" (Cancel / Restore). Results show in the banner, for example "Saved rc-backup-….yaml", "No SD card" or "Restore failed: …". |
| Setup screen | `src/screens/setup.cpp` | Lists the SD backups once on `enter`. If any exist, it adds **Restore from SD card**, which uses the same list and confirm. |
| Web | `src/web/web_server.cpp`, `page.h` | A **Backup** section with the routes below. The page notes that the file contains passwords and a sign-in token, and travels over plain HTTP unless you use the setup AP with a password. |

### Web routes

All of these use the page password like every other route.

| Route | Does |
|---|---|
| `GET /api/backup` | Sends the YAML as an attachment named `rc-backup-….yaml`. |
| `POST /api/restore` | The body is the YAML, at most 16 KB. Replies `{ok, message}`, then restarts. |
| `GET /api/backup/sd` | `{slot: bool, card: bool, files: [{name, size}]}`, newest first. |
| `POST /api/backup/sd` | Saves a backup to the card. Replies with the file name. |
| `POST /api/backup/sd/restore?name=` | Restores that file. |
| `GET /api/backup/sd/download?name=` | Sends that file. |
| `POST /api/backup/sd/delete?name=` | Deletes that file. |

Every `name` goes through `validBackupName` before any file call.

## 5. Concurrency

- **The SPI bus (M9, T-Deck).** The SD card shares SPI with the panel. Both
  use Arduino `SPI` transactions, and Arduino-ESP32 serialises those across
  tasks with the SPI bus lock. The Wio L2's card is on its own SDMMC pins,
  but its power switch goes through the expander's I2C lock.
- `board::sdMount()`, every file operation and `board::sdUnmount()` run
  under the backup-store mutex, as one unit per operation.
- **Large files.** A file over 16 KB is never read whole; it is refused from
  its size.
- **Who calls what.** The UI task calls the backup store from key presses,
  and the web task calls it from requests. Neither holds `lockedChat` or
  `lockedFeed` while doing so.

## 6. Error handling

- No slot (`board::hasSdSlot()` is false): the device items and the web SD
  section are hidden. The page learns this from `GET /api/backup/sd`, which
  returns `card: false` and `slot: false`.
- No card, or a failed mount: "No SD card".
- A card pulled during an operation: the read or write fails ("SD read
  failed" or "SD write failed"), the card is unmounted, and the next
  operation mounts afresh.
- A write or read fails: "SD write failed" or "SD read failed". A partly
  written file is removed.
- A parse or validation error: the first error with its line. Nothing is
  saved.
- Restore succeeded: "Restored. Restarting…"; the device restarts after
  about 1 s.

## 7. Testing

**Native (`test/test_backup_yaml`):**

- **Round trip:** every field, including quotes, a backslash, a newline,
  non-ASCII text and an emoji in an SSID or password.
- **Hand edits:** plain and single-quoted scalars, comments, CRLF line
  endings, a trailing comment, and keys in another order.
- **Merge:** missing keys keep their current values, and an empty `wifi: []`
  clears the list.
- **Rejected input:** the wrong format, a newer version, a bad indent, a bad
  Wi-Fi password length, a bad AP password, an unknown text size, and more
  than 5 networks (truncated, with a note).
- **Names:** `backupFileName` with and without a clock and on a clash;
  `validBackupName` rejects `../x.yaml`, `a/b.yaml`, `.yaml`, `x.yml` and a
  65-byte name.

**Hardware (`docs/TESTING.md`):**

- Settings → Back up to SD card writes the file.
- The file opens on a computer and holds everything.
- Restore from SD card → restart with the same settings.
- With the settings erased and the card holding backups, the Setup screen
  offers a restore, and the device comes back signed in.
- Web config: download, upload-restore, save to SD, then restore, download
  and delete from the SD list.
- With no card, every path says "No SD card".
- Pull the card after a backup and put it back (or swap in another): the
  next list, backup and restore work without a restart. Pull it during a
  backup: "SD write failed", and the device carries on.
- On every board with a slot: M9, Wio L2 and, once its HAL lands, T-Deck
  Plus.

## 8. Out of scope

- Encrypting the backup file.
- Automatic or scheduled backups. Every backup comes from a key press or a
  web action.
- Backing up chat, C-Mail or Feed content.
