# Contributing to Monster for Cyberspace

Thanks for your interest. Monster for Cyberspace is a hobby project, open by
design, and contributions are welcome: code, board ports, bug reports and
testing on hardware.

Before anything else, read [AGENTS.md](AGENTS.md). It holds the project's hard
rules, the build and test commands, and the layout. It applies to people and
coding agents alike.

## License of contributions

Monster for Cyberspace is licensed under **GPL-3.0-or-later** (see
[LICENSE](LICENSE)). By submitting a contribution you agree that it is licensed
under the same terms (inbound = outbound). This keeps every build and fork open.

Sign off your commits to certify that you wrote the change, or have the right to
submit it, per the [Developer Certificate of Origin](https://developercertificate.org/):

```
git commit -s -m "..."     # adds a Signed-off-by line
```

New source files should carry an SPDX header:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
```

Code taken from another project keeps its original license header. Don't
relicense other people's code; only your own additions are GPL.

## The Cyberspace API

cyberspace.online is a separate service with its own terms, and they ban bots,
scraping and LLM-driven agents. So:

- **Never call the live API** (`api.cyberspace.online` or its Firebase
  database) from tests, scripts, CI or an AI tool. Use the host-test fixtures in
  `test/fixtures/`. A human runs the live checklist in
  [docs/TESTING.md](docs/TESTING.md) on a real device.
- **Every write comes from a key press.** The firmware never posts, sends,
  follows or marks anything on its own, and never retries a send by itself.
- Reading the API docs (<https://api.cyberspace.online/docs.md>) is fine, and
  is the place to check what an endpoint takes.

## How we work

- **One topic per pull request.** Small, focused changes that do one thing are
  easy to review and easy to revert. Open a separate PR per concern.
- **Discuss next steps, keep the change scoped.** It's great to note what could
  come next in the PR description; just don't bundle it into the same diff.
- **Prefer pulling in a library over hand-rolling.** If a maintained library
  solves it, depend on it. Pin it to an exact version or commit in
  `platformio.ini`, and credit it in the README's License section.
- **Don't regress what ships.** The firmware runs on a dozen boards built from
  the same sources, so a change for one board can break another. Keep
  Cyberspace logic board-agnostic and every environment building.
- **Say what you tested.** State which boards you built, which you flashed, and
  what you could not test on hardware.

## Coding standards

Match the code around you. In general:

- **Style.** Two-space indents, opening braces on the same line, `camelCase`
  for functions and variables, `PascalCase` for types, `kName` for constants,
  `name_` for members, `gName` for file-scope globals, `UPPER_CASE` for macros.
  Put file-local code in an anonymous namespace.
- **Where code goes.** Portable logic lives in `lib/core` (no Arduino, with host
  tests); the LVGL views in `lib/lvui` (no `board.h`, no Arduino). Hardware is
  reached outside `src/hal/` only through `src/hal/board.h`: never include a
  `src/hal/<board>/` header elsewhere, and never hard-code a panel size, pin or
  key code in shared code. Use the feature flags in
  `lib/core/src/feature_flags.h` (`RC_FEAT_*`) rather than board checks.
- **`src/grid_ui/` is frozen.** It is the old grid UI that only the Xteink X4
  builds. Don't modify it.
- **Board code.** Put pins and bus addresses in the board's own `src/hal/<board>/`
  headers. Never guess a pin map: take it from a schematic, the vendor's
  examples or a working firmware, and say which in a comment.
- **Untrusted input.** Everything that arrives from the network, the web
  config page or an SD card is untrusted. Bounds-check every length, cap what
  you keep, and never let a malformed response crash the device.
- **Memory.** These are small devices, and some have no PSRAM (the Cardputer
  ADV, the X4). A TLS handshake needs tens of KB of internal RAM. Avoid large
  stack arrays in tasks, don't grow caches without checking the lean builds
  (`RC_LOW_MEM`, `RC_LV_LEAN`), and say in the PR how static RAM changed.
- **Settings.** Saved settings must survive an update. Give new NVS keys a safe
  default, never reuse or renumber an old one (themes are saved by index, so
  append new ones), and keep device Settings, web config and the backup file
  (`lib/core/src/config/`) in step.
- **Secrets.** Never log passwords or tokens. Backup files are the one place
  that deliberately holds them. Never commit `ota_signing_key.pem` or any other
  key.
- **Comments.** Explain *why*, not what: the hardware quirk, the API behaviour
  being matched, the bug a check prevents. Link the issue or the upstream source
  where one exists.
- **Dependencies.** Pin exact versions or commits. No floating `@^1` ranges or
  branch names.
- **Warnings.** Don't add compiler warnings, and don't silence one without a
  comment saying why it's safe.
- **Tests.** Logic that can run off the device (parsing, layout, policy
  decisions) belongs in `lib/core` with a host test in `test/`. Write the test
  first and watch it fail.
- **Docs.** A change users can see updates [docs/USE.md](docs/USE.md), and adds
  an item to the open hardware checks in [docs/TESTING.md](docs/TESTING.md)
  when it needs a device to verify.

## Using AI

AI tools are welcome here; much of this project was written with one. The same
rules apply to AI-assisted code as to any other, plus a few more:

- **Never point an AI tool at the live API.** See above. The API's terms ban
  LLM-driven agents; an agent session may read the docs, never call the API.
- **You are the author.** If you submit it, you are responsible for it. Read
  and understand every line before opening a PR, and be ready to explain and
  change it in review. "The AI wrote it" is not an answer to a review comment.
- **Say so.** Mention in the PR when a substantial part was written by an AI
  tool. A `Co-authored-by` line in the commit is fine.
- **Evidence, not claims.** Don't say something works on hardware unless you
  ran it there, and say what you saw (a serial log, a screenshot, a photo).
  Built, flashed and untested are different things; say which.
- **No invented facts.** AI tools make up pin numbers, register addresses, API
  fields and library calls that look right. Check each against a datasheet,
  schematic, the API docs, the upstream source or the library's headers, and
  cite it.
- **Keep it scoped.** AI tools like to refactor, rename and reformat code they
  were not asked to touch. Trim that out before submitting; a PR should contain
  only the change it describes.
- **Mind the license.** Don't paste in code an AI reproduced from another
  project unless you know where it came from and its license is compatible with
  GPL-3.0, and credit it.
- **No secrets.** Don't give an AI tool your Cyberspace password or tokens,
  Wi-Fi passwords, the OTA signing key or someone else's personal data, and
  check generated logs, backups and configs for them before committing.

The project's own AI working instructions live in [AGENTS.md](AGENTS.md) and
[.github/copilot-instructions.md](.github/copilot-instructions.md).

## Building and testing

See [docs/BUILD.md](docs/BUILD.md) for building and flashing, and
[docs/TESTING.md](docs/TESTING.md) for the tests and hardware checks. Before
opening a PR, run the host tests:

```
pio test -e native
pio test -e native_lv
node --test site/*.test.mjs          # if you touched the flasher site
python3 -m unittest discover -s tools -p 'test_*.py'
```

and build the boards you touched (`pio run -e <env>`), or all of them with
`scripts/build-upload-monitor.sh -B`.

CI builds every released environment and runs the tests on each push to a pull
request, and a PR must have a passing Build before it can be merged. The Build
also checks a PR against the rules above that a machine can check, and fails
on any of these:

- a commit without a sign-off;
- a new source file without an SPDX header;
- a dependency without an exact version;
- a board header included outside `src/hal/`;
- a change under `src/grid_ui/`;
- an added private key or `.pem`/`.key` file;
- a script, tool, workflow or site file that names the live API (linking its
  docs is fine).

Run the same check locally before pushing:

```
python3 tools/pr_guidelines.py origin/main HEAD
```

## Pull requests and releases

Open pull requests against `main`. Only the maintainer and the project's GitHub
Actions push to `main` directly; every other change arrives as a PR.
Releases are cut by the maintainer, and the release notes are written from the
PR description. So describe the user-facing effect of each change, and call out
anything users need to know, such as a settings change or a USB reflash.

## Reporting issues

Bug reports and feature ideas are welcome in the
[issue tracker](https://github.com/oumike/monster-cyberspace/issues). Please
include your board, firmware version and steps to reproduce, and a serial log
if you have one (it never contains passwords or tokens).
