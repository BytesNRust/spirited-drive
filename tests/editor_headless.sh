#!/bin/sh
# The track editor runs headless (Dear ImGui over the SDL software renderer): a project folder is made, N frames are drawn and the last one
# is written as a PNG that has the window's size, the road's line and the control points drawn in the viewport panel; --save writes the
# project files; --user makes its folder and keeps the layout in it; an unknown option fails. (Never run it without a PROJECT here: that opens a window on a folder in the real Documents.)
# usage: editor_headless.sh EDITOR
set -u
editor=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }

"$editor" "$work/proj" --frames 6 --save --shot "$work/shot.png" >"$work/log" 2>&1 || fail "run: $(cat "$work/log")"
[ -f "$work/proj/project.ini" ] && [ -f "$work/proj/trace.json" ] || fail "no project files"
[ -f "$work/shot.png" ] || { fail "no PNG"; exit 1; }
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
road, points, red = count((200, 204, 212)), count((90, 160, 255)), count((255, 70, 70))
if mode == "good":   # the road was made and no limit breaks: no red
    assert road > 400 and points > 100 and red == 0, (road, points, red)
else:                # a corner too tight: the road is made (grey), and its nodes at the corner are red
    assert road > 400 and points > 100 and red > 50, (road, points, red)
PY
png_check() { python3 "$work/png_check.py" "$1" "$2" || fail "$1 is not a 960x600 picture with the $2 project's road and points in it"; }
png_check "$work/shot.png" good
mkdir "$work/bad"
printf '{\n  "version": 1,\n  "width": 40.0,\n  "points": [[0.0, 0.0], [0.0, 80.0], [20.0, 90.0], [0.0, 100.0], [0.0, 180.0]]\n}\n' > "$work/bad/trace.json"
"$editor" "$work/bad" --frames 6 --shot "$work/bad.png" >"$work/log" 2>&1 || fail "run on a project that breaks limits: $(cat "$work/log")"
if [ -f "$work/bad.png" ]; then png_check "$work/bad.png" bad; else fail "no PNG of the project that breaks limits"; fi
"$editor" "$work/proj" --frames 2 --user "$work/user/sub" >/dev/null 2>&1 || fail "run with --user"
[ -f "$work/user/sub/track-editor.ini" ] || fail "the layout was not kept in the player folder (made when missing)"
"$editor" --no-such-option >/dev/null 2>&1 && fail "an unknown option was accepted"   # (no PROJECT is a run, not an error: it opens Documents/Spirited Drive Studio/my-track)
[ "$failures" -eq 0 ] && echo "editor_headless: ok"
exit "$failures"
