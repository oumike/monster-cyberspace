# Fluffy Generator — design

Fluffy is the text-art monster on the boot splash and the screensaver
(`lib/core/src/ui/splash_layout.cpp`), with a speech bubble of short phrases.
This lets a user swap in their own mascot art and phrases: at install time on
the browser flasher, or later in the device's web config. No firmware rebuild
per mascot.

## 1. Goals

- A **Make your Fluffy** button on the flasher opens a modal: draw art or pick
  a preset (Fluffy, cat, dog, rhino, platypus).
- A **Phrases** button opens a modal with up to 10 bubble phrases, prefilled
  with today's six; the defaults count against the 10.
- The result is flashed with the firmware and shown instead of Fluffy and his
  words. Web config can change it later.
- Every board. Art only for now: no mascot name.

## 2. Limits

- Art: 1 to 6 lines, each at most 13 characters (Fluffy's size), printable
  ASCII (0x20–0x7E). Trailing spaces are trimmed; leading spaces kept.
- Phrases: 1 to 10, each 1 to 80 printable ASCII characters.
- Art and phrases are independent: either may be absent, and an absent one
  falls back to the default.

## 3. Storage: the `mascot` partition

`partitions_16mb.csv` gains `mascot, data, 0x40, 0xcb0000, 0x1000,` (4 KB, in
the free gap after `nvs`). Every firmware env uses this table.

- The firmware reads the address with `esp_partition_find_first(DATA, 0x40,
  "mascot")`, falling back to a raw `esp_flash_read` at 0xcb0000 so a device
  whose table predates the partition (OTA keeps the old table) still works.
- Erased flash, a bad magic, a bad CRC or a bad field is "no mascot": Fluffy
  and the default words, no error shown (one `[mascot]` log line).
- OTA never writes it; factory reset clears NVS only, so the mascot stays.
  Not part of backup files.

### Blob format (little-endian)

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic `FLUF` |
| 4 | 1 | version, 1 |
| 5 | 1 | art line count, 0–6 (0: default art) |
| 6 | 1 | phrase count, 0–10 (0: default phrases) |
| 7 | 1 | reserved, 0 |
| 8 | 2 | payload length N |
| 10 | N | payload: each art line, then each phrase, as length byte + bytes |
| 10+N | 4 | CRC32 (IEEE, as zlib's `crc32`) of bytes 0 … 10+N−1 |

The rest of the 4 KB is 0xFF. A decoder rejects any line or phrase over its
limit, any byte outside 0x20–0x7E, or a payload that doesn't end exactly at N.

## 4. Firmware

- `lib/core/src/mascot/mascot.{h,cpp}` (portable, host-tested):
  `struct Mascot { std::vector<std::string> art, phrases; }`,
  `std::vector<uint8_t> encodeMascot(const Mascot&)` (4096 bytes, 0xFF-padded),
  `bool decodeMascot(const uint8_t*, size_t, Mascot&)`,
  `std::string validateMascot(const Mascot&)` (empty when valid, else the
  reason, for web config), and `defaultMascotArt()` / `defaultMascotPhrases()`
  (Fluffy and today's six words, moved out of `splash_layout.cpp`).
- `splashLayout` takes the art lines as a parameter; `MonsterWords` takes the
  phrase list (up to 10). Callers pass the current mascot.
- `src/store/mascot_store.{h,cpp}`: `load()` at boot into app state; `save()`
  erases and writes the 4 KB sector, then updates app state so the next
  splash or screensaver shows it (no restart).
- Web server: `GET /api/mascot` (art and phrases as JSON, plus `custom`
  flags) and `POST /api/mascot` (validate, save; empty art or phrases reset
  that part), behind the page password like the other settings.

## 5. Website (`site/`)

- `index.html` gets two buttons by the Install button and loads
  `fluffy.js` / `fluffy.css`; everything else lives in those files.
  `scripts/pages-site.sh` copies them.
- **Make your Fluffy** modal: a 13×6 monospace editor that caps width and
  height and drops non-ASCII as you type; a live preview with a sample bubble;
  preset buttons; Reset to Fluffy; Done.
- **Phrases** modal: ten rows, the six defaults then four blank, each with a
  0/80 counter; Reset to defaults; Done.
- Choices persist in `localStorage`. When either differs from the default the
  page encodes the blob, and the Install button gets a blob-URL manifest with
  two parts: the firmware image (absolute URL) at 0x0 and the blob at
  0xcb0000. Otherwise the normal manifest. A line under Install says what
  will be flashed ("with your mascot and 8 phrases").

## 6. Web config

A **Mascot** section on the device page: the same editor, presets and phrase
rows (compact), Save and Reset to Fluffy. Presets live in one file,
`site/mascots.json`; a generator (`tools/gen_mascot_presets.py`, a `pre:`
script like the spell dictionary) emits `src/web/mascot_presets.h` from it, so
the site and the firmware can't drift.

## 7. Presets (13×6 at most)

```
Fluffy          cat             dog
 /\       /\      /\_____/\       __     __
/  '-----'  \    /  o   o  \     /  \___/  \
|  (o) (o)  |   ( ==  ^  == )    \_/ o o \_/
|   .VVV.   |    )         (       |  _  |
 \  '^^^'  /    (           )      \ (_) /
  '-.___.-'      \_)_____(_/        '---'

rhino           platypus
    /\              ______
 __/  \_____     __/ o    \__
<_  o       \   <__)       _>
  \__       |      \______/
     |_|--|_|       ^^  ^^
```

## 8. Testing

- Native `test/test_mascot`: round trip; bad magic, CRC and version; a line
  of 14 characters; 7 art lines; 11 phrases; a phrase of 81; a byte 0x7F and
  a UTF-8 byte; art-only and phrases-only blobs fall back for the other part;
  erased flash (all 0xFF) is "no mascot"; `splashLayout` with custom art;
  `MonsterWords` with 1 and 10 phrases.
- A golden blob (hex, in `test/fixtures/mascot_golden.hex`) is checked by the
  native test and by `node --test site/` against `fluffy.js`'s encoder; CI
  runs both.
- Hardware (a human, on a T-Deck Plus): flash from the site with the cat and
  three custom phrases, check the splash and screensaver; change it in web
  config, check without a restart; factory reset keeps it; an OTA update
  keeps it.

## 9. Docs

USE.md (a Mascot section), BUILD.md (the partition, the flasher's second
part), TESTING.md (the hardware checklist), AGENTS.md (pointer to this spec).
