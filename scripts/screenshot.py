#!/usr/bin/env python3
"""Screenshots from a running Monster for Cyberspace device, over its serial console.

Sends "screenshot" and saves the frame on screen as a PNG. It reads the frame
buffer only: nothing on the device or the network is touched.

  scripts/screenshot.py PORT                 one shot into screenshots/
  scripts/screenshot.py PORT --watch         a shot each time the screen changes
                                             and settles (Ctrl-C to stop)
  scripts/screenshot.py PORT -o DIR --name mesh-deck

Needs pyserial; PlatformIO's Python has it (the python beside the pio executable).
The port opens with DTR and RTS low, so boards on a USB-serial chip don't reset.
"""
import argparse
import hashlib
import os
import struct
import sys
import time
import zlib

try:
    import serial
except ImportError:
    sys.exit("needs pyserial: pip install pyserial, or run with the python beside pio")


def open_port(port, baud):
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, baud, 0.5
    s.dtr = False
    s.rts = False
    s.open()
    return s


def read_line(s, deadline):
    buf = bytearray()
    while time.time() < deadline:
        b = s.read(1)
        if not b:
            continue
        if b == b"\n":
            return bytes(buf)
        buf += b
    return None


def read_exact(s, n, deadline):
    buf = bytearray()
    while len(buf) < n and time.time() < deadline:
        buf += s.read(n - len(buf))
    return bytes(buf) if len(buf) == n else None


def grab(s, timeout=30.0):
    """Returns (w, h, rgb565 bytes) or None."""
    s.reset_input_buffer()
    s.write(b"\nscreenshot\n")
    deadline = time.time() + timeout
    while True:
        line = read_line(s, deadline)
        if line is None:
            return None
        if line.startswith(b"@@SHOT "):
            break
    w, h = (int(v) for v in line.split()[1:3])
    want, raw, pixels = w * h, bytearray(), bytearray()
    got = 0
    while got < want:
        t = read_exact(s, 3, deadline)
        if t is None:
            return None
        raw += t
        run = t[0]
        if run == 0 or got + run > want:
            return None
        pixels += t[1:3] * run
        got += run
    tail = read_line(s, deadline)  # the newline before @@END
    end = read_line(s, deadline)
    if tail != b"" or not end or not end.startswith(b"@@END "):
        return None
    _, size, crc = end.split()
    if int(size) != len(raw) or int(crc, 16) != zlib.crc32(raw):
        return None
    return w, h, bytes(pixels)


def write_png(path, w, h, px565):
    rows = bytearray()
    for y in range(h):
        rows.append(0)
        for (v,) in struct.iter_unpack("<H", px565[y * w * 2:(y + 1) * w * 2]):
            r, g, b = (v >> 11) & 0x1F, (v >> 5) & 0x3F, v & 0x1F
            rows += bytes(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(bytes(rows), 9)))
        f.write(chunk(b"IEND", b""))


def grab_retry(s, tries=3):
    for _ in range(tries):
        shot = grab(s)
        if shot:
            return shot
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port")
    ap.add_argument("-o", "--out", default="screenshots")
    ap.add_argument("--name", default="screen", help="file name prefix")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--watch", action="store_true", help="save each new screen once it settles")
    ap.add_argument("--interval", type=float, default=1.0, help="seconds between polls with --watch")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    s = open_port(a.port, a.baud)
    n = 1

    def save(shot):
        nonlocal n
        while os.path.exists(os.path.join(a.out, f"{a.name}-{n:02d}.png")):
            n += 1
        path = os.path.join(a.out, f"{a.name}-{n:02d}.png")
        write_png(path, *shot)
        print(path, flush=True)

    if not a.watch:
        shot = grab_retry(s)
        if not shot:
            sys.exit("no screenshot: is the firmware running and the port right?")
        save(shot)
        return
    saved, last = set(), None
    try:
        while True:
            shot = grab_retry(s)
            if shot:
                digest = hashlib.sha1(shot[2]).digest()
                if digest == last and digest not in saved:
                    save(shot)
                    saved.add(digest)
                last = digest
            time.sleep(a.interval)
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
