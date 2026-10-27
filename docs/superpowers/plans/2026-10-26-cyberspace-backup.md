# Rhino Cyberspace settings backup and restore Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Save every device setting to a YAML file on the SD card or through web config, and restore it on this or another device, which then rejoins Wi-Fi still signed in.

**Architecture:**

- The YAML codec, merge and validation, and the file-name rules are pure functions in `lib/core/src/config/`, with native tests. `rc::Settings` moves there too.
- On the device:
  - `board::sdMount()`/`sdUnmount()` give the card behind the HAL, one operation at a time;
  - `src/store/backup_store` does every file operation under one mutex and applies a restore with `settings::saveAll`, then restarts;
  - a shared `BackupPicker` overlay serves the Settings and Setup screens;
  - web config gains a Backup section and seven routes.

**Tech Stack:** C++17, pioarduino (Arduino 3.3 / ESP-IDF 5.5), Arduino `SD`/`FS`, `esp_http_server`, ArduinoJson 7, Unity (native tests).

**Spec:** `docs/superpowers/specs/2026-10-26-cyberspace-backup-design.md`

## Global Constraints

- Never call the live API (`api.cyberspace.online` or its RTDB) from tests, scripts or an agent.
- `lib/core` stays Arduino-free and board-agnostic. Device code outside `src/hal/` reaches the card only through `src/hal/board.h`; never include `src/hal/<board>/` headers elsewhere.
- Every backup and restore comes from a key press or a web action. Nothing is automatic or scheduled, and nothing is retried.
- Backup files deliberately contain the Wi-Fi, page and setup-AP passwords and the refresh token. They are **never logged**: no `Serial` output of YAML, field values or parse input. Buffers that held a backup are wiped (zero-filled) after use, like `wipe()` in `web_server.cpp`.
- The Cyberspace password is never stored, so it is never in a backup.
- Format: `format: rhino-cyberspace-backup`, `version: 1`. Files over 16 KB (`16384` bytes) are refused from their size.
- At most `rc::kMaxWifiNets` (5) networks. SSID 1–32 bytes; Wi-Fi password empty or 8–63 characters; setup-AP password passes `rc::validApPassword`; page password at most 63 printable characters; text size `compact` or `comfortable`; theme `default`, `bw` or `wb`.
- Files live in `/rhino-cyberspace/backups/`. Names are `rc-backup-YYYYMMDD-HHMMSS.yaml`, or `rc-backup-N.yaml` without a clock. A valid name is only `[A-Za-z0-9._-]`, ends in `.yaml`, has no leading dot, and is at most 64 bytes.
- SD on every hardware board, and the card is removable: nothing stays mounted between operations. Each backup-store operation calls `board::sdMount()`, works, then `board::sdUnmount()`. M9: `SD`, shared SPI, CS 48, 20 MHz. Wio L2: `SD_MMC` 1-bit, CLK 2 / CMD 3 / D0 1, power on expander bit 14 (on before mounting, off after unmounting). T-Deck Plus gets its SD code in the T-Deck plan.
- User-facing copy (exact): `Back up to SD card`, `Restore from SD card…`, `Restore <name> and restart?` with `Cancel` / `Restore`, `Saved <name>`, `No SD card`, `SD write failed`, `SD read failed`, `Restore failed: <error>`, `Restored. Restarting…`, `Not a Rhino Cyberspace backup`, `Backup version N is newer than this firmware`.
- A successful restore restarts the device after about 1 s (`1000` ms).
- Never hold `lockedChat` or `lockedFeed` while calling the backup store.
- Each task follows TDD where it has host tests and ends with a commit whose message ends with a blank line and `Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>`.
- Verify: `pio test -e native` is green (195 tests before this plan); from Task 4 on, `pio run -e m9`, `-e m9_fake`, `-e wio_l2` and `-e wio_l2_fake` all build (`scripts/build-upload-monitor.sh -B`).

## Review Focus

1. **A backup from a later firmware with a new nested block** (`gps:` with indented keys, or a new list): the spec says unknown keys are ignored, so the whole indented block under an unknown top-level key must be skipped, not rejected as a bad indent (Task 2: `test_unknown_nested_block_ignored`).
2. **A file saved by a Windows editor:** a UTF-8 byte-order mark before line 1, CRLF endings and no final newline. Expected: it restores like the original (Task 2: `test_bom_crlf_no_final_newline`).
3. **A restore that fails half-way through the file:** a bad value on the last line must leave every setting untouched, including the networks parsed earlier (Task 2: `test_failure_leaves_settings_unchanged`).
4. **A backup taken while signed out** (`refreshToken: ""`) restored onto a signed-in device: the device ends up signed out, not holding the old account's token under the backup's email (Task 2: `test_empty_token_replaces_current`).
5. **A hand-edited quoted value with a trailing comment** (`ssid: "Home"  # main`) or a `#` inside quotes (`password: "a#b c#d"`): the comment is trimmed only outside quotes (Task 1: `test_scalar_comments_and_hash_in_quotes`).

---

## File Structure

| File | Responsibility |
|---|---|
| `lib/core/src/config/settings_model.h` (create) | `rc::WifiNet`, `rc::Settings`, `rc::kMaxWifiNets`, moved from `src/store/settings.h`. |
| `lib/core/src/config/yaml_scalar.{h,cpp}` (create) | Quoting a string for output; reading one scalar (double, single, plain) with trailing-comment trimming. |
| `lib/core/src/config/backup_yaml.{h,cpp}` (create) | `BackupMeta`, `backupToYaml`, `backupFromYaml` (parse, merge, validate), `backupFileName`, `validBackupName`, `sortBackupNames`, `kMaxBackupBytes`. |
| `test/test_backup_yaml/test_main.cpp` (create) | All native backup tests. |
| `src/store/settings.{h,cpp}` (modify) | Includes the model header; adds `settings::saveAll`. |
| `src/hal/board.h`, `src/hal/m9/board_m9.cpp`, `src/hal/wio_l2/board_wio_l2.cpp` (modify) | `board::hasSdSlot()`, `board::sdMount()`, `board::sdUnmount()`. |
| `src/app/restart.{h,cpp}` (create) | `restartSoon(ms)`, moved out of `web_server.cpp`. |
| `src/store/backup_store.{h,cpp}` (create) | SD file operations under a mutex, YAML for download, `applyAndRestart`. |
| `src/screens/backup_picker.{h,cpp}` (create) | The file list overlay and the restore confirm, shared by Settings and Setup. |
| `src/screens/settings.cpp`, `setup.cpp`, `screens.h` (modify) | The new menu items. |
| `src/web/web_server.cpp`, `src/web/page.h` (modify) | Seven routes and the Backup section. |
| `docs/USE.md`, `docs/TESTING.md`, `AGENTS.md` (modify) | User guide, hardware checks, rule change and pointer. |

---

### Task 1: Settings model in lib/core and YAML scalars

**Files:**
- Create: `lib/core/src/config/settings_model.h`, `lib/core/src/config/yaml_scalar.{h,cpp}`, `test/test_backup_yaml/test_main.cpp`
- Modify: `src/store/settings.h`

**Interfaces:**
- Produces, `settings_model.h` (namespace `rc`): `WifiNet`, `Settings` and `kMaxWifiNets` exactly as they are in `src/store/settings.h` today (same fields and defaults: `tz = "UTC0"`, `tzMode = TzMode::Manual`, `ntpServer = "pool.ntp.org"`, `TextSize::Compact`, `drawerOpen = false`, `buzzer = true`, `ThemeId::Default`, `lastRoom = "general"`). It includes `ui/grid.h` and `ui/theme.h`. `src/store/settings.h` drops those definitions and includes `config/settings_model.h`; the `settings::` declarations stay.
- Produces, `yaml_scalar.h` (namespace `rc`):
  ```cpp
  // Double-quoted, with \\ \" \n \t and \uXXXX for other bytes < 0x20 and 0x7F.
  // Other UTF-8 (accents, emoji) is written as-is.
  std::string yamlQuote(std::string_view s);
  enum class ScalarKind { Plain, Double, Single };
  // Reads the scalar that makes up the rest of a line (after "key: ").
  // Leading and trailing spaces are dropped; a plain scalar ends at the first
  // " #"; after a closing quote only spaces or " # comment" may follow.
  // false with err (no "line N: " prefix) on a bad escape, an unclosed quote,
  // or junk after the closing quote.
  bool yamlScalar(std::string_view rest, std::string& out, ScalarKind& kind, std::string& err);
  ```
  Double-quoted input accepts `\\ \" \n \t \r` and `\uXXXX` (encoded to UTF-8; a surrogate is an error). Single-quoted input reads `''` as `'` and has no escapes.

- [ ] **Step 1: Write the failing tests** in `test/test_backup_yaml/test_main.cpp` (Unity, `setUp`/`tearDown` empty, `RUN_TEST` each in `main`, like `test/test_theme`):
  ```cpp
  void test_quote_escapes() {
    TEST_ASSERT_EQUAL_STRING("\"a\\\\b\\\"c\\nd\\te\"", yamlQuote("a\\b\"c\nd\te").c_str());
    TEST_ASSERT_EQUAL_STRING("\"\\u0001\\u007F\"", yamlQuote("\x01\x7F").c_str());
    TEST_ASSERT_EQUAL_STRING("\"Café 📶\"", yamlQuote("Café 📶").c_str());
  }
  void test_scalar_round_trip() {
    // for each of "", "x", "a\\b\"c\nd\te", "\x01", "Café 📶", "it's":
    //   yamlScalar(yamlQuote(s)) → ok, kind Double, out == s
  }
  void test_scalar_plain_and_single() {
    // "compact" → Plain "compact";  "  hello world  " → "hello world"
    // "'it''s'" → Single "it's";  "'a\\n'" → "a\\n" (no escapes)
  }
  void test_scalar_comments_and_hash_in_quotes() {
    // "\"Home\"  # main" → "Home";  "compact # small" → "compact"
    // "\"a#b c#d\"" → "a#b c#d";  "'x # y'" → "x # y";  "a#b" → "a#b" (no space before #)
  }
  void test_scalar_errors() {
    // false for: "\"open", "'open", "\"x\" junk", "\"\\q\"", "\"\\uD83D\""
  }
  ```
- [ ] **Step 2: Run** `pio test -e native -f test_backup_yaml`. Expected: FAIL (headers missing).
- [ ] **Step 3: Implement** `settings_model.h`, the `settings.h` change, and `yaml_scalar.cpp`. Use `lib/core/src/util/utf8.h` to encode `\uXXXX`.
- [ ] **Step 4: Run** `pio test -e native` (all PASS) and `pio run -e m9` (SUCCESS: the move changes no behaviour).
- [ ] **Step 5: Commit**
  ```bash
  git add lib/core/src/config test/test_backup_yaml src/store/settings.h
  git commit -m "feat(core): settings model in lib/core, YAML scalars" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
  ```

### Task 2: Backup YAML writer, reader, merge and validation

**Files:**
- Create: `lib/core/src/config/backup_yaml.{h,cpp}`
- Test: `test/test_backup_yaml/test_main.cpp`

**Interfaces:**
- Consumes: `Settings`, `WifiNet`, `kMaxWifiNets` (Task 1); `yamlQuote`, `yamlScalar` (Task 1); `themeKey`, `parseThemeId` (`ui/theme.h`); `validApPassword`, `validNtpServer` (`web/web_util.h`); `tzModeKey`, `parseTzMode` (`tz/tz_lookup.h`).
- Produces, `backup_yaml.h` (namespace `rc`):
  ```cpp
  constexpr size_t kMaxBackupBytes = 16384;
  struct BackupMeta {
    std::string board, firmware;  // board::name(), RC_VERSION
    int64_t epochSec = 0;         // < 1700000000 means the clock isn't set
  };
  std::string backupToYaml(const Settings& s, const BackupMeta& m);
  // Parses, validates, then merges into inOut. On false, err holds the first
  // error and inOut is untouched. note is "" or "Kept the first 5 of N networks."
  bool backupFromYaml(std::string_view yaml, Settings& inOut, std::string& err, std::string& note);
  ```

**Writer.** Output is exactly the spec §3 example's layout: the three `#` header lines, then keys in that order, two-space indents, `- ssid:` items with `password:` under `ssid`.
- Strings use `yamlQuote`: `board`, `firmware`, `created`, `ssid`, `password`, `timezone`, `ntpServer`, `pagePassword`, `apPassword`, `lastRoom`, `email`, `refreshToken`.
- Plain values: `format`, `version`, `textSize`, booleans, `theme`, which is written as `theme: <key>  # default | bw | wb`, and `tzMode`, written as `tzMode: <key>  # auto | manual` right after `timezone`.
- `created` is `"YYYY-MM-DDTHH:MM:SSZ"` (UTC, from `gmtime_r`), or `""` when `epochSec < 1700000000`.
- `wifi: []` when the list is empty.

**Reader rules** (each error is `line N: …`, N 1-based):
- Strip one leading UTF-8 BOM. Split on `\n` and drop a trailing `\r`. Skip blank lines and lines whose first non-space is `#`. A tab in the indent is `bad indent`.
- Indent 0: `key: value` or `key:` (an empty value opens a block). Anything else is `expected "key: value"`.
- `wifi` takes `[]`, or an empty value followed by items. An item is `- key: value` at one indent greater than 0. Its other keys sit at the dash's column + 2. All items share the first item's indent. Anything else is `bad indent`. Any other flow value is `expected "key: value"`.
- `account` takes an empty value followed by `key: value` lines at one shared indent greater than 0; with no such lines it is an empty block and changes nothing.
- An unknown top-level key is ignored together with every following line indented deeper than 0. Unknown keys inside a wifi item or `account` are ignored.
- A key repeated at the same level is `duplicate key "<key>"`.
- `version` is a plain integer. Booleans are plain `true`/`false`, else `expected true or false`.
- Over `kMaxBackupBytes` → `Backup is over 16 KB` (no line).
- After parsing: `format` ≠ `rhino-cyberspace-backup`, or `version` missing or not a positive integer → `Not a Rhino Cyberspace backup`. A version above 1 → `Backup version N is newer than this firmware`. These are checked before field validation and carry no line.

**Validation** names the field's line:
- `line N: network name: 1-32 bytes`;
- `line N: Wi-Fi password: 8-63 characters, or empty`;
- `line N: duplicate network "<ssid>"`;
- `line N: setup-AP password: 8-63 printable characters, or empty`;
- `line N: page password: up to 63 printable characters`;
- `line N: text size: compact or comfortable`;
- `line N: theme: default, bw or wb`;
- `line N: time zone: 1-63 printable characters` (the web config rule);
- `line N: zone mode: auto or manual`;
- `line N: NTP server: a host name, 1-63 characters` (`validNtpServer`);
- `line N: last room: 1-63 printable characters`.

Networks after the fifth are dropped before validation and set `note`.

**Merge:** each present key replaces the current value; a present `wifi` replaces the whole list; inside `account`, each present key replaces its own value.

- [ ] **Step 1: Write the failing tests.** A helper `full()` builds a `Settings` with every field non-default: 2 networks, one SSID `Café "📶"\\` with password `p@ss\nword` (10 bytes or more), `theme = WhiteBlack`, `textSize = Comfortable`, `drawerOpen = true`, `buzzer = false`, a token.
  ```cpp
  void test_round_trip_every_field() {
    // y = backupToYaml(full(), {"M9","0.1.0",1793039400}); Settings d{}; ok;
    // every field of d equals full(); err == "" and note == ""
    // y contains "created: \"2026-10-26T18:30:00Z\"" and "theme: wb  # default | bw | wb"
    // y starts with "# Rhino Cyberspace settings backup.\n"
  }
  void test_writer_no_clock_no_wifi() {   // epochSec 0, wifi empty
    // contains "created: \"\"\n" and "wifi: []\n"; and it parses back with wifi empty
  }
  void test_hand_edits() {
    // plain and single-quoted scalars, comments, a trailing comment,
    // keys in another order (account before format), CRLF endings → values match
  }
  void test_bom_crlf_no_final_newline() {
    // "\xEF\xBB\xBF" + CRLF text with no final "\r\n" parses
  }
  void test_missing_keys_keep_current() {
    // only format/version/buzzer: false over full() → only buzzer changed
    // account with only email → refreshToken unchanged
  }
  void test_empty_wifi_list_clears() { /* "wifi: []" over full() → wifi empty */ }
  void test_empty_token_replaces_current() { /* refreshToken: "" → inOut.refreshToken == "" */ }
  void test_unknown_keys_ignored() {
    // "colour: red" at top level, "hidden: true" in an item, "nick: x" in account
  }
  void test_unknown_nested_block_ignored() {
    // "gps:\n  enabled: true\n  fix:\n    - a\n" then "buzzer: false" → ok, buzzer false
  }
  void test_wrong_format_and_version() {
    // format: other → "Not a Rhino Cyberspace backup"; no format → same
    // version: 2 → "Backup version 2 is newer than this firmware"; version: x → "Not a…"
  }
  void test_bad_indent() {
    // each gives err == "line N: bad indent", N the offending line:
    // an item's password one column right of its ssid; a second item at a
    // different indent from the first; an account key indented with a tab
    // ("account:" followed directly by a column-0 key is an empty block: no change)
  }
  void test_expected_key_value() { /* "just text" on line 5 → "line 5: expected \"key: value\"" */ }
  void test_bad_values_rejected() {
    // each one fails, inOut unchanged, err names the line:
    // Wi-Fi password "short"; ssid ""; 33-byte ssid; apPassword "abc";
    // pagePassword with "\n"; textSize: huge; theme: pink; buzzer: maybe; duplicate ssid
  }
  void test_more_than_five_networks_truncated() {
    // 7 items → ok, wifi.size()==5 (first five, in order),
    // note == "Kept the first 5 of 7 networks."
  }
  void test_backup_time_keys() {
    // tzMode Manual + ntpServer "time.example.org" round-trip;
    // a backup without tzMode and ntpServer keeps the current values;
    // ntpServer: "a b" → false, err "line N: NTP server: a host name, 1-63 characters", inOut unchanged
  }
  void test_duplicate_key() { /* two "buzzer:" lines → "line N: duplicate key \"buzzer\"" */ }
  void test_oversize_refused() { /* 16385 bytes of comments → "Backup is over 16 KB" */ }
  void test_failure_leaves_settings_unchanged() {
    // valid wifi list, then "textSize: huge" on the last line → false; inOut equals full()
  }
  ```
- [ ] **Step 2: Run** `pio test -e native -f test_backup_yaml`. Expected: FAIL.
- [ ] **Step 3: Implement** `backup_yaml.cpp`. Parse into a staging struct that marks each field present, with its line number. Validate it. Only then copy the present fields into `inOut`.
- [ ] **Step 4: Run** `pio test -e native -f test_backup_yaml`. Expected: PASS.
- [ ] **Step 5: Commit**
  ```bash
  git add lib/core/src/config/backup_yaml.h lib/core/src/config/backup_yaml.cpp test/test_backup_yaml
  git commit -m "feat(core): settings backup YAML codec with merge and validation" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
  ```

### Task 3: Backup file names

**Files:**
- Modify: `lib/core/src/config/backup_yaml.{h,cpp}`
- Test: `test/test_backup_yaml/test_main.cpp`

**Interfaces:**
- Produces (namespace `rc`):
  ```cpp
  // epochSec >= 1700000000: "rc-backup-YYYYMMDD-HHMMSS.yaml" (UTC); on a clash
  // "…-HHMMSS-2.yaml", "-3", …  Without a clock: "rc-backup-N.yaml", N one more
  // than the highest such N in existing (1 when none).
  std::string backupFileName(int64_t epochSec, const std::vector<std::string>& existing);
  bool validBackupName(std::string_view name);
  // Newest first: dated names descending, then rc-backup-N descending by N,
  // then any other valid names in ascending byte order.
  void sortBackupNames(std::vector<std::string>& names);
  ```

- [ ] **Step 1: Write the failing tests**
  ```cpp
  void test_file_name_with_clock() {
    // 1793039400, {} → "rc-backup-20261026-183000.yaml"
    // with that name existing → "rc-backup-20261026-183000-2.yaml"; with both → "-3"
  }
  void test_file_name_without_clock() {
    // 0, {} → "rc-backup-1.yaml"; {"rc-backup-1.yaml","rc-backup-7.yaml","x.yaml"} → "rc-backup-8.yaml"
  }
  void test_valid_backup_name() {
    // true: "rc-backup-1.yaml", "My_file.v2.yaml"
    // false: "../x.yaml", "a/b.yaml", ".yaml", "x.yml", "x.YAML", "a b.yaml", "", a 65-byte name;
    // true for a 64-byte name
  }
  void test_sort_backup_names() {
    // {"rc-backup-2.yaml","old.yaml","rc-backup-20261026-183000.yaml",
    //  "rc-backup-10.yaml","rc-backup-20261101-090000.yaml"} →
    // {"rc-backup-20261101-090000.yaml","rc-backup-20261026-183000.yaml",
    //  "rc-backup-10.yaml","rc-backup-2.yaml","old.yaml"}
  }
  ```
- [ ] **Step 2: Run** `pio test -e native -f test_backup_yaml`. Expected: FAIL.
- [ ] **Step 3: Implement** the three functions.
- [ ] **Step 4: Run** `pio test -e native`. Expected: all PASS.
- [ ] **Step 5: Commit**
  ```bash
  git add lib/core/src/config/backup_yaml.h lib/core/src/config/backup_yaml.cpp test/test_backup_yaml
  git commit -m "feat(core): backup file names" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
  ```

### Task 4: SD card behind the HAL, shared restart, saveAll

**Files:**
- Modify: `src/hal/board.h`, `src/hal/m9/board_m9.cpp`, `src/hal/wio_l2/board_wio_l2.cpp`, `src/store/settings.{h,cpp}`, `src/web/web_server.cpp`
- Create: `src/app/restart.{h,cpp}`

**Interfaces:**
- Produces, `board.h` (forward-declare `namespace fs { class FS; }`, so the header stays free of `FS.h`):
  ```cpp
  bool hasSdSlot();   // M9 and Wio L2 true
  // Mounts the card for one operation; nullptr with no slot or no card (the
  // card may have been pulled since last time). Pair every non-null result
  // with sdUnmount(). Not thread-safe: only the backup store calls these.
  fs::FS* sdMount();
  void sdUnmount();
  ```
  - M9: `SD.begin(m9::SD_CS, SPI, 20000000)` on the bus `board::begin` already set up; `SD.end()` to unmount.
  - Wio L2: power on (expander bit 14, through the existing expander lock), wait 10 ms, `SD_MMC.setPins(2, 3, 1)`, `SD_MMC.begin("/sdcard", true, false)`; unmount is `SD_MMC.end()` then power off. Add the pin constants to `pins_wio_l2.h`.
  - Either board: a mount that reports `cardType() == CARD_NONE` is ended and treated as no card.
- Produces, `src/app/restart.h`: `namespace rc { void restartSoon(uint32_t delayMs); }`. This is the `restartTask` from `web_server.cpp`, with the delay as a parameter. Web config keeps its 600 ms.
- Produces, `settings.h`: `void saveAll(const rc::Settings&);`. It calls `saveWifi`, `saveTz`, `saveSecurity`, `saveUiPrefs` and `saveAccount`. When `refreshToken` is empty it calls `clearToken()` in place of saving the token.

- [ ] **Step 1: Implement** the above. Remove `restartTask`/`restartSoon` from `web_server.cpp` and call `rc::restartSoon(600)`.
- [ ] **Step 2: Run** `pio test -e native` (PASS) and `scripts/build-upload-monitor.sh -B`. Expected: all four firmware envs build.
- [ ] **Step 3: Commit**
  ```bash
  git add src/hal src/app/restart.h src/app/restart.cpp src/store/settings.h src/store/settings.cpp src/web/web_server.cpp
  git commit -m "feat(hal): SD card access, shared restart, settings::saveAll" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
  ```

### Task 5: Backup store

**Files:**
- Create: `src/store/backup_store.{h,cpp}`
- Modify: `src/main.cpp` (call `backupstore::begin()` after `board::begin()`)

**Interfaces:**
- Consumes: `board::hasSdSlot`, `board::sdMount`, `board::sdUnmount`, `rc::restartSoon`, `settings::load`, `settings::saveAll` (Task 4); `backupToYaml`, `backupFromYaml`, `backupFileName`, `validBackupName`, `sortBackupNames`, `kMaxBackupBytes` (Tasks 2–3); `RC_VERSION`, `board::name()`.
- Produces, `backup_store.h` (namespace `backupstore`):
  ```cpp
  struct Result { bool ok = false; std::string message; };  // message is user-facing copy
  struct FileInfo { std::string name; size_t size = 0; };
  void begin();  // creates the mutex
  // The current settings (fresh from NVS, token included) as YAML, and a
  // download name. The caller wipes yaml after sending it.
  void currentYaml(std::string& yaml, std::string& fileName);
  Result saveToSd();                                         // "Saved <name>"
  Result listSd(std::vector<FileInfo>& out);                 // valid names only, newest first
  Result readSd(const std::string& name, std::string& yaml); // refuses > kMaxBackupBytes from size()
  Result deleteSd(const std::string& name);                  // "Deleted <name>"
  // Merges into the NVS settings and saves them with settings::saveAll, then
  // restartSoon(1000): "Restored. Restarting…", with the note inserted before
  // "Restarting…" when set. A failure is "Restore failed: <err>" and saves nothing.
  Result applyAndRestart(std::string_view yaml);
  ```
- Rules:
  - Every function takes the mutex for its whole body, mounts with `board::sdMount()`, and calls `board::sdUnmount()` before returning on every path (an RAII guard).
  - `sdMount()` null gives `"No SD card"`.
  - Each name passes `validBackupName`, else `"Bad file name"`.
  - The directory is `/rhino-cyberspace/backups`; create both levels on save.
  - A short write or failed open gives `"SD write failed"`, and the partial file is removed. A failed read gives `"SD read failed"`. Over the size gives `"Restore failed: Backup is over 16 KB"`.
  - Local YAML strings are wiped before return.
  - `restartSoon` is called only after the settings are saved.

- [ ] **Step 1: Implement** `backup_store.cpp`.
- [ ] **Step 2: Run** `scripts/build-upload-monitor.sh -B`. Expected: all envs build.
- [ ] **Step 3: Commit**
  ```bash
  git add src/store/backup_store.h src/store/backup_store.cpp src/main.cpp
  git commit -m "feat(store): backup store for the SD card and restore" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
  ```

### Task 6: Device UI: Settings and Setup

**Files:**
- Create: `src/screens/backup_picker.{h,cpp}`
- Modify: `src/screens/screens.h`, `src/screens/settings.cpp`, `src/screens/setup.cpp`

**Interfaces:**
- Consumes: `backupstore::listSd`, `saveToSd`, `readSd`, `applyAndRestart` (Task 5); `rc::ui::banner(text, error)`; `ListView`; `board::hasSdSlot()`.
- Produces, `backup_picker.h` (namespace `rc`):
  ```cpp
  class BackupPicker {
   public:
    // Lists the card. Shows a banner and returns false on "No SD card" or
    // "No backups on the SD card".
    bool open();
    bool isOpen() const;
    void close();
    // Up/Down pick; Select/Right on a file → confirm "Restore <name> and restart?"
    // (Cancel selected first / Restore). Restore reads, applies, and banners the
    // Result (error tone when !ok). Back/Left leaves the confirm, then the list.
    void onKey(const KeyEvent&);
    void render(Grid&, const Theme&);  // fills the rows above the hint, draws its own hint
  };
  ```
- Settings:
  - When `board::hasSdSlot()`, the menu gains `Back up to SD card` and `Restore from SD card…` after the setup-AP item. Item indices and the menu height follow from that, so `kItems` is no longer a constant.
  - Backup calls `saveToSd()` and banners the result.
  - Restore calls `picker_.open()`.
  - `hasOverlay()`/`closeOverlays()` include the picker.
- Setup:
  - `enter` calls `listSd` once.
  - When `hasSdSlot()` and the list is non-empty, it adds `Restore from SD card` after `Log in on this device`.
  - `onKey` dispatches by the selected index, and to the picker while it is open.

- [ ] **Step 1: Implement.**
- [ ] **Step 2: Run** `scripts/build-upload-monitor.sh -B`. Expected: all envs build.
- [ ] **Step 3: Flash `m9`** (stop any monitor first) with a FAT32 card in, and check:
  - Back up shows `Saved rc-backup-….yaml`;
  - Restore lists it newest first, and Cancel is preselected;
  - Restore restarts with the same settings;
  - with the card out, both items say `No SD card`.
- [ ] **Step 4: Commit**
  ```bash
  git add src/screens/backup_picker.h src/screens/backup_picker.cpp src/screens/screens.h src/screens/settings.cpp src/screens/setup.cpp
  git commit -m "feat(ui): back up and restore from the SD card in Settings and Setup" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
  ```

### Task 7: Web config Backup section

**Files:**
- Modify: `src/web/web_server.cpp`, `src/web/page.h`

**Interfaces:**
- Consumes: every `backupstore::` function (Task 5), `rc::validBackupName`, `rc::urlDecode`, `rc::kMaxBackupBytes`, `board::hasSdSlot()`.
- Routes (each begins with `authorized(req)`; failures use `fail()` with `{ok:false,message}`):

  | Route | Handler | Reply |
  |---|---|---|
  | `GET /api/backup` | `getBackup` | `text/yaml; charset=utf-8`, `Content-Disposition: attachment; filename="<name>"`, `Cache-Control: no-store` |
  | `POST /api/restore` | `postRestore` | `content_len > kMaxBackupBytes` → `413` `"Restore failed: Backup is over 16 KB"`, before reading; otherwise `applyAndRestart` → `okReply` / `400` |
  | `GET /api/backup/sd` | `getBackupSd` | `{slot, card, files:[{name,size}]}`; `card` is `listSd(...).ok` |
  | `POST /api/backup/sd` | `postBackupSd` | `{ok, message:"Saved <name>"}` |
  | `POST /api/backup/sd/restore?name=` | `postBackupSdRestore` | `readSd` then `applyAndRestart` |
  | `GET /api/backup/sd/download?name=` | `getBackupSdDownload` | as `GET /api/backup`, with that name |
  | `POST /api/backup/sd/delete?name=` | `postBackupSdDelete` | `deleteSd` |

  - A helper `bool queryName(httpd_req_t*, std::string& name)` decodes `name` with `urlDecode`. It returns false unless `validBackupName(name)`, which gives `400` `"Bad file name"`. It runs before any store call.
  - A helper `bool readBody(httpd_req_t*, std::string& body, size_t max)` serves the raw YAML upload; the body is wiped after use.
  - Raise `cfg.max_uri_handlers` from 16 to 24.
- Page:
  - A `<h2>Backup</h2>` section before Device.
  - Notice (exact): "A backup holds your Wi-Fi passwords, the page and setup-AP passwords and a Cyberspace sign-in token. Anyone with the file can use your account: keep it private. It travels over plain HTTP unless you use the setup AP with a password."
  - A `Download backup` link to `/api/backup`.
  - A file input (`accept=".yaml"`) with `Restore from file…`. It refuses files over 16384 bytes on the client side, confirms `Restore <file name> and restart?`, then POSTs the raw text as `application/yaml`.
  - An SD block, hidden when `slot` is false:
    - `Save backup to SD card`;
    - the file list, each row with `restore`, `download` and `delete` (both restore and delete confirm);
    - `No SD card` when `card` is false.
  - Results go to `backupmsg` via `show()`.

- [ ] **Step 1: Implement.**
- [ ] **Step 2: Run** `pio test -e native` (PASS) and `scripts/build-upload-monitor.sh -B` (all envs build).
- [ ] **Step 3: Flash `m9`** and check in a browser, with a page password set:
  - download, then upload-restore of that file (the device restarts with the same settings);
  - save to SD, then the list's download, restore and delete;
  - `curl -u admin:<pw> -X POST 'http://<ip>/api/backup/sd/delete?name=..%2Fx.yaml'` gives `Bad file name`;
  - a 17 KB upload gives the 16 KB error;
  - with the card out, the SD block says `No SD card`.
- [ ] **Step 4: Commit**
  ```bash
  git add src/web/web_server.cpp src/web/page.h
  git commit -m "feat(web): settings backup download, upload restore and SD backups" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
  ```

### Task 8: Documentation

**Files:**
- Modify: `docs/USE.md`, `docs/TESTING.md`, `AGENTS.md`

- [ ] **Step 1: `USE.md`.** Add a "Backup and restore" section:
  - the Settings items, the Setup screen item and the web Backup section;
  - where the files live;
  - that the file holds passwords and a sign-in token, so keep it private;
  - that missing keys keep the current values;
  - that a restore restarts the device.
- [ ] **Step 2: `TESTING.md`.** Under "Open hardware checks", add the spec §7 hardware list:
  - Settings → Back up to SD card writes the file;
  - the file opens on a computer and holds everything;
  - Restore from SD card restarts with the same settings;
  - after a factory reset, with the card holding backups, Setup offers a restore and the device comes back signed in;
  - web download, upload-restore, save to SD, then restore, download and delete from the SD list;
  - with no card, every path says `No SD card`.
- [ ] **Step 3: `AGENTS.md`.**
  - Replace the hard rule with: "Never log or store passwords, except that backup files (`docs/superpowers/specs/2026-10-26-cyberspace-backup-design.md`) deliberately contain the Wi-Fi, page and setup-AP passwords and the refresh token. Never log them, or tokens."
  - Add a line with the backup spec and this plan.
  - Update the progress section and the native test count from `pio test -e native`.
- [ ] **Step 4: Commit**
  ```bash
  git add docs/USE.md docs/TESTING.md AGENTS.md
  git commit -m "docs: settings backup guide, checks and rule change" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
  ```
