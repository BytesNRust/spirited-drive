#!/usr/bin/env python3
"""The classes of a car kit's masks: prints one line `<texture> <class>=<texels> ...` per art-generated/car_masks/*_mask.png (8-bit grey, the texel IS its class).
usage: originals_sweep_classes.py KIT_DIR"""
import glob
import os
import struct
import sys
import zlib


def chunks(data):
    p = 8
    while p < len(data):
        n, t = struct.unpack(">I4s", data[p:p + 8])
        yield t, data[p + 8:p + 8 + n]
        p += 12 + n


def paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    return a if pa <= pb and pa <= pc else b if pb <= pc else c


def texels(path):
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")
    parts = dict(chunks(data))
    w, h, depth, ctype = struct.unpack(">IIBB", parts[b"IHDR"][:10])
    if depth != 8 or ctype != 0 or parts[b"IHDR"][12] != 0:
        raise ValueError("not an 8-bit grey, non-interlaced PNG")
    raw = zlib.decompress(b"".join(d for t, d in chunks(data) if t == b"IDAT"))
    prev, out = bytearray(w), []
    for y in range(h):
        f = raw[y * (w + 1)]
        cur = bytearray(raw[y * (w + 1) + 1:(y + 1) * (w + 1)])
        for x in range(w):
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
                cur[x] = (cur[x] + paeth(a, b, c)) & 255
        out.append(cur)
        prev = cur
    return b"".join(out)


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    files = sorted(glob.glob(os.path.join(argv[1], "art-generated", "car_masks", "*_mask.png")))
    if not files:
        print("no masks")
        return 1
    for path in files:
        try:
            counts = {}
            for v in texels(path):
                counts[v] = counts.get(v, 0) + 1
        except (ValueError, KeyError, zlib.error) as e:
            print("%s unreadable: %s" % (os.path.basename(path)[:-9], e))
            return 1
        print(os.path.basename(path)[:-9], " ".join("%d=%d" % (k, counts[k]) for k in sorted(counts)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
