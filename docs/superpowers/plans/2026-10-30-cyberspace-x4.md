# Xteink X4 — implementation plan

Spec: `docs/superpowers/specs/2026-10-30-cyberspace-x4-design.md`. Branch `x4`.

## Global constraints

- TDD for `lib/core`: tests first, run only the affected suite
  (`pio test -e native -f test_<name>`), then commit once per task.
- No firmware builds unless the user asks (the spec's "Building" rule).
  Nothing is pushed or released without the user's go-ahead.
- Never call the live API. Every write comes from a key press, except X4
  web compose (spec §4.4).
- Boards with PSRAM keep today's paths: the no-PSRAM code is chosen at
  compile time (`BOARD_HAS_PSRAM`) or falls back only when PSRAM is absent.

## Rulings made while planning

- **Like:** `CsApi` has no like or reaction call and the repo documents no
  endpoint for one; the live API can't be probed. Like is deferred and the
  hold-Confirm menu leaves it out until an endpoint is known.
- **Notifications:** built from the endpoints we already use (unread chat
  mentions, feed notifications), not a new API.

## Tasks

- [x] 1. `input/keymap_x4`: ADC ladder → button, debounce and hold → `KeyEvent`
  (Up/Down/Left/Right/Select/Back; held Confirm → Palette; held Back → Home;
  power tap → ScreenOff, power hold → PowerTap). Test `test_x4_input`.
- [x] 2. Settings: `phrases` (≤10 × ≤80 bytes), `webConfig` (on),
  `idleSecs` (300), `wakeMins` (15); portable phrase helpers; NVS and backup
  YAML. Tests in `test_backup_yaml`, `test_phrases`.
- [x] 3. `power/power_policy`: idle-on-battery → sleep, timed wake, critical
  battery → off. Test `test_power_policy`.
- [x] 4. Canned picker (`ui/widgets/canned_picker`): phrases then the tray
  emoji; screens open it instead of a text field when `!board::canType()`;
  free-text-only actions hidden. Test `test_canned_picker`.
- [x] 5. Web config switch: `web/web_access` rule (AP up, or enabled), the
  web server, device Settings, page. Test in `test_web_access`.
- [x] 6. No-PSRAM: `sys/mem` (SPIRAM with internal fallback), `sys/tasks`
  (core pin clamped to the cores present), renderer 1-bit frame, smaller
  buffers, no spell dictionary, heap logging.
- [x] 7. `board::canType()`, `board::webCompose()` on every HAL.
- [x] 8. Web compose (X4): `/api/compose` + page section; target parsing in
  `lib/core` (`web/compose_req`). Test `test_compose_req`.
- [x] 9. X4 HAL `src/hal/x4/`, env `x4`, release/flash scripts, CI.
- [x] 10. `src/app/power_service.cpp`: idle sleep, timed wake, power off.
- [x] 11. Notifications screen and Home entry.
- [x] 12. Docs: AGENTS.md, BUILD.md, USE.md, TESTING.md.
