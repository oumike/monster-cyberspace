# Rhino Cyberspace — agent instructions

Standalone ESP32 firmware client for [cyberspace.online](https://cyberspace.online),
a retro text-only social network. Milestone 1 is cIRC chat plus a device-served
web config page, on the Elecrow ThinkNode M9. More boards and Cyberspace
features come later.

- **Design spec:** `docs/superpowers/specs/2026-10-25-cyberspace-m9-circ-design.md`
- **Implementation plan:** `docs/superpowers/plans/2026-10-25-cyberspace-m9-circ.md`.
  Work its tasks in order, tick each `- [ ]` box as you finish a step, and
  commit once per task. The plan's "Global Constraints" apply to every task.

## Hard rules

- **Never call the live Cyberspace API** (`api.cyberspace.online` or its
  Firebase RTDB) from tests, scripts, or an AI agent session. The API terms ban
  bots, scraping, and LLM-driven agents. Use the fixtures and the `m9_fake`
  build. A human runs the live checklist in `docs/TESTING.md`.
- Every write the firmware makes must come from a user's key press. No
  automation, and no automatic retries of sends.
- **No captive portal.** The setup AP (`Rhino-Cyberspace-<BOARD>-Setup`) is
  open unless a password is configured in web config. The page is served at
  `http://192.168.4.1`, with no DNS hijacking and nothing that pops up.
- Never log or store passwords. Never log tokens.
- **Long-term idea, not to be built yet:** a LoRa gateway that bridges cIRC to
  other devices. It needs the Cyberspace operator's permission first (spec §12).

## Commands

```sh
pio test -e native          # host unit tests (lib/core)
pio run -e m9               # firmware
pio run -e m9_fake          # firmware with the scripted fake API (no account needed)
scripts/build-upload-monitor.sh m9   # flash + serial monitor (115200)
```

## Layout

`lib/core/` holds portable, Arduino-free logic with native tests in `test/`.
`src/` holds the M9 HAL, networking, tasks, screens, and the web server. Hardware
references live in the sibling projects `../camillia/camillia-mt/src/hal/hw_m9.h`
and `../rhino-moshi/firmware/`.

## Progress (hand-off from Claude Code, 2026-10-25)

Work is on branch **`m1-circ`**. Tasks 1–4 of the plan are done and committed,
with their boxes ticked in the plan. **Next: Task 5 (message model, chat store, mentions).** The native
suite is green: 17/17 (`pio test -e native`), and `pio run -e m9` and
`-e m9_fake` both build.

Notes for whoever continues:
- Each task follows test-driven development: write the plan's tests first, run
  them and watch them fail, implement, run them green, then commit with the
  plan's commit message.
- **Task 13:** add `-Isrc/ui/fonts` to the `native` env's `build_flags` so the
  header-only `cell_font.h` compiles in the glyph tests.
- If the first `pio run` fails with
  `FileNotFoundError: 'package-postinstall.py'` while installing
  `esptoolpy`, run it again. It's a one-off PlatformIO install glitch.
- The design mockups the user approved are described in spec §5–§6. The
  original HTML lives in the git-ignored `.superpowers/brainstorm/` folder.
