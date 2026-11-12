#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check a pull request against the CONTRIBUTING.md rules a machine can check.

  python3 tools/pr_guidelines.py BASE HEAD

BASE and HEAD are commits (the PR's base and head); the checks look at what
BASE..HEAD adds or changes. Any finding fails the run (exit 1).

  sign-off   every non-merge commit carries a Signed-off-by line
  spdx       every source file the PR adds starts with an
             SPDX-License-Identifier line, any license
  pinning    every platform and lib_deps entry in platformio.ini names an
             exact version, commit, tag or release. The whole file is
             checked, not only the PR's lines; the project's own lib/<name>
             libraries need no pin
  hal        no file outside src/hal/ that the PR touches includes a board's
             own header (src/hal/<board>/...); go through src/hal/board.h
  grid-ui    the PR leaves the frozen src/grid_ui/ alone
  secrets    the PR adds no .pem/.key file and no private key block
  live-api   scripts, tools, the site and workflows the PR touches never name
             the Cyberspace API or its database hosts (the API docs page is
             fine to link)

Run under GitHub Actions it also writes ::error:: annotations and a step
summary.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SOURCE_EXTS = (".c", ".cpp", ".h", ".hpp", ".py", ".sh", ".js", ".mjs")
# Generated or third-party trees whose files carry their own terms.
SPDX_EXEMPT_DIRS = ("src/ui/fonts/", "managed_components/", "boards/", "dict/")
SPDX_RE = re.compile(r"SPDX-License-Identifier:\s*\S")
SPDX_LINES = 5

SHA_RE = re.compile(r"\b[0-9a-f]{40}\b")
EXACT_VERSION_RE = re.compile(r"^v?\d+(\.\d+)*([-+][0-9A-Za-z.\-]+)?$")

BOARD_INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"](?:src/)?hal/[A-Za-z0-9_]+/', re.M)
CODE_EXTS = (".c", ".cpp", ".h", ".hpp")

KEY_BLOCK_RE = re.compile(r"-----BEGIN [A-Z ]*PRIVATE KEY-----")
SECRET_FILE_EXTS = (".pem", ".key")

# Hosts no script may talk to (AGENTS.md: never call the live API).
LIVE_API_RE = re.compile(
    r"api\.cyberspace\.online(?!/docs)|firebasedatabase\.app|firebaseio\.com")
LIVE_API_DIRS = ("scripts/", "tools/", "site/", ".github/")


def git(*args):
    return subprocess.run(["git", "-C", ROOT, *args], check=True,
                          capture_output=True, text=True).stdout


def changed_files(base, head, flt="AM"):
    return git("diff", "--name-only", f"--diff-filter={flt}",
               f"{base}...{head}").split()


def added_lines(base, head, path):
    """The lines BASE...HEAD adds to PATH."""
    diff = git("diff", "--unified=0", f"{base}...{head}", "--", path)
    return [l[1:] for l in diff.splitlines()
            if l.startswith("+") and not l.startswith("+++")]


# ---- sign-off ---------------------------------------------------------------

def commits_missing_signoff(base, head):
    out = git("log", "--no-merges", "--format=%H%x1f%an%x1f%s%x1f%B%x1e",
              f"{base}..{head}")
    missing = []
    for rec in out.split("\x1e"):
        rec = rec.strip("\n")
        if not rec:
            continue
        sha, author, subject, body = rec.split("\x1f", 3)
        if author.endswith("[bot]"):
            continue
        if not re.search(r"^Signed-off-by: .+ <[^>]+>\s*$", body, re.M):
            missing.append((sha[:10], subject))
    return missing


# ---- SPDX headers -----------------------------------------------------------

def needs_spdx(path):
    return path.endswith(SOURCE_EXTS) and not path.startswith(SPDX_EXEMPT_DIRS)


def has_spdx(text):
    return any(SPDX_RE.search(line) for line in text.splitlines()[:SPDX_LINES])


def added_files_missing_spdx(base, head):
    return [p for p in changed_files(base, head, "A")
            if needs_spdx(p) and not has_spdx(git("show", f"{head}:{p}"))]


# ---- dependency pinning -----------------------------------------------------

def pin_problem(spec):
    """Why a platform/lib_deps entry is not pinned, or None when it is."""
    spec = spec.strip()
    if spec.startswith("${"):
        return None                      # a reference to another section
    if "=" in spec and "://" in spec.split("=", 1)[1]:
        spec = spec.split("=", 1)[1].strip()   # "Name=https://..." form
    if "://" in spec:
        if SHA_RE.search(spec):
            return None
        if "/releases/download/" in spec or "/archive/refs/tags/" in spec:
            return None
        if re.search(r"/archive/v?\d[^/]*\.(zip|tar\.gz)$", spec):
            return None
        m = re.search(r"#(.+)$", spec)
        if m and EXACT_VERSION_RE.match(m.group(1)):
            return None
        return "URL is not pinned to a commit, tag or release"
    if "@" not in spec:
        return "no version given"
    version = spec.rsplit("@", 1)[1].strip()
    if not EXACT_VERSION_RE.match(version):
        return f"version '{version}' is a range, not an exact version"
    return None


def ini_dependencies(text):
    """(line number, entry) for every platform and lib_deps value."""
    out, in_deps = [], False
    for n, raw in enumerate(text.splitlines(), 1):
        line = re.split(r"\s[;#]|^[;#]", raw, maxsplit=1)[0].rstrip()
        if not line.strip():
            continue
        key = re.match(r"^([A-Za-z_]+)\s*=\s*(.*)$", line)
        if key:
            in_deps = key.group(1) == "lib_deps"
            if key.group(1) == "platform" and key.group(2):
                out.append((n, key.group(2).strip()))
            elif in_deps and key.group(2):
                out.append((n, key.group(2).strip()))
            continue
        if line.startswith("["):
            in_deps = False
            continue
        if in_deps and line[0].isspace():
            out.append((n, line.strip()))
    return out


def local_libs(root=ROOT):
    """Names of the project's own libraries (lib/<name>), which need no pin."""
    lib = os.path.join(root, "lib")
    return {d for d in os.listdir(lib) if os.path.isdir(os.path.join(lib, d))} \
        if os.path.isdir(lib) else set()


def unpinned_dependencies(text, local=()):
    return [(n, spec, why) for n, spec in ini_dependencies(text)
            if spec not in local and (why := pin_problem(spec))]


# ---- project rules (AGENTS.md) ----------------------------------------------

def board_includes(text):
    """Line numbers that include a board's own HAL header."""
    return [text.count("\n", 0, m.start()) + 1
            for m in BOARD_INCLUDE_RE.finditer(text)]


def hal_boundary_breaks(base, head):
    out = []
    for path in changed_files(base, head):
        if path.startswith("src/hal/") or not path.endswith(CODE_EXTS):
            continue
        for n in board_includes(git("show", f"{head}:{path}")):
            out.append((path, n))
    return out


def grid_ui_changes(base, head):
    return [p for p in changed_files(base, head, "ACDMR")
            if p.startswith("src/grid_ui/")]


def secret_additions(base, head):
    out = []
    for path in changed_files(base, head, "A"):
        if path.endswith(SECRET_FILE_EXTS):
            out.append((path, "key or certificate file"))
    for path in changed_files(base, head):
        if path.endswith(SECRET_FILE_EXTS):
            continue
        if any(KEY_BLOCK_RE.search(l) for l in added_lines(base, head, path)):
            out.append((path, "private key block"))
    return out


def live_api_mentions(base, head):
    out = []
    for path in changed_files(base, head):
        if not path.startswith(LIVE_API_DIRS):
            continue
        for line in added_lines(base, head, path):
            if LIVE_API_RE.search(line):
                out.append((path, line.strip()))
    return out


# ---- reporting --------------------------------------------------------------

def annotate(msg, path=None, line=None):
    if os.environ.get("GITHUB_ACTIONS") != "true":
        return
    loc = ""
    if path:
        loc = f" file={path}" + (f",line={line}" if line else "")
    msg = msg.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")
    print(f"::error{loc}::{msg}")


class Report:
    def __init__(self):
        self.errors = 0
        self.summary = ["## Contribution guidelines", ""]

    def section(self, name, items, ok_text, bad_text, describe):
        """describe(item) -> (console line, summary line, annotation, path, line)."""
        print(f"{name}: {len(items)} finding(s)")
        if not items:
            self.summary += [f"**{name}:** {ok_text}", ""]
            return
        self.errors += len(items)
        self.summary.append(f"**{name}:** {bad_text}")
        for item in items:
            console, summary, note, path, line = describe(item)
            print(f"  {console}")
            self.summary.append(f"- {summary}")
            annotate(note, path, line)
        self.summary.append("")


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2
    base, head = argv[1], argv[2]
    r = Report()

    r.section("Sign-off", commits_missing_signoff(base, head),
              "every commit is signed off.",
              "these commits have no `Signed-off-by` line (fix with "
              f"`git rebase --signoff {base[:10]}`):",
              lambda c: (f"{c[0]} {c[1]}", f"`{c[0]}` {c[1]}",
                         f"Commit {c[0]} ({c[1]}) has no Signed-off-by line. "
                         "Sign off with git commit -s; see CONTRIBUTING.md.",
                         None, None))

    r.section("SPDX", added_files_missing_spdx(base, head),
              "every new source file has a license header.",
              f"these new files have no `SPDX-License-Identifier` line in "
              f"their first {SPDX_LINES} lines:",
              lambda p: (p, f"`{p}`",
                         "New file has no SPDX-License-Identifier header "
                         "(e.g. // SPDX-License-Identifier: GPL-3.0-or-later).",
                         p, 1))

    with open(os.path.join(ROOT, "platformio.ini"), encoding="utf-8") as fh:
        unpinned = unpinned_dependencies(fh.read(), local_libs())
    r.section("Pinning", unpinned, "every dependency is pinned.",
              "these `platformio.ini` entries are not pinned to an exact "
              "version:",
              lambda d: (f"line {d[0]}: {d[1]} ({d[2]})",
                         f"line {d[0]}: `{d[1]}` ({d[2]})",
                         f"{d[1]}: {d[2]}. Pin an exact version or commit.",
                         "platformio.ini", d[0]))

    r.section("HAL boundary", hal_boundary_breaks(base, head),
              "no board header is included outside `src/hal/`.",
              "these files include a board's own header; go through "
              "`src/hal/board.h`:",
              lambda b: (f"{b[0]}:{b[1]}", f"`{b[0]}` line {b[1]}",
                         "Board header included outside src/hal/. Use "
                         "src/hal/board.h (AGENTS.md).", b[0], b[1]))

    r.section("Grid UI", grid_ui_changes(base, head),
              "`src/grid_ui/` is untouched.",
              "`src/grid_ui/` is a frozen copy for the X4; these files "
              "changed:",
              lambda p: (p, f"`{p}`",
                         "src/grid_ui/ is frozen; never modify it (AGENTS.md).",
                         p, None))

    r.section("Secrets", secret_additions(base, head),
              "no keys added.",
              "these look like private keys; never commit them:",
              lambda s: (f"{s[0]} ({s[1]})", f"`{s[0]}` ({s[1]})",
                         f"Possible secret: {s[1]}. Never commit keys.",
                         s[0], None))

    r.section("Live API", live_api_mentions(base, head),
              "no script names the live Cyberspace API.",
              "scripts must never call the live API (use the test "
              "fixtures); these lines name it:",
              lambda m: (f"{m[0]}: {m[1]}", f"`{m[0]}`: `{m[1][:80]}`",
                         "Never call the live Cyberspace API from scripts, "
                         "tools, the site or CI (AGENTS.md).", m[0], None))

    path = os.environ.get("GITHUB_STEP_SUMMARY")
    if path:
        with open(path, "a", encoding="utf-8") as fh:
            fh.write("\n".join(r.summary) + "\n")

    print("FAILED" if r.errors else "OK")
    return 1 if r.errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
