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
road, points, red, amber = count((200, 204, 212)), count((90, 160, 255)), count((255, 70, 70)), count((255, 176, 32))
if mode == "good":   # the road was made and no limit breaks: no red, no amber
    assert road > 400 and points > 100 and red == 0 and amber == 0, (road, points, red, amber)
elif mode == "warn":   # a curve over an amber limit and under the red ones: amber nodes, no red
    assert road > 400 and points > 100 and amber > 20 and red == 0, (road, points, red, amber)
else:                # a corner too tight: the road is made (grey), and its nodes at the corner are red
    assert road > 400 and points > 100 and red > 50, (road, points, red)
PY
png_check() { python3 "$work/png_check.py" "$1" "$2" || fail "$1 is not a 960x600 picture with the $2 project's road and points in it"; }
png_check "$work/shot.png" good
mkdir "$work/mild"
printf '{\n  "version": 1,\n  "width": 40.0,\n  "points": [[0.0, 0.0], [0.0, 40.0], [15.0, 80.0], [15.0, 120.0], [0.0, 160.0], [0.0, 200.0]]\n}\n' > "$work/mild/trace.json"   # an S of 6.5 degrees a node at most: nothing is marked (the old limit was 6)
"$editor" "$work/mild" --frames 6 --shot "$work/mild.png" >"$work/log" 2>&1 || fail "run on a mild S: $(cat "$work/log")"
if [ -f "$work/mild.png" ]; then png_check "$work/mild.png" good; else fail "no PNG of the mild S"; fi
mkdir "$work/warn"
printf '{\n  "version": 1,\n  "width": 40.0,\n  "points": [[0.0, -40.0], [0.0, 0.0], [20.0, 40.0], [20.0, 80.0], [0.0, 120.0], [0.0, 160.0]]\n}\n' > "$work/warn/trace.json"   # an S that turns about 9 degrees a node: amber
"$editor" "$work/warn" --frames 6 --shot "$work/warn.png" >"$work/log" 2>&1 || fail "run on a curve over an amber limit: $(cat "$work/log")"
if [ -f "$work/warn.png" ]; then png_check "$work/warn.png" warn; else fail "no PNG of the curve over an amber limit"; fi
"$editor" "$work/warn" --select 3 --frames 6 --shot "$work/select.png" >"$work/log" 2>&1 || fail "run with a point selected (the Node window): $(cat "$work/log")"
[ -s "$work/select.png" ] || fail "no PNG with a point selected"
mkdir "$work/bad"
printf '{\n  "version": 1,\n  "width": 40.0,\n  "points": [[0.0, 0.0], [0.0, 80.0], [20.0, 90.0], [0.0, 100.0], [0.0, 180.0]]\n}\n' > "$work/bad/trace.json"
"$editor" "$work/bad" --frames 6 --shot "$work/bad.png" >"$work/log" 2>&1 || fail "run on a project that breaks limits: $(cat "$work/log")"
if [ -f "$work/bad.png" ]; then png_check "$work/bad.png" bad; else fail "no PNG of the project that breaks limits"; fi
# Build: a curve under the limits is built, one over them is refused with the limit named
"$editor" "$work/mild" --frames 2 --build >"$work/log" 2>&1 || fail "Build refused a curve under the limits: $(cat "$work/log")"
[ -s "$work/mild/build/mild.zip" ] || fail "no zip of the mild S"
"$editor" "$work/warn" --frames 2 --build >"$work/log" 2>&1 || fail "Build refused a curve that is only over an amber limit: $(cat "$work/log")"
[ -s "$work/warn/build/warn.zip" ] || fail "no zip of the amber curve"
"$editor" "$work/bad" --frames 2 --build >"$work/log" 2>&1 && fail "Build went on a curve over a red limit"
grep -q "tighter than 20.9 m\|more than 16.5 degrees" "$work/log" || fail "the refusal does not name the red limit: $(cat "$work/log")"
"$editor" "$work/proj" --frames 2 --user "$work/user/sub" >/dev/null 2>&1 || fail "run with --user"
[ -f "$work/user/sub/track-editor.ini" ] || fail "the layout was not kept in the player folder (made when missing)"
# the game is not where --game says: Install says so, and what to do, without running anything
out=$("$editor" "$work/proj" --frames 2 --install --game "$work/no-such-game" --user "$work/u2" 2>&1) && fail "install with a game that is not there went"
echo "$out" | grep -q "copy Spirited Drive and Spirited Drive Studio to Applications" || fail "no hint about the game's place: $out"
# and with no --game at all, from a folder with no ./tnfs (skipped where the game really is in /Applications)
if [ ! -x "/Applications/Spirited Drive.app/Contents/MacOS/tnfs" ] && [ ! -x "$(dirname "$editor")/../../../Spirited Drive.app/Contents/MacOS/tnfs" ]; then
    out=$(cd "$work" && "$editor" "$work/proj" --frames 2 --install 2>&1) && fail "install with no game found went"
    echo "$out" | grep -q "Applications" || fail "no hint with no game found: $out"
fi
"$editor" --no-such-option >/dev/null 2>&1 && fail "an unknown option was accepted"   # (no PROJECT is a run, not an error: it opens Documents/Spirited Drive Studio/my-track)
[ "$failures" -eq 0 ] && echo "editor_headless: ok"
exit "$failures"
