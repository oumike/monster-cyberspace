# Adds an `upload_erase` target: erase the whole flash and upload in one
# esptool session (an erase run separately makes the board reset in between).
Import("env")

from SCons.Script import COMMAND_LINE_TARGETS

if "upload_erase" in COMMAND_LINE_TARGETS:
    upload_flags = list(env.get("UPLOADERFLAGS", []))
    # esptool v5 (pioarduino) spells it write-flash; v4 used write_flash.
    for command in ("write-flash", "write_flash"):
        if command in upload_flags:
            write_flash_index = upload_flags.index(command)
            break
    else:
        raise RuntimeError("upload_erase requires esptool write-flash")

    upload_flags.insert(write_flash_index + 1, "--erase-all")
    env.Replace(UPLOADERFLAGS=upload_flags)

env.AddCustomTarget(
    name="upload_erase",
    dependencies="upload",
    actions=None,
    title="Erase Flash and Upload",
    description="Erase the flash and upload in one esptool session",
)
