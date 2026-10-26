#!/usr/bin/env python3
"""Build the offline time-zone blob (zone polygons, POSIX strings, 1-degree grid)
as a C++ header. Standard library only; run with `python3 -I`.

    python3 -I scripts/tzgen.py SOURCE [--zoneinfo DIR] [--posix MAP.json]
        [--release TAG] [--tolerance DEG] [--no-simplify] [--name kTzData]
        [--namespace rc] > OUT.h

SOURCE is timezone-boundary-builder's timezones.geojson.zip (the no-oceans
release) or a .geojson file. The blob format is documented in
lib/core/src/tz/tz_lookup.h and read by tz_lookup.cpp.
"""

import argparse
import bisect
import json
import math
import os
import shlex
import struct
import sys
import zipfile

ATTRIBUTION = ("Time zone boundaries \u00a9 OpenStreetMap contributors (ODbL), "
               "from timezone-boundary-builder.")
MAX_BLOB = 2097152
MAX_POSIX = 63
HEADER_SIZE = 36
ZONE_SIZE = 16
RING_HEADER_SIZE = 24
GRID_W, GRID_H = 360, 180
EMPTY_CELL = 0xFFFFFFFF
DIRECT_CELL = 0x80000000
UDEG = 1000000
CHUNK = 1024


def fail(msg):
    print("tzgen: " + msg, file=sys.stderr)
    sys.exit(1)


def load_geojson(path):
    if path.endswith(".zip"):
        with zipfile.ZipFile(path) as z:
            names = [n for n in z.namelist() if n.endswith((".json", ".geojson"))]
            if not names:
                fail("no .json or .geojson file in " + path)
            with z.open(names[0]) as f:
                return json.load(f)
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def zone_rings(features):
    """{tzid: [ring, ...]} where a ring is a list of (lon, lat) floats, holes included."""
    zones = {}
    for feat in features:
        tzid = feat["properties"]["tzid"]
        geom = feat["geometry"]
        if geom["type"] == "Polygon":
            polys = [geom["coordinates"]]
        elif geom["type"] == "MultiPolygon":
            polys = geom["coordinates"]
        else:
            fail("unsupported geometry %s for %s" % (geom["type"], tzid))
        rings = zones.setdefault(tzid, [])
        for poly in polys:
            for ring in poly:
                rings.append([(float(p[0]), float(p[1])) for p in ring])
    return zones


def read_posix(zoneinfo, name):
    """The POSIX TZ string on the last line of a TZif v2+ file, or None."""
    try:
        with open(os.path.join(zoneinfo, name), "rb") as f:
            data = f.read()
    except OSError:
        return None
    if len(data) < 5 or data[:4] != b"TZif" or data[4:5] not in (b"2", b"3", b"4"):
        return None
    lines = data.rstrip(b"\n").rsplit(b"\n", 1)
    if len(lines) != 2:
        return None
    return lines[1].decode("ascii")


def douglas_peucker(pts, tol):
    """Iterative Douglas-Peucker; keeps the first and last points."""
    n = len(pts)
    if n < 3:
        return list(pts)
    tol2 = tol * tol
    keep = bytearray(n)
    keep[0] = keep[n - 1] = 1
    stack = [(0, n - 1)]
    while stack:
        a, b = stack.pop()
        if b - a < 2:
            continue
        ax, ay = pts[a]
        bx, by = pts[b]
        dx, dy = bx - ax, by - ay
        len2 = dx * dx + dy * dy
        best, besti = -1.0, -1
        for i in range(a + 1, b):
            px, py = pts[i]
            if len2 == 0.0:
                d2 = (px - ax) ** 2 + (py - ay) ** 2
            else:
                t = ((px - ax) * dx + (py - ay) * dy) / len2
                if t < 0.0:
                    t = 0.0
                elif t > 1.0:
                    t = 1.0
                ex, ey = ax + t * dx - px, ay + t * dy - py
                d2 = ex * ex + ey * ey
            if d2 > best:
                best, besti = d2, i
        if best > tol2:
            keep[besti] = 1
            stack.append((a, besti))
            stack.append((besti, b))
    return [p for p, k in zip(pts, keep) if k]


def quantise(ring):
    """µdeg integer points, consecutive duplicates and the closing point removed."""
    out = []
    for lon, lat in ring:
        p = (int(round(lon * UDEG)), int(round(lat * UDEG)))
        if not out or out[-1] != p:
            out.append(p)
    while len(out) > 1 and out[-1] == out[0]:
        out.pop()
    return out


def varint(v, buf):
    z = (v << 1) ^ (v >> 63)
    z &= 0xFFFFFFFFFFFFFFFF
    while True:
        b = z & 0x7F
        z >>= 7
        if z:
            buf.append(b | 0x80)
        else:
            buf.append(b)
            return


def encode_ring(ring):
    xs = [p[0] for p in ring]
    ys = [p[1] for p in ring]
    data = bytearray()
    px = py = 0
    for x, y in ring:
        varint(x - px, data)
        varint(y - py, data)
        px, py = x, y
    head = struct.pack("<iiiiII", min(xs), min(ys), max(xs), max(ys), len(ring), len(data))
    return head + bytes(data)


def cell_col(x):
    return min(max(x // UDEG, -180), 179) + 180


def cell_row(y):
    return min(max(y // UDEG, -90), 89) + 90


def edge_cells(x0, y0, x1, y1):
    """Every 1-degree cell a segment touches (a closed-segment supercover)."""
    c0, c1 = cell_col(x0), cell_col(x1)
    r0, r1 = cell_row(y0), cell_row(y1)
    if c0 == c1 or r0 == r1:
        for r in range(min(r0, r1), max(r0, r1) + 1):
            for c in range(min(c0, c1), max(c0, c1) + 1):
                yield r * GRID_W + c
        return
    if x0 > x1:
        x0, y0, x1, y1 = x1, y1, x0, y0
        c0, c1 = c1, c0
    for c in range(c0, c1 + 1):
        lo = max(x0, (c - 180) * UDEG)
        hi = min(x1, (c - 179) * UDEG)
        ya = y0 + (y1 - y0) * (lo - x0) / (x1 - x0)
        yb = y0 + (y1 - y0) * (hi - x0) / (x1 - x0)
        ra = cell_row(math.floor(min(ya, yb)))
        rb = cell_row(math.floor(max(ya, yb)))
        for r in range(ra, rb + 1):
            yield r * GRID_W + c


def ceil_div(a, b):
    return -((-a) // b)


def build_grid(zones):
    """zones: list of rings (µdeg) per zone index. Returns per-cell zone lists:
    (edge_zones: {cell: set}, centre_zones: [set per cell])."""
    edge_zones = {}
    rows = [dict() for _ in range(GRID_H)]  # row -> {zone: [crossing x]}
    half = UDEG // 2
    for zi, rings in enumerate(zones):
        for ring in rings:
            n = len(ring)
            for i in range(n):
                x0, y0 = ring[i]
                x1, y1 = ring[(i + 1) % n]
                for cell in edge_cells(x0, y0, x1, y1):
                    s = edge_zones.get(cell)
                    if s is None:
                        edge_zones[cell] = {zi}
                    else:
                        s.add(zi)
                if y0 == y1:
                    continue
                ymin, ymax = (y0, y1) if y0 < y1 else (y1, y0)
                # Rows whose centre latitude yc satisfies ymin <= yc < ymax.
                kmin = max(ceil_div(ymin - half, UDEG), -90)
                kmax = min(ceil_div(ymax - half, UDEG) - 1, 89)
                for k in range(kmin, kmax + 1):
                    yc = k * UDEG + half
                    xc = x0 + (x1 - x0) * (yc - y0) / (y1 - y0)
                    rows[k + 90].setdefault(zi, []).append(xc)
    centre_zones = [None] * (GRID_W * GRID_H)
    for r, row in enumerate(rows):
        for zi, xs in row.items():
            xs.sort()
            # A centre cx is inside when an odd number of crossings is <= cx.
            for i in range(0, len(xs) - 1, 2):
                a, b = xs[i], xs[i + 1]
                cmin = max(math.ceil((a - half) / UDEG), -180)
                cmax = min(math.ceil((b - half) / UDEG) - 1, 179)
                for c in range(cmin, cmax + 1):
                    cell = r * GRID_W + c + 180
                    if centre_zones[cell] is None:
                        centre_zones[cell] = {zi}
                    else:
                        centre_zones[cell].add(zi)
    return edge_zones, centre_zones


def c_literal(blob):
    out = []
    for i in range(0, len(blob), CHUNK):
        parts = ['    "']
        for b in blob[i:i + CHUNK]:
            if b == 0x5C:
                parts.append("\\\\")
            elif b == 0x22:
                parts.append('\\"')
            elif b == 0x3F:
                parts.append("\\?")
            elif 0x20 <= b < 0x7F:
                parts.append(chr(b))
            else:
                parts.append("\\%03o" % b)
        parts.append('"')
        out.append("".join(parts))
    if not out:
        out.append('    ""')
    return "\n".join(out)


def display_path(p):
    ap = os.path.abspath(p)
    cwd = os.getcwd()
    if ap.startswith(cwd + os.sep):
        return os.path.relpath(ap, cwd)
    return os.path.basename(p)


def main():
    ap = argparse.ArgumentParser(description="Build the offline time-zone blob header.")
    ap.add_argument("source")
    ap.add_argument("--zoneinfo", default="/usr/share/zoneinfo")
    ap.add_argument("--posix", help="JSON map of zone name to POSIX string, instead of zoneinfo")
    ap.add_argument("--release", default="unknown")
    ap.add_argument("--tolerance", type=float, default=0.01)
    ap.add_argument("--no-simplify", action="store_true")
    ap.add_argument("--name", default="kTzData")
    ap.add_argument("--namespace", default="rc")
    args = ap.parse_args()

    if len(args.release.encode()) > 7:
        fail("--release is at most 7 bytes")

    raw = zone_rings(load_geojson(args.source)["features"])
    names = sorted(raw)
    if len(names) > 0xFFFF:
        fail("too many zones")

    if args.posix:
        with open(args.posix, encoding="utf-8") as f:
            pmap = json.load(f)
        tzdata = "none"
        missing = [n for n in names if n not in pmap]
        posix = {n: pmap[n] for n in names if n in pmap}
    else:
        try:
            with open(os.path.join(args.zoneinfo, "+VERSION"), encoding="ascii") as f:
                tzdata = f.read().strip()
        except OSError:
            tzdata = "unknown"
        posix = {}
        missing = []
        for n in names:
            p = read_posix(args.zoneinfo, n)
            if p is None:
                missing.append(n)
            else:
                posix[n] = p
    if missing:
        fail("zones missing from %s:\n  %s\nPoint --zoneinfo at a newer tzdata, for example "
             "the zoneinfo folder of the `tzdata` pip package." %
             (args.posix or args.zoneinfo, "\n  ".join(missing)))
    long_posix = [n for n in names if len(posix[n].encode()) > MAX_POSIX]
    if long_posix:
        fail("POSIX strings over %d bytes: %s" % (MAX_POSIX, ", ".join(long_posix)))
    if len(tzdata.encode()) > 7:
        tzdata = tzdata[:7]

    zones = []
    ring_count = point_count = 0
    for n in names:
        rings = []
        for ring in raw[n]:
            pts = ring if args.no_simplify else douglas_peucker(ring, args.tolerance)
            q = quantise(pts)
            if len(q) >= 3:
                rings.append(q)
                point_count += len(q)
        ring_count += len(rings)
        zones.append(rings)

    edge_zones, centre_zones = build_grid(zones)

    blob = bytearray(HEADER_SIZE + ZONE_SIZE * len(names))
    zones_off = HEADER_SIZE
    records = []
    for i, n in enumerate(names):
        name_off = len(blob)
        blob += n.encode() + b"\0"
        posix_off = len(blob)
        blob += posix[n].encode() + b"\0"
        rings_off = len(blob)
        for ring in zones[i]:
            blob += encode_ring(ring)
        records.append((name_off, posix_off, rings_off, len(zones[i])))
    for i, rec in enumerate(records):
        struct.pack_into("<IIII", blob, zones_off + ZONE_SIZE * i, *rec)

    grid_off = len(blob)
    grid = [EMPTY_CELL] * (GRID_W * GRID_H)
    lists = bytearray()
    list_base = grid_off + 4 * GRID_W * GRID_H
    list_offs = {}
    for cell in range(GRID_W * GRID_H):
        centre = centre_zones[cell] or set()
        edges = edge_zones.get(cell)
        if edges is None:
            if not centre:
                continue
            if len(centre) == 1:
                grid[cell] = DIRECT_CELL | next(iter(centre))
                continue
            zl = tuple(sorted(centre))
        else:
            zl = tuple(sorted(edges | centre))
        off = list_offs.get(zl)
        if off is None:
            off = list_base + len(lists)
            list_offs[zl] = off
            lists += struct.pack("<H", len(zl)) + struct.pack("<%dH" % len(zl), *zl)
        grid[cell] = off
    blob += struct.pack("<%dI" % len(grid), *grid)
    blob += lists

    struct.pack_into("<4sHHII8s8sI", blob, 0, b"RCTZ", 1, len(names), zones_off, grid_off,
                     args.release.encode(), tzdata.encode(),
                     int(round(0 if args.no_simplify else args.tolerance * UDEG)))

    print("zones %d, rings %d, points %d, blob %d bytes" %
          (len(names), ring_count, point_count, len(blob)), file=sys.stderr)
    if len(blob) > MAX_BLOB:
        fail("blob is %d bytes, over %d; try a larger --tolerance" % (len(blob), MAX_BLOB))

    cmd = ["python3", "-I", "scripts/tzgen.py"]
    argv = sys.argv[1:]
    skip_next = False
    for i, a in enumerate(argv):
        if skip_next:
            skip_next = False
            cmd.append(display_path(a))
            continue
        if a == args.source:
            cmd.append(display_path(a))
        elif a in ("--zoneinfo", "--posix"):
            cmd.append(a)
            skip_next = True
        else:
            cmd.append(a)
    tz_line = ("none (POSIX strings from %s)" % display_path(args.posix)) if args.posix else tzdata
    tol_line = "none (--no-simplify)" if args.no_simplify else "%g deg" % args.tolerance
    out = sys.stdout
    out.write("// Generated by scripts/tzgen.py. Do not edit; see docs/BUILD.md.\n")
    out.write("// Command: %s\n" % " ".join(shlex.quote(c) for c in cmd))
    out.write("// Boundary release: timezone-boundary-builder %s\n" % args.release)
    out.write("// tzdata: %s\n" % tz_line)
    out.write("// Tolerance: %s\n" % tol_line)
    out.write("// %s\n" % ATTRIBUTION)
    out.write("#pragma once\n\n#include <cstddef>\n#include <cstdint>\n\n")
    out.write("namespace %s {\n\n" % args.namespace)
    out.write("alignas(4) static const uint8_t %s[] =\n" % args.name)
    out.write(c_literal(blob))
    out.write(";\n\nconstexpr size_t %sSize = sizeof(%s) - 1;\n\n" % (args.name, args.name))
    out.write("}  // namespace %s\n" % args.namespace)


if __name__ == "__main__":
    main()
