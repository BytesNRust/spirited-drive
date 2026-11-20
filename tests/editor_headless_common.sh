#!/bin/sh
# What editor_headless_track.sh and editor_headless_car.sh share (sourced, with the editor as their $1): the editor run headless (SDL dummy
# video and audio), a scratch folder, fail() that counts, and png_check (a 960x600 picture with the road and the points in it).
# Neither script bounds its runs: ctest does (TIMEOUT), as CI has no `timeout`.
set -u
editor=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir "$work/tmp" && export TMPDIR="$work/tmp/"   # the scratch folders the editor makes (studio-race-*) are this run's alone: another session's game cannot fail a check
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }

cat > "$work/png_check.py" <<'PY'
import sys, struct, zlib
d = open(sys.argv[1], "rb").read()
mode = sys.argv[2]
assert d[:8] == b"\x89PNG\r\n\x1a\n"
w, h, depth, ctype = struct.unpack(">IIBB", d[16:26])
assert (w, h) == (960, 600), (w, h)
assert ctype in (2, 6) and depth == 8
bpp = 3 if ctype == 2 else 4
i, idat = 8, b""
while i < len(d):
    n, t = struct.unpack(">I4s", d[i:i + 8])
    if t == b"IDAT": idat += d[i + 8:i + 8 + n]
    i += 12 + n
raw = zlib.decompress(idat)
stride = w * bpp
rows, prev = [], bytearray(stride)
for y in range(h):
    f = raw[y * (stride + 1)]
    line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
    for x in range(stride):
        a = line[x - bpp] if x >= bpp else 0
        b = prev[x]
        c = prev[x - bpp] if x >= bpp else 0
        if f == 1: line[x] = (line[x] + a) & 255
        elif f == 2: line[x] = (line[x] + b) & 255
        elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
        elif f == 4:
            p = a + b - c
            pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
            line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
    rows.append(line)
    prev = line
# the road is light grey and the control points blue (viewport.cpp): near those colours, so many pixels, and nothing else is
def count(rgb, tol=12):
    n = 0
    for row in rows:
        n += sum(1 for r, g, b in zip(row[0::bpp], row[1::bpp], row[2::bpp])
                 if abs(r - rgb[0]) <= tol and abs(g - rgb[1]) <= tol and abs(b - rgb[2]) <= tol)
    return n
road, points, red, amber = count((200, 204, 212)), count((90, 160, 255)), count((255, 70, 70)), count((255, 176, 32))
if mode == "good":   # the road was made and no limit breaks: no red, no amber
    assert road > 400 and points > 100 and red == 0 and amber == 0, (road, points, red, amber)
elif mode == "warn":   # a curve over an amber limit and under the red ones: amber nodes, no red
    assert road > 400 and points > 100 and amber > 20 and red == 0, (road, points, red, amber)
else:                # a corner too tight: the road is made (grey), and its nodes at the corner are red
    assert road > 400 and points > 100 and red > 50, (road, points, red)
PY
png_check() { python3 "$work/png_check.py" "$1" "$2" || fail "$1 is not a 960x600 picture with the $2 project's road and points in it"; }
