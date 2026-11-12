# SPDX-License-Identifier: GPL-3.0-or-later
"""tools/pr_guidelines.py: dependency pinning rules and the PR checks end to end.

  python3 -m unittest discover -s tools -p 'test_*.py'
"""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))
import pr_guidelines  # noqa: E402

# Built from pieces so this file never trips the checks it tests.
API_HOST = "api." + "cyberspace" + ".online"
KEY_BLOCK = "-----BEGIN " + "PRIVATE KEY-----"
SPDX = "// SPDX-License-" + "Identifier: GPL-3.0-or-later\n"


class PinningTests(unittest.TestCase):
    def test_pinned(self):
        for spec in [
            "bblanchon/ArduinoJson@7.4.3",
            "lvgl/lvgl@9.5.0",
            "https://github.com/pschatzmann/arduino-audio-driver/archive/v0.3.0.zip",
            "https://github.com/pioarduino/platform-espressif32/releases/download/55.03.312/platform-espressif32.zip",
            "https://github.com/foo/bar/archive/1c817c2f27aa4593e07d0f7e13b3dbf8d980bcae.zip",
            "Foo=https://github.com/foo/bar/archive/refs/tags/v1.0.zip",
            "https://github.com/foo/bar.git#v1.2.3",
            "${env.lib_deps}",
        ]:
            self.assertIsNone(pr_guidelines.pin_problem(spec), spec)

    def test_unpinned(self):
        for spec in [
            "bblanchon/ArduinoJson",
            "bblanchon/ArduinoJson@^7.0.0",
            "lovyan03/LovyanGFX@~1.2.35",
            "lvgl/lvgl@>=9",
            "https://github.com/foo/bar.git",
            "https://github.com/foo/bar.git#main",
            "https://github.com/foo/bar/archive/refs/heads/main.zip",
            "https://github.com/foo/bar/archive/master.zip",
        ]:
            self.assertIsNotNone(pr_guidelines.pin_problem(spec), spec)

    def test_ini_parsing(self):
        ini = (
            "[env:a]\n"
            "platform  = espressif32@7.0.1\n"
            "lib_deps =\n"
            "    ; a comment line\n"
            "    # another comment\n"
            "    bblanchon/ArduinoJson@^7.0.0   ; trailing comment\n"
            "    lvgl/lvgl@9.5.0\n"
            "build_flags =\n"
            "    -DFOO=1\n"
            "[env:b]\n"
            "lib_deps = ricmoo/QRCode\n"
        )
        bad = pr_guidelines.unpinned_dependencies(ini)
        self.assertEqual([(n, s) for n, s, _ in bad],
                         [(6, "bblanchon/ArduinoJson@^7.0.0"), (11, "ricmoo/QRCode")])

    def test_repo_ini_is_pinned(self):
        text = (TOOLS.parent / "platformio.ini").read_text(encoding="utf-8")
        self.assertEqual(pr_guidelines.unpinned_dependencies(
            text, pr_guidelines.local_libs(str(TOOLS.parent))), [])

    def test_local_lib_needs_no_pin(self):
        ini = "[lvgl]\nlib_deps =\n    lvgl/lvgl@9.5.0\n    lvui\n"
        self.assertEqual(pr_guidelines.unpinned_dependencies(ini, {"lvui"}), [])
        self.assertEqual(len(pr_guidelines.unpinned_dependencies(ini)), 1)

    def test_spdx_detection(self):
        self.assertTrue(pr_guidelines.has_spdx("#!/bin/sh\n# SPDX-License-Identifier: MIT\n"))
        self.assertFalse(pr_guidelines.has_spdx("// nothing here\nint x;\n"))
        self.assertFalse(pr_guidelines.has_spdx("\n" * 5 + SPDX))
        self.assertTrue(pr_guidelines.needs_spdx("src/foo.cpp"))
        self.assertTrue(pr_guidelines.needs_spdx("site/app.mjs"))
        self.assertFalse(pr_guidelines.needs_spdx("src/ui/fonts/cell8x16.h"))
        self.assertFalse(pr_guidelines.needs_spdx("docs/USE.md"))

    def test_board_includes(self):
        text = '#include "hal/board.h"\n#include "hal/m9/pins_m9.h"\n# include <hal/tdeck/x.h>\n'
        self.assertEqual(pr_guidelines.board_includes(text), [2, 3])


class EndToEndTests(unittest.TestCase):
    """A scratch repo with a base commit and a PR commit on top."""

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="monster guidelines ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "tools").mkdir()
        shutil.copyfile(TOOLS / "pr_guidelines.py", self.root / "tools" / "pr_guidelines.py")
        for d in ("src/hal/m9", "src/screens", "src/grid_ui", "scripts"):
            (self.root / d).mkdir(parents=True)
        (self.root / "platformio.ini").write_text("[env:a]\nplatform = native@1.2.1\n")
        (self.root / "src/screens/home.cpp").write_text(SPDX + '#include "hal/board.h"\n')
        (self.root / "src/grid_ui/old.cpp").write_text("int old;\n")
        self.git("init", "-q", "-b", "main")
        self.commit("base", signoff=True)
        self.base = self.git("rev-parse", "HEAD").strip()

    def git(self, *args):
        env = dict(os.environ, GIT_AUTHOR_NAME="T", GIT_AUTHOR_EMAIL="t@example.com",
                   GIT_COMMITTER_NAME="T", GIT_COMMITTER_EMAIL="t@example.com")
        return subprocess.run(["git", "-C", str(self.root), *args], check=True,
                              capture_output=True, text=True, env=env).stdout

    def commit(self, msg, signoff):
        self.git("add", "-A")
        self.git("commit", "-q", "--no-gpg-sign", *(["-s"] if signoff else []), "-m", msg)

    def check(self):
        env = {k: v for k, v in os.environ.items()
               if k not in ("GITHUB_ACTIONS", "GITHUB_STEP_SUMMARY")}
        return subprocess.run([sys.executable, str(self.root / "tools" / "pr_guidelines.py"),
                               self.base, "HEAD"], capture_output=True, text=True, env=env)

    def assertFails(self, needle):
        r = self.check()
        self.assertEqual(r.returncode, 1, r.stdout)
        self.assertIn(needle, r.stdout)

    def test_clean_pr_passes(self):
        (self.root / "src/hal/m9/new.h").write_text(SPDX + '#include "hal/m9/pins.h"\n')
        (self.root / "scripts/doc.sh").write_text("# SPDX-License-Identifier: MIT\n# see https://" + API_HOST + "/docs.md\n")
        self.commit("add files", signoff=True)
        r = self.check()
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("OK", r.stdout)

    def test_missing_signoff_fails(self):
        (self.root / "README").write_text("x\n")
        self.commit("unsigned change", signoff=False)
        self.assertFails("unsigned change")

    def test_new_file_without_spdx_fails(self):
        (self.root / "src/bare.cpp").write_text("int x;\n")
        self.commit("bare file", signoff=True)
        self.assertFails("src/bare.cpp")

    def test_existing_file_without_spdx_is_not_checked(self):
        (self.root / "src/screens/home.cpp").write_text('#include "hal/board.h"\nint y;\n')
        self.commit("drop header from existing file", signoff=True)
        self.assertEqual(self.check().returncode, 0)

    def test_unpinned_dependency_fails(self):
        (self.root / "platformio.ini").write_text("[env:a]\nplatform = native\n")
        self.commit("float platform", signoff=True)
        self.assertFails("line 2")

    def test_board_header_outside_hal_fails(self):
        (self.root / "src/screens/home.cpp").write_text(SPDX + '#include "hal/m9/pins_m9.h"\n')
        self.commit("reach into the m9", signoff=True)
        self.assertFails("src/screens/home.cpp:2")

    def test_grid_ui_change_fails(self):
        (self.root / "src/grid_ui/old.cpp").write_text("int changed;\n")
        self.commit("touch frozen ui", signoff=True)
        self.assertFails("src/grid_ui/old.cpp")

    def test_private_key_fails(self):
        (self.root / "signing.pem").write_text("x\n")
        (self.root / "src/hal/m9/k.h").write_text(SPDX + "// " + KEY_BLOCK + "\n")
        self.commit("keys", signoff=True)
        r = self.check()
        self.assertEqual(r.returncode, 1, r.stdout)
        self.assertIn("signing.pem", r.stdout)
        self.assertIn("src/hal/m9/k.h (private key block)", r.stdout)

    def test_live_api_in_script_fails(self):
        (self.root / "scripts/poll.sh").write_text(
            "# SPDX-License-Identifier: MIT\ncurl https://" + API_HOST + "/v1/posts\n")
        self.commit("poll the api", signoff=True)
        self.assertFails("scripts/poll.sh")


if __name__ == "__main__":
    unittest.main()
