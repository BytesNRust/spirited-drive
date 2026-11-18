#!/bin/sh
# The track editor runs headless (Dear ImGui over the SDL software renderer): a project folder is made, N frames are drawn and the last one
# is written as a PNG that has the window's size and more than one colour; --save writes the project files; a bad folder fails.
# usage: editor_headless.sh EDITOR
set -u
editor=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }

"$editor" "$work/proj" --frames 3 --save --shot "$work/shot.png" >"$work/log" 2>&1 || fail "run: $(cat "$work/log")"
[ -f "$work/proj/project.ini" ] && [ -f "$work/proj/trace.json" ] || fail "no project files"
[ -f "$work/shot.png" ] || { fail "no PNG"; exit 1; }
python3 - "$work/shot.png" <<'PY' || fail "the PNG is not a 960x600 picture with the ImGui window's title bar in it"
import sys, struct, zlib
d = open(sys.argv[1], "rb").read()
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
# the "Project" window opens at (60, 60): its title bar is ImGui's blue, which nothing else in the picture is
r, g, b = rows[70][300 * bpp], rows[70][300 * bpp + 1], rows[70][300 * bpp + 2]
assert b > r + 40 and b > g + 20, (r, g, b)
PY
"$editor" "$work/proj" --frames 2 --user "$work/user" >/dev/null 2>&1 || fail "run with --user"
[ -z "$(ls "$work/user" 2>/dev/null)" ] || fail "headless run kept a layout file in the player folder"
"$editor" >/dev/null 2>&1 && fail "no project folder was accepted"
[ "$failures" -eq 0 ] && echo "editor_headless: ok"
exit "$failures"
