# Rhino Cyberspace GPS Time, RTC and Automatic Time Zone Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep the clock right on every board with or without Wi-Fi (NTP, then GPS, then the M9's PCF8563 RTC), and set the time zone offline from the GPS position.

**Architecture:**

- Pure logic lives in `lib/core`, with native tests:
  - `gps/nmea` parses the GPS byte stream into a `GpsState`;
  - `time/clock_policy` decides when to set the clock, write the RTC and power the GPS;
  - `tz/tz_lookup` finds the zone in a flash blob;
  - `time/civil` and `time/pcf8563` do the date and register maths.
- The board glue goes behind `src/hal/board.h` (GPS power and UART, RTC).
- A small time service task (`src/app/time_service.cpp`) feeds the parser, ticks the policy, applies its actions and publishes status lines for the Settings screen and the web page.
- `scripts/tzgen.py` turns timezone-boundary-builder's GeoJSON into `src/tz/tz_data.h`.

**Tech Stack:** C++17, pioarduino (Arduino 3.3 / ESP-IDF 5.5, `esp_sntp`), Unity native tests, Python 3 standard library only (for `tzgen.py`).

**Spec:** `docs/superpowers/specs/2026-10-26-cyberspace-gps-time-design.md`

## Global Constraints

- Never call the live Cyberspace API (`api.cyberspace.online` or its RTDB) from tests, scripts or an agent session.
- `lib/core` stays Arduino-free and board-agnostic. Outside `src/hal/`, device code reaches hardware only through `src/hal/board.h` and never includes `src/hal/<board>/` headers.
- Sources, best first: NTP, GPS, RTC. GPS never overrides an NTP sync from the last hour.
- Times (spec §4.2):
  - GPS may set the clock when it is unset or its last sync is over 1 hour old;
  - the RTC is written after an NTP or GPS sync, at most once an hour;
  - the GPS is on while the last sync is over 6 hours old (or the clock is unset), or, in Automatic zone mode, while there is no position from the last 6 hours;
  - a search gives up after 15 minutes and retries 1 hour later.
- A time is plausible only from 2025-01-01T00:00:00Z (`1735689600`) to before 2100-01-01T00:00:00Z (`4102444800`). Nothing else sets the clock.
- Monotonic times on the device come from `esp_timer_get_time() / 1000` (64-bit), never `millis()`, which wraps after 49 days.
- NTP servers: `ntpServer` (default `pool.ntp.org`) first, then `time.google.com`. SNTP restarts on every Wi-Fi station connect.
- The zone is recomputed only on the first fix after boot or after moving more than 10 km. With no zone (open sea): `Etc/GMT±n`, with n = round(lon/15) and the sign inverted.
- Privacy (spec §7): the position is never sent, stored or logged. Logs say only fix yes/no, satellite counts and the zone name. Only the zone name and POSIX string are saved.
- The tz blob is at most 2 MiB (2 097 152 bytes). Each POSIX string is at most 63 bytes (the web form's limit).
- Attribution, exact copy, wherever the spec asks for it: `Time zone boundaries © OpenStreetMap contributors (ODbL), from timezone-boundary-builder.`
- The fake builds (`RC_FAKE_API`) keep their fixed clock: no time task, no RTC or GPS access. They report a canned GPS status.
- Every task commits with its given message, followed by a blank line and `Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>`.
- Verify:
  - `pio test -e native` is green;
  - from Task 1 on, `pio run -e m9`, `-e m9_fake`, `-e wio_l2` and `-e wio_l2_fake` all build (`scripts/build-upload-monitor.sh -B` builds all four).

## Review Focus

1. **A bogus GPS date:** a receiver without a fix, or after a week-number rollover, sends status `V`, or `A` with a year like 2006. Expected: the clock is never set from it (Task 2: `test_rmc_void_not_valid`; Task 3: `test_gps_implausible_ignored`).
2. **Garbage on the UART:** power-on noise, a wrong baud rate, or a line longer than 82 bytes. Expected: the parser drops it and resyncs on the next `$` (Task 2: `test_garbage_and_overlong_resync`).
3. **More than 49 days of uptime:** the 6-hour, 15-minute and 1-hour timers keep working with large monotonic values (Task 3: `test_search_gives_up_and_retries` runs from `nowMs` 5 000 000 000).
4. **Positions at the edges:** latitude ±90, longitude ±180, a point exactly on a shared border, and a blob offset out of range. Expected: one deterministic answer, and no read past the blob (Task 5: `test_lookup_extremes`, `test_lookup_on_edge`, `test_lookup_corrupt_blob`).
5. **The Settings screen in Comfortable size with the setup AP on:** three more status lines must not overwrite the menu. Expected: status lines stop above the menu (Task 7: a flash check).

---

## File Structure

| File | Responsibility |
|---|---|
| `lib/core/src/time/civil.{h,cpp}` | Days ↔ civil date (moved out of `feed_parse.cpp`) |
| `lib/core/src/time/pcf8563.{h,cpp}` | PCF8563 register encode and decode (BCD, VL flag) |
| `lib/core/src/gps/nmea.{h,cpp}` | Streaming NMEA parser → `GpsState` |
| `lib/core/src/time/clock_policy.{h,cpp}` | Clock source, RTC-write and GPS-power decisions |
| `lib/core/src/time/time_status.{h,cpp}` | The `gps`, `time` and `zone` status lines |
| `lib/core/src/tz/tz_lookup.{h,cpp}` | The blob reader, point-in-polygon, ocean fallback, `TzTracker`, `TzMode`, `effectiveTz` |
| `scripts/tzgen.py` | GeoJSON + tzdata → `tz_data.h` |
| `src/tz/tz_data.h` | Generated: the zone blob |
| `test/fixtures/tz_fixture.geojson`, `tz_fixture_posix.json`, `tz_fixture_blob.h` | Three hand-made square zones and their generated blob |
| `src/hal/board.h`, `src/hal/m9/gps_rtc_m9.cpp`, `src/hal/wio_l2/gps_wio_l2.cpp` | GPS power/read and RTC per board |
| `src/app/time_service.{h,cpp}` | The time task: parser, policy, SNTP, RTC, zone; status lines |
| `src/store/settings.{h,cpp}` | `tzMode`, `tzAutoName`, `tzAutoPosix`, `ntpServer` |
| `src/screens/settings.cpp`, `src/web/web_server.cpp`, `src/web/page.h` | Status lines, zone mode, NTP server, attribution |

---

### Task 1: Board GPS and RTC, with civil-date and PCF8563 helpers

**Files:**
- Create: `lib/core/src/time/civil.{h,cpp}`, `lib/core/src/time/pcf8563.{h,cpp}`, `test/test_time_civil/test_main.cpp`
- Create: `src/hal/m9/gps_rtc_m9.cpp`, `src/hal/wio_l2/gps_wio_l2.cpp`
- Modify: `lib/core/src/feed/feed_parse.cpp` (use `civil.h`; delete its private `daysFromCivil`)
- Modify: `src/hal/board.h`, `src/hal/m9/pins_m9.h`, `src/hal/m9/board_m9.cpp` (`begin`), `src/hal/wio_l2/pins_wio_l2.h`, `src/hal/wio_l2/io_pca9555.cpp` (the "no GPS in this firmware" comment)

**Interfaces:**
- Produces, in namespace `rc`:
  ```cpp
  // civil.h — proleptic Gregorian, UTC.
  int64_t daysFromCivil(int64_t y, int m, int d);  // 1970-01-01 → 0
  void civilFromDays(int64_t days, int& y, int& m, int& d);
  // pcf8563.h — regs = registers 0x02..0x08 (seconds … years). Weekday 0 = Sunday.
  void pcf8563Encode(int64_t epochSec, uint8_t regs[7]);  // years 2000–2099, century bit 0
  bool pcf8563Decode(const uint8_t regs[7], int64_t& epochSec);  // false: VL (bit 7 of regs[0]) set, or a field out of range
  ```
- Produces, in `board.h` (namespace `board`):
  ```cpp
  bool hasGps();
  void gpsPower(bool on);                  // on: power, a 10 ms reset pulse, UART up; off: UART down, power off
  size_t gpsRead(uint8_t* buf, size_t n);  // never blocks; 0 while off
  bool rtcRead(int64_t& epochUtc);         // seconds; false with no RTC, an I2C error or the VL flag
  bool rtcWrite(int64_t epochUtc);
  ```
- Board values:

  | | M9 (`pins_m9.h`) | Wio L2 (`pins_wio_l2.h`) |
  |---|---|---|
  | UART | `Serial1`, `GPS_RX = 2`, `GPS_TX = 3`, `GPS_BAUD = 115200` | `Serial1`, `GPS_RX = 18`, `GPS_TX = 17`, `GPS_BAUD = 9600` |
  | Power | `GPS_EN = 11`, LOW = on | `wio::ioSet(EXP_GNSS_POWER, on)` |
  | Reset | `GPS_RST = 5`, HIGH = reset (through an NPN) | `wio::ioSet(EXP_GNSS_RST, …)`, HIGH = reset |
  | RTC | PCF8563 `RTC_ADDR = 0x51` on `Wire1`, `RTC_SDA = 7`, `RTC_SCL = 6`, 100 kHz | none: `rtcRead`/`rtcWrite` return false |

  The M9's `Wire` is the keyboard bus (20/21), so the RTC gets `Wire1`, begun on first use. `M9 board::begin` drives `GPS_EN` HIGH and `GPS_RST` LOW, so the GPS starts off. Call `Serial1.setRxBufferSize(4096)` before `Serial1.begin`.

- [x] **Step 1: Write the failing tests** in `test/test_time_civil/test_main.cpp`:
  ```cpp
  void test_days_from_civil() {
    TEST_ASSERT_EQUAL_INT64(0, daysFromCivil(1970, 1, 1));
    TEST_ASSERT_EQUAL_INT64(20752, daysFromCivil(2026, 10, 26));
    int y, m, d; civilFromDays(daysFromCivil(2028, 2, 29), y, m, d);
    TEST_ASSERT_EQUAL(2028, y); TEST_ASSERT_EQUAL(2, m); TEST_ASSERT_EQUAL(29, d);
  }
  void test_pcf_encode_decode() {
    uint8_t r[7]; pcf8563Encode(1793039400, r);  // Mon 2026-10-26 18:30:00Z
    const uint8_t want[7] = {0x00, 0x30, 0x18, 0x26, 0x01, 0x10, 0x26};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(want, r, 7);
    int64_t e; TEST_ASSERT_TRUE(pcf8563Decode(r, e)); TEST_ASSERT_EQUAL_INT64(1793039400, e);
  }
  void test_pcf_vl_flag() { /* r[0] |= 0x80 → Decode false */ }
  void test_pcf_bad_bcd() { /* month 0x13 → false; seconds 0x5A → false; day 0x00 → false */ }
  ```
  `pcf8563Decode` masks the unused bits: seconds `0x7F`, minutes `0x7F`, hours `0x3F`, days `0x3F`, weekday `0x07`, months `0x1F`.
- [x] **Step 2: Run** `pio test -e native -f test_time_civil`. Expected: it fails to build (no headers).
- [x] **Step 3: Implement** `civil.cpp` (Howard Hinnant's algorithms) and `pcf8563.cpp`. Switch `feed_parse.cpp` to `rc::daysFromCivil`.
- [x] **Step 4: Run** `pio test -e native`. Expected: all pass, `test_feed_parse` included.
- [x] **Step 5: Implement the board functions** in `gps_rtc_m9.cpp` (inside `#if defined(BOARD_M9)`) and `gps_wio_l2.cpp` (inside `#if defined(BOARD_WIO_L2)`). `rtcRead` reads 7 bytes from register 0x02 and decodes them; `rtcWrite` encodes and writes them. Fix the comment in `io_pca9555.cpp` to "GPS off until the time service turns it on".
- [x] **Step 6: Run** `scripts/build-upload-monitor.sh -B`. Expected: all four envs build.
- [x] **Step 7: Commit**
  ```bash
  git add <the Files listed above>
  git commit -m "feat(hal): GPS power and UART, PCF8563 RTC" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
  ```

### Task 2: NMEA parser

**Files:**
- Create: `lib/core/src/gps/nmea.{h,cpp}`, `test/test_nmea/test_main.cpp`

**Interfaces:**
- Produces (namespace `rc`):
  ```cpp
  struct GpsState {
    bool fix = false;         // the latest RMC has status 'A'
    int satsUsed = 0;         // GGA field 7
    int satsInView = 0;       // GSV field 3, summed over talkers (GP, GL, GA, BD, GB, GQ …)
    bool utcValid = false;    // an 'A' RMC with both time and date has been seen
    int64_t utcEpochMs = 0;   // that RMC's UTC
    int64_t utcAtMs = -1;     // monotonic ms when it was parsed
    double lat = 0, lon = 0;  // degrees, south and west negative; valid while fix
    int64_t lastFixMs = -1;   // monotonic ms of the latest 'A' RMC
  };
  class NmeaParser {
   public:
    void feed(const uint8_t* data, size_t n, int64_t nowMs);
    const GpsState& state() const;
    void reset();  // the GPS was powered off: back to GpsState{}
  };
  ```
- Rules:
  - A sentence starts at `$` and ends at `\n` (a `\r` before it is ignored).
  - It needs `*HH`, the XOR of the bytes between `$` and `*`. Otherwise it is dropped.
  - The 2-letter talker is ignored, and the type (`RMC`, `GGA`, `GSV`) is matched. Anything else is ignored.
  - A sentence longer than 82 bytes is dropped, and the parser waits for the next `$`.
  - An RMC with status `V` sets `fix = false` and leaves `utc*` alone.
  - The RMC date `ddmmyy` means year 2000 + yy.
  - GSV: only message 1 of each talker's group updates that talker's count. `satsInView` is the sum of the latest count per talker.

- [x] **Step 1: Write the failing tests.** Add a test helper `std::string nmea(const char* body)` that returns `"$" + body + "*" + hex(XOR) + "\r\n"`.
  ```cpp
  const char* kRmc = "GNRMC,183000.00,A,4042.768,N,07400.360,W,0.0,0.0,261026,,,A";
  void test_rmc_sets_time_and_position() {
    // feed nmea(kRmc) at nowMs 5000:
    // utcValid, utcEpochMs == 1793039400000, utcAtMs == 5000, fix, lastFixMs == 5000,
    // lat within 1e-4 of 40.7128, lon within 1e-4 of -74.006
  }
  void test_rmc_fraction_ms()            // "183000.50" → 1793039400500
  void test_gga_sats_used()              // "GNGGA,183000.00,4042.768,N,07400.360,W,1,07,1.0,10.0,M,-34.0,M,," → satsUsed 7
  void test_gsv_summed_over_talkers()    // GPGSV 1 of 2 (total 08), GPGSV 2 of 2 (08), GLGSV 1 of 1 (03) → 11
  void test_bad_checksum_ignored()       // one hex digit flipped → utcValid false
  void test_rmc_void_not_valid()         // kRmc with 'V' → fix false, utcValid false
  void test_rmc_empty_date_not_valid()   // 'A' with an empty date field → utcValid false
  void test_split_across_reads()         // kRmc fed one byte per call → same state as whole
  void test_unknown_and_talkers()        // "GPTXT,…" leaves the state alone; "BDRMC" parses like GNRMC
  void test_garbage_and_overlong_resync() {
    // 200 bytes of 0xFF, then "$GPXXX" + 120 'A's with no newline, then nmea(kRmc) → utcValid
  }
  void test_reset_clears()               // after reset(): fix false, satsInView 0, utcAtMs -1
  ```
- [x] **Step 2: Run** `pio test -e native -f test_nmea`. Expected: it fails to build.
- [x] **Step 3: Implement.** Use a 96-byte line buffer and split on `,`. Latitude `ddmm.mmmm` and longitude `dddmm.mmmm` become degrees, negated for `S` and `W`. Use `rc::daysFromCivil` for the epoch.
- [x] **Step 4: Run** `pio test -e native -f test_nmea`. Expected: PASS.
- [x] **Step 5: Commit**
  ```bash
  git add <the Files listed above>
  git commit -m "feat(core): NMEA parser for RMC, GGA and GSV" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
  ```

### Task 3: Clock policy and status text

**Files:**
- Create: `lib/core/src/time/clock_policy.{h,cpp}`, `lib/core/src/time/time_status.{h,cpp}`, `test/test_clock_policy/test_main.cpp`

**Interfaces:**
- Consumes: `GpsState` (Task 2).
- Produces (namespace `rc`):
  ```cpp
  enum class TimeSource : uint8_t { None, Rtc, Gps, Ntp };
  constexpr int64_t kMinPlausibleSec = 1735689600, kMaxPlausibleSec = 4102444800;
  struct ClockInputs {
    bool hasGps = false;
    bool autoZone = false;        // zone mode Automatic (Task 6 wires it; false until then)
    bool ntpSynced = false;       // the SNTP callback fired since the last tick
    int64_t systemEpochMs = 0;    // the system clock now
    const GpsState* gps = nullptr;
  };
  struct ClockActions {
    bool setClock = false;  int64_t clockEpochMs = 0;
    bool writeRtc = false;  int64_t rtcEpochSec = 0;
    bool gpsChanged = false; bool gpsOn = false;
  };
  struct ClockStatus {
    TimeSource source = TimeSource::None;
    int64_t lastSyncMs = -1;       // monotonic; NTP or GPS only
    bool gpsOn = false;
    int64_t searchStartMs = -1;
  };
  class ClockPolicy {
   public:
    ClockActions boot(int64_t nowMs, bool rtcOk, int64_t rtcEpochSec);
    ClockActions tick(int64_t nowMs, const ClockInputs& in);
    ClockStatus status() const;
  };
  // time_status.h
  std::string gpsStatusText(bool hasGps, const ClockStatus& c, const GpsState& g, int64_t nowMs);
  std::string timeStatusText(const ClockStatus& c, int64_t nowMs);
  ```
- Decisions:
  - **An RTC boot is not a sync.** It sets the clock (source `Rtc`) but leaves `lastSyncMs` at -1. The GPS therefore still powers on to confirm the time, and GPS may set the clock.
  - **GPS sets the clock** when `gps->utcValid`, its `utcAtMs` is newer than the last sample used, and the epoch is plausible. The clock must be unset, or `lastSyncMs` must be more than 1 h old and NTP must not have synced in the last hour. The value set is `utcEpochMs + (nowMs - utcAtMs)`.
  - **NTP** (`ntpSynced`) sets the source to `Ntp` and the sync time. SNTP has already set the clock, so there is no `setClock`.
  - **The RTC** is written after an NTP or GPS sync if no write happened in the last hour. The value is the synced epoch in seconds: `systemEpochMs / 1000` after NTP.
  - **GPS power:**
    - the GPS needs to be on when the board has GPS and either `lastSyncMs` is -1 or more than 6 h old, or `autoZone` holds and the GPS has no fix (`lastFixMs`) from the last 6 h;
    - it turns on when needed and not inside a retry wait, which starts the search;
    - it turns off once nothing needs it;
    - after 15 min on without meeting the need, it turns off and waits 1 h before searching again;
    - `gpsChanged` is true only on a change.
- Status copy (exact):
  - `gpsStatusText`:
    - `"none"` when the board has no GPS;
    - `"off"`;
    - `"searching m:ss"` (time since `searchStartMs`);
    - `"fix, N of M satellites"` while on with a fix, where N = `satsUsed` and M = `satsInView`.
  - `timeStatusText`:
    - `"not set"`;
    - `"RTC, at boot"`;
    - for NTP or GPS, the age of `lastSyncMs`: `"NTP, just now"` under 1 min, `"GPS, 12 min ago"` under 1 h, `"NTP, 3 h ago"` under 2 days, otherwise `"NTP, 4 d ago"`.

- [x] **Step 1: Write the failing tests.** The helper `GpsState gpsAt(int64_t atMs, int64_t epochMs, bool fix)` sets `utcValid` and `fix`.
  ```cpp
  constexpr int64_t H = 3600000, M = 60000, T = 1793039400000LL;
  void test_boot_from_rtc()           // boot(0,true,1793039400) → setClock, clockEpochMs T, status().source Rtc, lastSyncMs -1
  void test_boot_rtc_implausible()    // rtcOk with 946684800 → no setClock, source None; rtcOk false → none
  void test_gps_sets_unset_clock()    // gps utcAtMs 800, tick at 1000 → clockEpochMs T+200, source Gps, writeRtc, rtcEpochSec (T+200)/1000
  void test_ntp_beats_gps()           // ntpSynced at 0 → source Ntp, writeRtc; a GPS sample at 30*M → no setClock
  void test_gps_after_ntp_stale()     // ntpSynced at 0; a GPS sample at H+1 → setClock, source Gps
  void test_gps_implausible_ignored() // utcEpochMs of 2006-01-01 → no setClock, source None
  void test_rtc_boot_counts_unsynced()// boot from RTC, then tick: gpsOn; a GPS sample → setClock
  void test_rtc_write_throttled()     // GPS sync at 0 (write), NTP at 30*M (no write), NTP at 61*M (write)
  void test_gps_power_on_off()        // hasGps, unset: first tick turns it on; after a GPS sync it turns off; 6*H+1 later, on again
  void test_ntp_turns_gps_off()       // searching; ntpSynced → gpsChanged, gpsOn false
  void test_no_gps_hardware()         // hasGps false → gpsOn never true
  void test_search_gives_up_and_retries() {
    // base = 5000000000 (58 days): on at base; off at base+15*M; still off at base+15*M+59*M;
    // on again at base+15*M+H
  }
  void test_auto_zone_keeps_gps_until_position() {
    // autoZone, NTP synced: GPS on with no fix; off after a fix; on again 6*H+1 after that fix
  }
  void test_status_text() {
    // "none", "off", "searching 3:07" (on for 187 s), "fix, 7 of 11 satellites";
    // "not set", "RTC, at boot", "NTP, just now", "GPS, 12 min ago", "NTP, 3 h ago", "NTP, 4 d ago"
  }
  ```
- [x] **Step 2: Run** `pio test -e native -f test_clock_policy`. Expected: it fails to build.
- [x] **Step 3: Implement** `ClockPolicy` and the two text functions. They have no clocks of their own: every time comes from the arguments.
- [x] **Step 4: Run** `pio test -e native -f test_clock_policy`. Expected: PASS.
- [x] **Step 5: Commit**
  ```bash
  git add <the Files listed above>
  git commit -m "feat(core): clock source and GPS power policy" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
  ```

### Task 4: Time service wiring (RTC at boot, SNTP restart, GPS)

**Files:**
- Create: `src/app/time_service.{h,cpp}`
- Modify: `src/main.cpp`, `src/net/wifi_mgr.cpp`

**Interfaces:**
- Consumes: the board functions (Task 1), `NmeaParser` (Task 2), `ClockPolicy` and the status texts (Task 3).
- Produces (namespace `rc::timesvc`):
  ```cpp
  struct Snapshot { std::string gps, time, zone; };  // ready-made status lines
  void start(const Settings& s);  // applies the TZ, does the RTC boot step, configures SNTP, starts the task
  void restartNtp();              // esp_sntp_stop() if running, then esp_sntp_init()
  void settingsChanged();         // reloads time settings from NVS and reapplies them (used from Task 6)
  Snapshot snapshot();
  ```
- Behaviour:
  - **`start`:**
    1. `setenv("TZ", s.tz)` and `tzset()`;
    2. `board::rtcRead` → `policy.boot(...)` → `settimeofday` when it says so;
    3. `esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL)`, servers 0 = `pool.ntp.org` (a static `std::string`, replaced by `ntpServer` in Task 7) and 1 = `time.google.com`, and `sntp_set_time_sync_notification_cb` setting an `std::atomic<bool>`. SNTP is not initialised here;
    4. the task `"time"`: 4096-byte stack, priority 1, core 0.
  - **The task loop:**
    - every 100 ms, it drains `board::gpsRead` in 256-byte chunks into the parser;
    - every 1000 ms, it ticks the policy and applies the actions: `settimeofday`, `board::rtcWrite` and `board::gpsPower`. Turning the GPS off also calls `parser.reset()`;
    - it rebuilds the snapshot under a `std::mutex`. `zone` is `s.tz + " (manual)"` until Task 6.
  - **Logs:**
    - `[time] set from RTC|GPS`;
    - `[time] NTP sync`;
    - `[time] RTC written`;
    - `[gps] on`;
    - `[gps] off`;
    - `[gps] off: no fix in 15 min, retry in 1 h`;
    - `[gps] fix, N of M satellites`, once each time a fix is gained.
    
    The logs never show positions.
  - **`RC_FAKE_API`:** `start`, `restartNtp` and `settingsChanged` only apply the TZ or do nothing. `snapshot()` returns `{"fix, 7 of 11 satellites", "GPS, 2 min ago", "America/New_York (auto)"}`.
  - **`main.cpp`:** call `timesvc::start(app().cfg)` after `settings::load`, before `startChatSource`.
  - **`wifi_mgr.cpp`:** remove `configTzTime`. Call `timesvc::restartNtp()` where `[wifi] connected` is logged.

- [x] **Step 1: Implement** `time_service.cpp` and the two call sites.
- [x] **Step 2: Run** `scripts/build-upload-monitor.sh -B` and `pio test -e native`. Expected: all four envs build, and every test passes.
- [ ] **Step 3: Flash `m9`** and check:
  - with Wi-Fi in range, `[time] NTP sync` and `[time] RTC written` appear, and `[gps] off` follows;
  - after a reboot with the saved network switched off, the status-bar clock is right at once (`[time] set from RTC`), and `[gps] on` follows;
  - outdoors, `[gps] fix, …` and `[time] set from GPS` appear within 15 min;
  - a Wi-Fi drop and rejoin logs a fresh `[time] NTP sync`.
- [x] **Step 4: Commit**
  ```bash
  git add <the Files listed above>
  git commit -m "feat(time): time service with RTC boot, SNTP restart on connect and GPS" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
  ```

### Task 5: Zone data generator and lookup

**Files:**
- Create: `scripts/tzgen.py`, `src/tz/tz_data.h` (generated)
- Create: `lib/core/src/tz/tz_lookup.{h,cpp}`, `test/test_tz_lookup/test_main.cpp`
- Create: `test/fixtures/tz_fixture.geojson`, `test/fixtures/tz_fixture_posix.json`, `test/fixtures/tz_fixture_blob.h` (generated)
- Modify: `README.md` (an attribution section), `docs/BUILD.md` (how to regenerate)

**Interfaces:**
- **The blob format** is shared by `tzgen.py` and `tz_lookup.cpp`. It is little-endian, and readers use `memcpy` because fields are unaligned. Every offset is absolute.
  - Header, 36 bytes:

    | Bytes | Field |
    |---|---|
    | 0–3 | `"RCTZ"` |
    | 4–5 | u16 version = 1 |
    | 6–7 | u16 zoneCount |
    | 8–11 | u32 zonesOff |
    | 12–15 | u32 gridOff |
    | 16–23 | char boundaryRelease[8], NUL-padded (`"2026e"`) |
    | 24–31 | char tzdataVersion[8] |
    | 32–35 | u32 toleranceMicroDeg |

  - Zone record (16 bytes, at `zonesOff + 16*i`):
    - u32 nameOff and u32 posixOff, each pointing to a NUL-terminated string;
    - u32 ringsOff;
    - u32 ringCount.
  - Ring:
    - a 24-byte header: i32 minLon, minLat, maxLon, maxLat (µdeg), u32 pointCount, u32 dataBytes;
    - then pointCount pairs of zigzag LEB128 varints (dlon, dlat) in µdeg, each the delta from the previous point. The first point's delta is from (0, 0).
    - Rings are not closed explicitly (the last point joins the first). A zone's outer rings and holes are listed alike, and the even-odd rule decides.
  - Grid: 360×180 u32 cells. The cell index is `(clamp(floor(lat),-90,89)+90)*360 + clamp(floor(lon),-180,179)+180`. Each cell holds:
    - `0xFFFFFFFF` for no zone;
    - `0x80000000 | zoneIndex` when the cell lies wholly inside one zone;
    - otherwise the offset of `{u16 n, u16 zoneIndex[n]}`.
- **Produces** (namespace `rc`):
  ```cpp
  enum class TzMode : uint8_t { Automatic, Manual };
  const char* tzModeKey(TzMode);                  // "auto" | "manual"
  bool parseTzMode(std::string_view, TzMode& out);
  struct TzZone { std::string name, posix; };
  bool tzBlobValid(const uint8_t* blob, size_t size);  // magic, version, offsets in range
  // True when inside a zone polygon. Otherwise false, with out = oceanZone(lon); that includes an invalid blob.
  bool tzLookup(const uint8_t* blob, size_t size, double lat, double lon, TzZone& out);
  TzZone oceanZone(double lon);                       // n = clamp(round(lon/15), -12, 12)
  double distanceKm(double lat1, double lon1, double lat2, double lon2);  // haversine, R = 6371
  class TzTracker {
   public:
    // True when it looked the zone up: on the first call, or more than 10 km from the last lookup.
    bool update(const uint8_t* blob, size_t size, double lat, double lon, TzZone& out);
    void reset();
  };
  // Automatic with a known autoPosix → autoPosix; otherwise manualTz.
  const std::string& effectiveTz(TzMode mode, const std::string& manualTz, const std::string& autoPosix);
  ```
  - `oceanZone`:
    - n > 0 gives `{"Etc/GMT-n", "<+NN>-n"}`;
    - n < 0 gives `{"Etc/GMT+|n|", "<-NN>|n|"}`;
    - n = 0 gives `{"Etc/GMT", "GMT0"}`.
    
    NN is two digits.
- **`tzgen.py`** (Python 3 standard library only, run with `python3 -I`):
  ```
  python3 -I scripts/tzgen.py SOURCE [--zoneinfo DIR] [--posix MAP.json] [--release TAG]
      [--tolerance DEG] [--no-simplify] [--name kTzData] [--namespace rc] > OUT.h
  ```
  - **Input:**
    - `SOURCE` is `timezones.geojson.zip` (the no-oceans release, chosen so zone names stay true) or a `.geojson`;
    - `--zoneinfo` defaults to `/usr/share/zoneinfo`, and the tzdata version comes from `+VERSION` in it;
    - `--posix` maps zone → POSIX string instead (for the fixture);
    - `--tolerance` defaults to `0.01`.
  - **Steps:**
    1. Read each zone's POSIX string from the last line of its TZif file.
    2. Simplify every ring with Douglas–Peucker (iterative, not recursive) and drop rings with fewer than 3 points.
    3. Quantise to µdeg.
    4. Build the grid:
       - rasterise every ring edge into the 1° cells it touches;
       - for each row, find the zones containing the cell centres with a scanline (bucket each edge into the rows whose centre latitude `k+0.5` it spans, then sort the crossings per zone);
       - an edge-free cell is direct (or empty);
       - a cell with edges lists the zones with edges in it, plus the zone holding its centre.
    5. Write the blob.
  - **Output:** a header comment with the command line, the source release, the tzdata version, the tolerance and the attribution line. Then `#pragma once`, `<cstddef>`, `<cstdint>`, and in the namespace:
    - `alignas(4) static const uint8_t NAME[] =` the blob as concatenated string literals of 1 KiB of data each, using 3-digit octal escapes for anything outside printable ASCII and escaping `\`, `"` and `?`;
    - `constexpr size_t NAMESize = sizeof(NAME) - 1;` (so `kTzDataSize`, `kTzFixtureSize`).
  - **Summary:** it prints `zones N, rings R, points P, blob B bytes` to stderr.
  - **Exit 1:**
    - when a zone is missing from zoneinfo (it lists them and suggests `--zoneinfo` pointing at a newer tzdata, for example the `tzdata` pip package's `zoneinfo` folder);
    - when a POSIX string is over 63 bytes;
    - when the blob is over 2 097 152 bytes (it suggests a larger `--tolerance`).
- **The fixture zones** (`tz_fixture.geojson` and `tz_fixture_posix.json`):

  | Zone | Area | POSIX |
  |---|---|---|
  | `Test/West` | lon −10…−5, lat 40…45 | `<-01>1` |
  | `Test/East` | lon −5…0, lat 40…45 | `<+02>-2` |
  | `Test/Hole` | lon 10…20, lat −10…0, with a hole at lon 14…16, lat −6…−4 | `<+05>-5` |

  Generate the blob with `--no-simplify --release test --name kTzFixture --namespace fx`.

- [x] **Step 1: Write the fixture GeoJSON, the POSIX map and `tzgen.py`.** Generate `test/fixtures/tz_fixture_blob.h`.
- [x] **Step 2: Write the failing tests** in `test/test_tz_lookup/test_main.cpp`, including `../fixtures/tz_fixture_blob.h`:
  ```cpp
  void test_blob_valid()        // tzBlobValid(fixture) true; a copy with a bad magic → false
  void test_lookup_inside()     // (42.5,-7.5) → true, "Test/West", "<-01>1"; (42.5,-2.5) → "Test/East"
  void test_lookup_outside()    // (50,-7.5) → false, "Etc/GMT", "GMT0"
  void test_lookup_on_edge()    // (42.5,-5.0) → "Test/East" (half-open crossing rule)
  void test_lookup_hole()       // (-5,15) → false, "Etc/GMT-1", "<+01>-1"; (-2,11) → "Test/Hole"
  void test_ocean_both_hemispheres() {
    // oceanZone(-75) → "Etc/GMT+5", "<-05>5"; oceanZone(135) → "Etc/GMT-9", "<+09>-9";
    // oceanZone(180) → "Etc/GMT-12", "<+12>-12"; oceanZone(-180) → "Etc/GMT+12", "<-12>12"
  }
  void test_lookup_extremes()   // lat ±90, lon ±180 on the fixture: no crash, oceanZone(lon)
  void test_lookup_corrupt_blob() // a copy with one zone's ringsOff = 0xFFFFFF00 → false, no crash (run under ASan if available)
  void test_tracker_10km() {
    // first update → true; 5 km away → false (out unchanged); 11 km from the first → true; reset() → the next update is true
  }
  void test_effective_tz_and_mode_keys() // Automatic+"" → manual; Automatic+"<+02>-2" → it; Manual → manual; parseTzMode("auto"/"manual"/"x")
  ```
- [x] **Step 3: Run** `pio test -e native -f test_tz_lookup`. Expected: it fails to build.
- [x] **Step 4: Implement `tz_lookup.cpp`.**
  - Use the grid to find the candidate zones, and skip any ring whose bounding box excludes the point. That is safe under even-odd.
  - The crossing test uses int64 µdeg maths: an edge counts when `(yi > y) != (yj > y)` and `x < xi + (xj - xi) * (y - yi) / (yj - yi)`.
  - The first zone with an odd count wins.
  - Bounds-check every offset and every varint read against `size`. An out-of-range read counts as "not in this zone".
- [x] **Step 5: Run** `pio test -e native -f test_tz_lookup`. Expected: PASS.
- [x] **Step 6: Generate the real data.**
  - Download `timezones.geojson.zip` from timezone-boundary-builder release `2026e` outside the repo (like the Terminus fonts).
  - Run `python3 -I scripts/tzgen.py <zip> --release 2026e > src/tz/tz_data.h`.
  - If the blob is over 2 MiB, retry with `--tolerance 0.015`, then `0.02`. Record the final command in the header and in `docs/BUILD.md`.
- [x] **Step 7: Add a real-data sanity test** `test_real_data` that includes `../../src/tz/tz_data.h` and checks these points. Then run `pio test -e native`. Expected: all pass.

  | Place | Position | Expected |
  |---|---|---|
  | New York | (40.7128, −74.006) | `America/New_York`, `EST5EDT,M3.2.0,M11.1.0` |
  | London | (51.5074, −0.1278) | `Europe/London` |
  | Paris | (48.8566, 2.3522) | `Europe/Paris` |
  | Tokyo | (35.6762, 139.6503) | `Asia/Tokyo` |
  | Sydney | (−33.8688, 151.2093) | `Australia/Sydney` |
  | Mid-Atlantic | (30, −40) | `Etc/GMT+3` |
- [x] **Step 8: Add the README attribution section**, "Time zone data", with the attribution line and links to timezone-boundary-builder and the ODbL. Then run `scripts/build-upload-monitor.sh -B`. Expected: all four envs build; the M9 flash usage is under 100%.
- [x] **Step 9: Commit**
  ```bash
  git add <the Files listed above>
  git commit -m "feat(tz): tzgen.py, bundled zone boundaries and offline lookup" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
  ```

### Task 6: Automatic zone in the time service

**Files:**
- Modify: `src/store/settings.{h,cpp}`, `src/app/time_service.cpp`, `lib/core/src/time/time_status.{h,cpp}`, `test/test_clock_policy/test_main.cpp`

**Interfaces:**
- Consumes: `TzMode`, `tzLookup`, `TzTracker`, `effectiveTz` and `kTzData`/`kTzDataSize` (Task 5); `ClockInputs::autoZone` (Task 3).
- Produces:
  - `Settings` gains `TzMode tzMode = TzMode::Manual` and `std::string tzAutoName, tzAutoPosix`.
  - `settings::load` reads the NVS keys `tzmode` (uchar: 0 = Automatic, 1 = Manual), `tzaname` and `tzaposix`. Without `tzmode`, the default is Automatic when `board::hasGps()`, otherwise Manual. On a board without GPS, Automatic is forced to Manual.
  - New savers:
    ```cpp
    void saveTzMode(rc::TzMode);
    void saveTzAuto(const std::string& name, const std::string& posix);  // only called when the zone changes
    ```
  - In `time_status.h`:
    ```cpp
    std::string zoneStatusText(TzMode mode, const std::string& autoName, const std::string& manualTz);
    // Manual → "<manualTz> (manual)"; Automatic with a name → "<name> (auto)";
    // Automatic without → "<manualTz> (auto, no fix yet)"
    ```
- Time service:
  - `start` applies `effectiveTz(...)`;
  - each tick passes `autoZone = tzMode == Automatic`;
  - on each fix in Automatic mode, it calls `tracker.update(kTzData, …)`. When that returns true with a new name, it saves with `saveTzAuto`, then calls `setenv` and `tzset`, and logs `[tz] zone <name>`;
  - `settingsChanged()` reloads the mode and zone, reapplies `effectiveTz`, and resets the tracker when the mode becomes Automatic;
  - the snapshot's `zone` comes from `zoneStatusText`.

- [x] **Step 1: Write the failing test** `test_zone_status_text` with the three cases above (`"America/New_York (auto)"`, `"UTC0 (manual)"`, `"UTC0 (auto, no fix yet)"`).
- [x] **Step 2: Run** `pio test -e native -f test_clock_policy`. Expected: it fails to build.
- [x] **Step 3: Implement** `zoneStatusText`, the settings fields and savers, and the time-service changes.
- [x] **Step 4: Run** `pio test -e native` and `scripts/build-upload-monitor.sh -B`. Expected: all pass, and all four envs build.
- [ ] **Step 5: Flash `m9`** and check outdoors with Wi-Fi off:
  - `[tz] zone <name>` matches the location;
  - the status-bar clock shows local time;
  - after a reboot, local time is right before any fix (the zone was saved);
  - `[gps] off` follows once both the time and the position are fresh;
  - no log line shows coordinates.
- [x] **Step 6: Commit**
  ```bash
  git add <the Files listed above>
  git commit -m "feat(time): automatic time zone from the GPS position" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
  ```

### Task 7: NTP server, Settings screen, web config and backup keys

**Files:**
- Modify: `lib/core/src/web/web_util.{h,cpp}`, `test/test_keymap_web/test_main.cpp`
- Modify: `src/store/settings.{h,cpp}`, `src/app/time_service.cpp`, `src/screens/settings.cpp`, `src/web/web_server.cpp`, `src/web/page.h`
- Modify, if the backup codec exists: `lib/core/src/config/backup_yaml.{h,cpp}`, `test/test_backup_yaml/test_main.cpp`

**Interfaces:**
- Consumes: `timesvc::snapshot()` and `settingsChanged()` (Tasks 4 and 6); `parseTzMode` and `tzModeKey` (Task 5).
- Produces:
  ```cpp
  bool validNtpServer(std::string_view host);  // 1–63 chars, each [A-Za-z0-9.-]
  ```
  - `Settings::ntpServer = "pool.ntp.org"` (NVS key `ntp`), with `settings::saveNtpServer(const std::string&)`.
  - The time service uses it as SNTP server 0. When it changes, `settingsChanged()` restarts SNTP if Wi-Fi is connected.
- **The Settings screen:**
  - The `gps`, `time` and `zone` field lines come from `timesvc::snapshot()`, after the setup-AP block.
  - `field()` draws only while `r < bottom - kItems`, so status lines never reach the menu.
- **`/api/status`** gains the strings `gps`, `time` and `zone`.
- **`/api/prefs`:**
  - GET adds `tzMode` (`"auto"`/`"manual"`), `ntpServer` and `hasGps`.
  - POST accepts `tzMode`, `ntpServer` and `tz`. The errors (exact copy):
    - `"Zone mode: auto or manual"`;
    - `"This board has no GPS"` (for auto without GPS);
    - `"NTP server: a host name, 1-63 characters"`;
    - for `tz`, the existing message.
  - It saves what changed, then calls `timesvc::settingsChanged()`. That replaces the direct `setenv`/`tzset`.
- **`page.h`:**
  - The Status table gets the rows `gps`, `time` and `zone`.
  - A new `<h2>Time</h2>` section follows Display. It holds:
    - `Time zone` with `<select id="tzmode">` (`auto` = "Automatic, from GPS", which is hidden when `!hasGps`; `manual` = "Manual");
    - the existing `tzpick`/`tz` controls, moved here, with a dim note "Used until GPS finds the zone" shown in auto mode;
    - `NTP server <input id="ntp">`;
    - `<button onclick="saveTime()">Save time</button>` and `<div id="timemsg" class="msg">`.
  - `savePrefs()` no longer sends `tz`. `saveTime()` posts `{tzMode, tz, ntpServer}`.
  - Before `<script>`: `<footer class="dim">` with the attribution line.
- **Backup:**
  - If `lib/core/src/config/backup_yaml.cpp` exists, add `tzMode: auto|manual` (after `timezone`) and `ntpServer: "…"`. Missing keys keep the current value, and a bad `ntpServer` is an error.
  - If it doesn't exist yet, add both keys to the §3 example in `docs/superpowers/specs/2026-10-26-cyberspace-backup-design.md` and its plan's codec task instead.

- [x] **Step 1: Write the failing tests:**
  ```cpp
  void test_valid_ntp_server() {
    TEST_ASSERT_TRUE(validNtpServer("pool.ntp.org")); TEST_ASSERT_TRUE(validNtpServer("time-a.nist.gov"));
    TEST_ASSERT_TRUE(validNtpServer(std::string(63, 'a')));
    TEST_ASSERT_FALSE(validNtpServer("")); TEST_ASSERT_FALSE(validNtpServer(std::string(64, 'a')));
    TEST_ASSERT_FALSE(validNtpServer("ntp server")); TEST_ASSERT_FALSE(validNtpServer("ntp:123"));
    TEST_ASSERT_FALSE(validNtpServer("a/b")); TEST_ASSERT_FALSE(validNtpServer("\xc3\xa9.org"));
  }
  // With the backup codec:
  void test_backup_time_keys() // round trip of tzMode Manual + ntpServer "time.example.org";
                               // a backup without them keeps the current values; ntpServer "a b" → error
  ```
- [x] **Step 2: Run** `pio test -e native -f test_keymap_web` (and `-f test_backup_yaml`). Expected: FAIL.
- [x] **Step 3: Implement** `validNtpServer`, the setting, the screen, the web changes and the backup keys.
- [x] **Step 4: Run** `pio test -e native` and `scripts/build-upload-monitor.sh -B`. Expected: all pass, and all four envs build.
- [ ] **Step 5: Flash `m9`** and check:
  - the Settings screen shows `gps`, `time` and `zone` in both text sizes, and with the setup AP on in Comfortable size nothing overlaps the menu;
  - the web Status shows the same three lines;
  - switching the zone mode to Manual applies the picked TZ at once;
  - a bad NTP server shows the error;
  - a good one (`time.cloudflare.com`) logs `[time] NTP sync` after a Wi-Fi rejoin;
  - the footer shows the attribution.
  
  On `m9_fake`, Settings shows the canned lines.
- [x] **Step 6: Commit**
  ```bash
  git add <the Files listed above>
  git commit -m "feat(time): NTP server, time status in Settings and web config, backup keys" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
  ```

### Task 8: Documentation

**Files:**
- Modify: `docs/USE.md`, `docs/TESTING.md`, `AGENTS.md`

- [x] **Step 1: `USE.md`.**
  - Add a "Time and time zone" section covering:
    - the sources (NTP, GPS, RTC);
    - the GPS powering itself off;
    - Automatic and Manual zone mode;
    - the NTP server field;
    - the three Settings lines;
    - the privacy note from spec §7.
  - Add the attribution line under Web config.
- [x] **Step 2: `TESTING.md`.** Under "Open hardware checks", add the Task 4, 6 and 7 flash checks as unticked items:
  - the clock is right after a reboot with Wi-Fi off (RTC, M9);
  - an outdoor GPS fix with Wi-Fi off sets the clock;
  - the zone matches the location;
  - the GPS turns off afterwards;
  - the NTP server field works;
  - the same GPS checks on the Wio L2.
  
  Don't change the user's other edits there.
- [x] **Step 3: `AGENTS.md`.**
  - Add a "GPS time" line pointing at the spec and this plan.
  - Note `scripts/tzgen.py` next to the font-generation note.
  - Update the progress section (task status and the native test count from `pio test -e native`).
- [x] **Step 4: Commit**
  ```bash
  git add <the Files listed above>
  git commit -m "docs: GPS time, RTC and time zone guide and checks" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
  ```
