#!/usr/bin/env python3
"""tools/light_sheet.py and the arguments of tools/light_photo.sh, with no game and no CD data (so CI runs it): the 2x2 sheet puts dawn, day, dusk and night in
their places, a half-size picture is the 2x2 average, a picture that is not a P6 or not the others' size is refused, and the script refuses a bad TRACK, NODE
or binary before it runs anything. Standard library only; usage: light_photo_check.py REPO"""
import os
import struct
import subprocess
import sys
import tempfile
import zlib

repo = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
sys.path.insert(0, os.path.join(repo, "tools"))
import light_sheet  # noqa: E402

failures = []


def check(ok, what):
    if not ok:
        failures.append(what)
        print("FAIL: " + what)


def write_ppm(path, width, height, colour, comment=False):
    with open(path, "wb") as f:
        f.write(b"P6\n" + (b"# a comment\n" if comment else b"") + b"%d %d\n255\n" % (width, height) + bytes(colour) * (width * height))


def read_png(path):
    """(width, height, rows of RGB bytes) of a PNG with filter 0 on every row, as write_png makes."""
    with open(path, "rb") as f:
        data = f.read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    at, idat, width, height = 8, b"", 0, 0
    while at < len(data):
        size, tag = struct.unpack(">I4s", data[at:at + 8])
        body = data[at + 8:at + 8 + size]
        assert zlib.crc32(tag + body) & 0xFFFFFFFF == struct.unpack(">I", data[at + 8 + size:at + 12 + size])[0], "bad crc"
        if tag == b"IHDR":
            width, height = struct.unpack(">II", body[:8])
        elif tag == b"IDAT":
            idat += body
        at += 12 + size
    raw = zlib.decompress(idat)
    stride = width * 3 + 1
    return width, height, [raw[y * stride + 1:(y + 1) * stride] for y in range(height)]


def pixel(rows, x, y):
    return tuple(rows[y][3 * x:3 * x + 3])


work = tempfile.mkdtemp(prefix="light_photo_check.")
colours = [(200, 10, 10), (10, 200, 10), (10, 10, 200), (200, 200, 10)]   # dawn, day, dusk, night
names = []
for i, colour in enumerate(colours):
    path = os.path.join(work, "%s.ppm" % "ABCN"[i])
    write_ppm(path, 64, 48, colour, comment=(i == 1))   # a comment in one header: the reader skips it
    names.append(path)

# the sheet: each quarter is its picture (halved: 32 x 24), sampled away from the letter
png = os.path.join(work, "sheet.png")
check(light_sheet.main(["light_sheet.py", png] + names) == 0, "the sheet is made")
width, height, rows = read_png(png)
check((width, height) == (64, 48), "the halved sheet is the size of one picture: %dx%d" % (width, height))
for i, colour in enumerate(colours):
    x0, y0 = (i % 2) * 32, (i // 2) * 24
    check(pixel(rows, x0 + 30, y0 + 22) == colour, "quarter %s holds its picture: %s" % ("ABCN"[i], pixel(rows, x0 + 30, y0 + 22)))
    white = sum(1 for y in range(y0 + 6, y0 + 24) for x in range(x0 + 6, x0 + 28) if pixel(rows, x, y) == (255, 255, 255))
    check(white > 20, "quarter %s has its letter (%d white pixels in its corner)" % ("ABCN"[i], white))

# --full keeps the size
full = os.path.join(work, "full.png")
check(light_sheet.main(["light_sheet.py", full] + names + ["--full"]) == 0, "the full sheet is made")
width, height, rows = read_png(full)
check((width, height) == (128, 96), "the full sheet is two pictures wide: %dx%d" % (width, height))
check(pixel(rows, 64 + 60, 40) == colours[1], "full: the day quarter holds its picture")

# a half-size picture is the average of 2x2 blocks (rounded)
w, h, px = light_sheet.halve(2, 2, bytearray([0, 0, 0, 100, 100, 100, 200, 200, 200, 255, 255, 255]))
check((w, h) == (1, 1) and bytes(px) == bytes([139, 139, 139]), "halve averages a 2x2 block: %s" % bytes(px).hex())

# refusals: not a P6, a size that differs from the others', the wrong count
bad = os.path.join(work, "bad.ppm")
with open(bad, "wb") as f:
    f.write(b"P3\n1 1\n255\n0 0 0\n")
check(light_sheet.main(["light_sheet.py", png, bad] + names[1:]) == 1, "a P3 picture is refused")
small = os.path.join(work, "small.ppm")
write_ppm(small, 32, 24, (1, 2, 3))
check(light_sheet.main(["light_sheet.py", png] + names[:3] + [small]) == 1, "a picture of another size is refused")
check(light_sheet.main(["light_sheet.py", png] + names[:3]) == 2, "three pictures are a usage error")
check(light_sheet.main(["light_sheet.py", png, os.path.join(work, "missing.ppm")] + names[1:]) == 1, "a missing picture is refused")

# the script's arguments are checked before anything runs (a bad one exits 2 and prints why)
script = os.path.join(repo, "tools", "light_photo.sh")
fake_bin = os.path.join(work, "tnfs")
open(fake_bin, "w").close()
os.chmod(fake_bin, 0o755)


def run(*args):
    done = subprocess.run(["sh", script] + list(args), stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    return done.returncode, done.stderr


rc, err = run()
check(rc == 2 and "usage" in err, "no arguments: usage, exit 2 (%d)" % rc)
rc, err = run(fake_bin, work, "cy1", "70")
check(rc == 2 and "TRACK" in err, "a lower-case track is refused (%d)" % rc)
rc, err = run(fake_bin, work, "CY1", "seventy")
check(rc == 2 and "NODE" in err, "a node that is not a number is refused (%d)" % rc)
rc, err = run(os.path.join(work, "nothing"), work, "CY1", "70")
check(rc == 2 and "binary" in err, "a missing binary is refused (%d)" % rc)
rc, err = run(fake_bin, os.path.join(work, "no_cd"), "CY1", "70")
check(rc == 2 and "CD data" in err, "a missing CD folder is refused (%d)" % rc)

print("light_photo_check: %s" % ("ok" if not failures else "%d failure(s)" % len(failures)))
sys.exit(1 if failures else 0)
