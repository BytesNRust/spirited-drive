#!/usr/bin/env python3
"""A mask changed from outside Studio, as an image editor would: paints a rectangle of an 8-bit grey PNG (the game's mask format) with one value, in place.
Studio no longer edits masks (dep100), so the tests that need a changed mask write it this way, with the standard library only.
usage: mask_poke.py FILE X Y W H VALUE"""
import struct
import sys
import zlib


def chunks(data):
    p = 8
    while p < len(data):
        n, t = struct.unpack(">I4s", data[p:p + 8])
        yield t, data[p + 8:p + 8 + n]
        p += 12 + n


def chunk(t, d):
    return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)


def unfilter(raw, w, h):
    rows, prev = [], bytearray(w)
    for y in range(h):
        f = raw[y * (w + 1)]
        cur = bytearray(raw[y * (w + 1) + 1:(y + 1) * (w + 1)])
        for x in range(w) if f else ():   # filter 0: nothing to undo
            a = cur[x - 1] if x else 0
            b = prev[x]
            c = prev[x - 1] if x else 0
            if f == 1:
                cur[x] = (cur[x] + a) & 255
            elif f == 2:
                cur[x] = (cur[x] + b) & 255
            elif f == 3:
                cur[x] = (cur[x] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                cur[x] = (cur[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(cur)
        prev = cur
    return rows


def main(argv):
    if len(argv) != 7:
        print(__doc__)
        return 2
    path, x0, y0, rw, rh, value = argv[1], *map(int, argv[2:7])
    if not 0 <= value <= 255:
        print(__doc__)
        return 2
    data = open(path, "rb").read()
    parts = dict(chunks(data))
    w, h, depth, ctype = struct.unpack(">IIBB", parts[b"IHDR"][:10])
    if depth != 8 or ctype != 0 or parts[b"IHDR"][12] != 0:
        print("not an 8-bit grey, non-interlaced PNG")
        return 1
    idat = b"".join(d for t, d in chunks(data) if t == b"IDAT")
    rows = unfilter(zlib.decompress(idat), w, h)
    for y in range(max(0, y0), min(h, y0 + rh)):
        for x in range(max(0, x0), min(w, x0 + rw)):
            rows[y][x] = value
    raw = b"".join(b"\0" + bytes(r) for r in rows)
    out = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", parts[b"IHDR"]) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    open(path, "wb").write(out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
