#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Draws icon.png, the CardputerZero launcher's 100x100 icon: the monster as
# 20x20 pixel art at 5x on the terminal green of the Default theme. Standard
# library only; run it again after changing ART.
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
SCALE = 5


def chunk(kind, data):
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))


def png(rows, w, h):
    raw = b"".join(b"\0" + bytes(c for px in row for c in px) for row in rows)
    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9))
            + chunk(b"IEND", b""))


def main():
    rows = []
    for line in ART:
        row = [COLOURS[ch] for ch in line for _ in range(SCALE)]
        rows += [row] * SCALE
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "icon.png")
    with open(out, "wb") as fh:
        fh.write(png(rows, len(rows[0]), len(rows)))
    print(out)


if __name__ == "__main__":
    main()
