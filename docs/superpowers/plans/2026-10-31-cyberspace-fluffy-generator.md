# Fluffy Generator Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let users replace Fluffy's art and bubble phrases, from the browser flasher (two modals) or the device's web config, stored in a 4 KB `mascot` flash partition.

**Architecture:** A portable codec (`lib/core/src/mascot/`) defines the blob; the firmware reads it at boot from the `mascot` partition and the splash and screensaver draw it. The site encodes the same blob in JS and flashes it as a second ESP Web Tools part at 0xcb0000; web config writes it through a new `/api/mascot`. One presets file feeds both.

**Tech Stack:** C++17 / Arduino-ESP32 (pioarduino), Unity native tests, ESP Web Tools 10, plain ES modules, Node's built-in test runner.

**Spec:** `docs/superpowers/specs/2026-10-31-cyberspace-fluffy-generator-design.md`

## Global Constraints

- Art: 1–6 lines, ≤ 13 chars each, printable ASCII 0x20–0x7E; trailing spaces trimmed, leading kept.
- Phrases: 1–10, each 1–80 printable ASCII chars.
- Blob: magic `FLUF`, version 1, layout per spec §3, CRC32 = zlib `crc32` over bytes 0…10+N−1, padded with 0xFF to 4096.
- Partition: `mascot, data, 0x40, 0xcb0000, 0x1000,` in `partitions_16mb.csv`.
- Invalid / erased blob → Fluffy and the six default words, one `[mascot]` log line, no UI error.
- Factory reset and OTA never touch the mascot; it is not in backup files.
- Hard rules from AGENTS.md: never call the live Cyberspace API; keep Cyberspace logic in `lib/core` and hardware behind `src/hal/board.h`.
- Build with `nice`; don't flash unless the user says (the test board is a T-Deck Plus).
- Commit per task, message ending with `Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>`.

## Review Focus

- Art with trailing spaces or a blank middle line: trailing spaces trimmed, blank lines inside kept, blank lines at the end dropped (Task 1 test `test_trim_art`).
- A device whose partition table predates `mascot` (updated by OTA): read falls back to raw 0xcb0000, which is erased (0xFF) → Fluffy (Task 1 `test_erased_is_none`; Task 3 raw-read fallback).
- Web config saves while the splash/screensaver is up: next frame shows the new art, no crash from a stale phrase pointer (Task 2 makes `MonsterWords` return `std::string`).
- The flasher with defaults only: the normal one-part manifest, so nothing is written to 0xcb0000 (Task 5 test `defaults need no blob`).
- Pasted art with tabs or Unicode: dropped as typed on the site, rejected with a reason by the API (Task 1 `test_reject_non_ascii`, Task 4 400 response).

---

### Task 1: Mascot codec

**Files:**
- Create: `lib/core/src/mascot/mascot.h`, `lib/core/src/mascot/mascot.cpp`
- Create: `test/test_mascot/test_main.cpp`, `test/fixtures/mascot_golden.h`

**Interfaces:**
- Produces:
  - `struct rc::Mascot { std::vector<std::string> art, phrases; };` (empty vector = default)
  - `constexpr size_t rc::kMascotBlobSize = 4096; kMascotArtRows = 6; kMascotArtCols = 13; kMascotMaxPhrases = 10; kMascotPhraseMax = 80;`
  - `std::vector<uint8_t> rc::encodeMascot(const Mascot&)` — 4096 bytes; caller validated first.
  - `bool rc::decodeMascot(const uint8_t* data, size_t n, Mascot& out)` — false on any spec §3 violation or all-0xFF.
  - `std::string rc::validateMascot(const Mascot&)` — "" or a short reason ("art line 2 is over 13 characters").
  - `Mascot rc::cleanMascot(Mascot)` — trims trailing spaces per art line, drops trailing blank art lines, drops empty phrases.
  - `const std::vector<std::string>& rc::defaultMascotArt()`, `const std::vector<std::string>& rc::defaultMascotPhrases()` — Fluffy (moved from `splash_layout.cpp`) and the six words.
  - Uses `crc32Update` from `ui/screenshot.h`.

- [x] **Step 1: Write the golden fixture.** `test/fixtures/mascot_golden.h` defines `constexpr const char* kMascotGoldenHex` — the 10+N+4 meaningful bytes (no 0xFF padding), lowercase hex, space-separated — for art `{"/\\_/\\", "(o o)"}` and phrases `{"boo", "meow"}`. Generate the hex with:
  ```sh
  python3 -c "import zlib,struct;L=[b'/\\\\_/\\\\',b'(o o)',b'boo',b'meow'];p=b''.join(bytes([len(x)])+x for x in L);h=b'FLUF'+bytes([1,2,2,0])+struct.pack('<H',len(p))+p;print((h+struct.pack('<I',zlib.crc32(h))).hex(' '))"
  ```
- [x] **Step 2: Write failing tests** in `test/test_mascot/test_main.cpp`: `test_golden` (encode equals hex then 0xFF to 4096), `test_round_trip`, `test_bad_magic`, `test_bad_crc`, `test_bad_version`, `test_erased_is_none`, `test_reject_long_line` (14 chars), `test_reject_seven_rows`, `test_reject_eleven_phrases`, `test_reject_long_phrase` (81), `test_reject_non_ascii` (0x7F and "é"), `test_art_only` / `test_phrases_only` (other vector empty after decode), `test_trim_art` (`{"ab  ", "", "c", "", ""}` → `{"ab", "", "c"}`), `test_defaults` (Fluffy is 6 rows, widest 13; six phrases, first "boo").
- [x] **Step 3: Run** `nice pio test -e native -f test_mascot` — expect build failure (no header).
- [x] **Step 4: Implement** `mascot.{h,cpp}`; move `kArt` and the words' text out of `splash_layout.cpp` into the defaults (splash_layout keeps compiling by calling the defaults).
- [x] **Step 5: Run** `nice pio test -e native` — all green.
- [x] **Step 6: Commit** `Mascot codec: blob format, validation, defaults`.

### Task 2: Splash and bubble take the mascot

**Files:**
- Modify: `lib/core/src/ui/splash_layout.{h,cpp}`, `test/test_layout/test_main.cpp`
- Modify: `src/app/ui_task.{h,cpp}`, `src/screens/splash.cpp`, `src/screens/screens.h`

**Interfaces:**
- Consumes: Task 1 defaults.
- Produces:
  - `splashLayout(int cols, int rows, std::string_view detail, std::string_view say = {}, int maxScale = 1, const std::vector<std::string>* art = nullptr)` — nullptr or empty = Fluffy.
  - `std::string MonsterWords::next(uint32_t rnd, const std::vector<std::string>& phrases)` — empty list = defaults; a changed list restarts the round. `kMonsterWordCount`/`kMonsterWords` are removed.
  - `std::string ui::monsterWord()`; `SplashScreen::say_` becomes `std::string`.

- [x] **Step 1: Failing tests** in test_layout: `test_splash_custom_art` (2-line art `{"(o o)", "/|_|\\"}` at 40×15: two Art lines, widest 5, centred); `test_words_custom` (with `{"a","b"}` 20 draws give only a/b, never the same twice running); `test_words_single` (`{"x"}` always "x"); update the existing `MonsterWords` test to pass the defaults.
- [x] **Step 2: Run** `nice pio test -e native -f test_layout` — fails to compile.
- [x] **Step 3: Implement** the signatures above; art width is measured per mascot.
- [x] **Step 4: Wire the firmware** to pass `&app().mascot.art` / `app().mascot.phrases` (Task 3 adds the field; until then pass nullptr / `{}`). Run `nice pio test -e native` and `nice pio run -e tdeck`.
- [x] **Step 5: Commit** `Splash and bubble draw a given mascot`.

### Task 3: Partition, store, app state

**Files:**
- Modify: `partitions_16mb.csv`, `src/app/app_state.h`, `src/app/messages.h`, `src/app/ui_task.cpp`, `src/main.cpp`
- Create: `src/store/mascot_store.{h,cpp}`

**Interfaces:**
- Produces:
  - `rc::Mascot AppState::mascot;` (UI task owns it).
  - `void mascotstore::load(Mascot& out)` — partition by label, else raw `esp_flash_read` at 0xcb0000; logs `[mascot] custom art/phrases` or `[mascot] default`.
  - `bool mascotstore::save(const Mascot&)` — erase + write the 4 KB; then `postEvent(Event{EvtType::MascotChanged})`. Without the partition entry, raw `esp_flash_erase_region`/`esp_flash_write` at 0xcb0000.
  - `EvtType::MascotChanged` — UI task reloads with `load(app().mascot)`.

- [x] **Step 1:** Add the partition line (spec §3) and a comment that it survives OTA and factory reset.
- [x] **Step 2:** Implement the store; call `load` in `main.cpp` before `ui::start()`; handle `MascotChanged` in `ui_task.cpp`; switch Task 2's placeholders to `app().mascot`.
- [x] **Step 3: Verify** `nice pio run -e tdeck -e x4` succeed and `nice pio test -e native` is green.
- [x] **Step 4: Commit** `Mascot partition and store`.

### Task 4: Web config Mascot section

**Files:**
- Create: `site/mascots.json`, `tools/gen_mascot_presets.py`
- Modify: `platformio.ini` (each firmware env: `pre:tools/gen_mascot_presets.py`), `.gitignore` (`src/web/mascot_presets.h`), `src/web/web_server.cpp`, `src/web/page.h`

**Interfaces:**
- Produces:
  - `site/mascots.json`: `{"presets":[{"name":"Fluffy","art":[...]},{"name":"cat",...},{"name":"dog",...},{"name":"rhino",...},{"name":"platypus",...}],"phrases":["boo","moo","bop!","meow","hi!","We've been trying to reach you about your car's extended warranty."]}` — art exactly as spec §7.
  - Generated `src/web/mascot_presets.h`: `constexpr const char* kMascotPresetsJson` (the file, minified).
  - `GET /api/mascot` → `{"art":[...],"phrases":[...],"customArt":bool,"customPhrases":bool,"presets":<kMascotPresetsJson>}` (defaults filled in when not custom).
  - `POST /api/mascot` `{"art":[...],"phrases":[...]}` → `cleanMascot`, `validateMascot` (400 with the reason), art equal to the defaults stored as empty (same for phrases), `mascotstore::save`; `{"reset":true}` stores an all-default (erased) blob.

- [ ] **Step 1:** Write `site/mascots.json`. Add `test_presets_match_defaults` to `test_mascot`: it includes `../../src/web/mascot_presets.h`, parses it with ArduinoJson, and checks that Fluffy's art and the phrases equal `defaultMascotArt()`/`defaultMascotPhrases()` and every preset passes `validateMascot`. Run it: fails (no header yet).
- [ ] **Step 2:** Write the generator (validates every preset against spec §2 limits, exits non-zero on a violation); add it to the firmware envs and `[env:native]`. Run `nice pio test -e native` — green.
- [ ] **Step 3:** Add the routes (same `authorized(req)` gate and `max_uri_handlers` headroom) and a **Mascot** section in `page.h` after Canned replies: a 13-col × 6-row `<textarea>`, preset `<select>`, a 10-line phrases `<textarea>`, Save and Reset to Fluffy buttons, message line.
- [ ] **Step 4: Verify** `nice pio run -e tdeck` succeeds.
- [ ] **Step 5: Commit** `Web config: Mascot section and /api/mascot`.

### Task 5: Flasher modals and the second part

**Files:**
- Create: `site/mascot.mjs` (pure: `crc32`, `cleanMascot`, `validateMascot`, `encodeMascot` → `Uint8Array(4096)`, `isDefault`), `site/fluffy.js` (modals, localStorage, manifest), `site/fluffy.css`, `site/mascot.test.mjs`
- Modify: `site/index.html` (two buttons, a status line, the two script/link tags), `scripts/pages-site.sh` (copy `mascot.mjs`, `fluffy.js`, `fluffy.css`, `mascots.json`), `.github/workflows/build.yml` (a step `node --test site/` before the firmware builds)

**Interfaces:**
- Consumes: `site/mascots.json`; `test/fixtures/mascot_golden.h` (the test reads `kMascotGoldenHex` with a regex).
- Produces: when not default, `install.manifest` = blob URL of `{"name","version","new_install_prompt_erase":true,"builds":[{"chipFamily", "parts":[{"path":<absolute firmware URL>,"offset":0},{"path":<blob URL of the 4 KB mascot>,"offset":13303808}]}]}` (0xcb0000), built from the board's normal manifest; recomputed when the board, art or phrases change.

- [ ] **Step 1: Failing tests** in `site/mascot.test.mjs`: `golden` (encoding the golden mascot matches `kMascotGoldenHex`, then 0xFF), `crc` (`crc32("123456789") === 0xCBF43926`), `limits` (14-char line, 7 rows, 11 phrases, 81-char phrase, "é" all rejected with a reason), `defaults need no blob` (`isDefault` true for the JSON's Fluffy and phrases, false after one change). Run `node --test site/` — fails.
- [ ] **Step 2:** Implement `mascot.mjs`; run `node --test site/` — green.
- [ ] **Step 3:** Build the modals per spec §5 in `fluffy.js`/`fluffy.css` (match the page's existing look; Esc and the backdrop close; focus returns to the opening button) and the manifest swap; edit `index.html` minimally (the other agent edits its prose).
- [ ] **Step 4: Verify** locally: `scripts/pages-site.sh <latest tag> <dir with that release's images> /tmp/site` (download with `gh release download`), serve `/tmp/site` with `python3 -m http.server`, and in a browser check the modals, presets, counters, localStorage, and that the generated manifest JSON lists both parts (log it to the console). No device flash.
- [ ] **Step 5: Commit** `Flasher: Fluffy and Phrases modals, mascot flashed as a second part`.

### Task 6: Docs

**Files:** `docs/USE.md` (Mascot section: flasher, web config, survives OTA/reset), `docs/BUILD.md` (partition, second part, presets file and generator), `docs/TESTING.md` (open hardware checks per spec §8, T-Deck Plus), `AGENTS.md` (spec/plan pointers; `site/mascots.json` is the presets source).

- [ ] **Step 1:** Write them.
- [ ] **Step 2: Verify** `nice pio test -e native` green, `node --test site/` green, `scripts/build-upload-monitor.sh -B` only if the user allows the full build.
- [ ] **Step 3: Commit** `Docs: Fluffy Generator`.
