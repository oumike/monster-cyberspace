#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Draws icon.png, the CardputerZero launcher's 100x100 icon: the monster as
# 20x20 pixel art at 5x on the terminal green of the Default theme, and
# store/icon.png, the app store's 256x256 one (12x, 8 px clear border).
# Standard library only; run it again after changing ART.
import os
import struct
import zlib

ART = [
    "....................",
    "...#............#...",
    "...##..........##...",
    "....############....",
    "...##############...",
    "..################..",
    "..###oo######oo###..",
    "..###o@o####o@o###..",
    "..###oo######oo###..",
    "..################..",
    "..################..",
    "..###w#w#w#w#w#w##..",
    "..###.#.#.#.#.#.##..",
    "..###w#w#w#w#w#w##..",
    "..################..",
    "...##############...",
    "...#.##.####.##.#...",
    "...#..#..##..#..#...",
    "....................",
    "....................",
]
COLOURS = {
    ".": (0, 0, 0, 0),
    "#": (0x33, 0xff, 0x66, 255),
    "o": (255, 255, 255, 255),
    "@": (0, 0, 0, 255),
    "w": (255, 255, 255, 255),
}


def chunk(kind, data):
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))


def png(rows, w, h):
    raw = b"".join(b"\0" + bytes(c for px in row for c in px) for row in rows)
    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9))
            + chunk(b"IEND", b""))


def draw(scale, border, out):
    side = len(ART[0]) * scale + 2 * border
    clear = [COLOURS["."]] * side
    rows = [clear] * border
    for line in ART:
        row = [COLOURS[ch] for ch in line for _ in range(scale)]
        rows += [[COLOURS["."]] * border + row + [COLOURS["."]] * border] * scale
    rows += [clear] * border
    with open(out, "wb") as fh:
        fh.write(png(rows, side, side))
    print(out)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    draw(5, 0, os.path.join(here, "icon.png"))
    draw(12, 8, os.path.join(here, "store", "icon.png"))


if __name__ == "__main__":
    main()
