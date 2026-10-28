# Injects RC_VERSION into the build from the VERSION file (as platy-aprs and
# camillia-mt do). Python rather than a `!echo $(cat VERSION)` build flag so the
# build also works from Windows cmd.exe. A missing VERSION leaves
# core_version.h's default. The leading 'v' is dropped: the firmware adds it.
Import("env")  # noqa: F821  - injected by PlatformIO/SCons

import os

path = os.path.join(env.subst("$PROJECT_DIR"), "VERSION")  # noqa: F821
try:
    with open(path, "r", encoding="utf-8") as fh:
        version = fh.read().strip()
except OSError:
    version = ""
if version.startswith("v"):
    version = version[1:]
if version:
    env.Append(CPPDEFINES=[("RC_VERSION", env.StringifyMacro(version))])  # noqa: F821
    print("[set_app_version] RC_VERSION=%s" % version)
else:
    print("[set_app_version] VERSION file missing or empty; using core_version.h's default")
