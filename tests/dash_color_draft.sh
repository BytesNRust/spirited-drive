#!/bin/sh
# tools/dash_color_draft.py (dep98 item 2): a first dash_color.png per dash from the dash's art. A fixture dash (a red, a blue and a grey gauge) gives red, blue and the
# default colour on exactly the lit pixels; a dash that has a colour file is left alone; an output inside data/ or art/, a file that is there, a missing dump and a bad
# colour are refused and write nothing. With the game's files (original/) two real dashes are dumped and drafted, each with its sheet. Skipped without python3 + Pillow.
# usage: dash_color_draft.sh PYTHON TOOL [TNFS ORIGINAL_DIR DASH_MASKS_DIR]
py=$1
tool=$2
tnfs=${3:-}
orig=${4:-}
real_masks=${5:-}
"$py" -c "import PIL" 2>/dev/null || { echo "skip: no Pillow"; exit 0; }
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }

mkdir -p "$work/dump" "$work/masks/FIX" "$work/masks/HAS"
"$py" - "$work" <<'PY'
import sys
from PIL import Image, ImageDraw
w = sys.argv[1]
art = Image.new("RGB", (640, 480), (20, 20, 20))
d = ImageDraw.Draw(art)
d.rectangle([100, 300, 139, 339], fill=(230, 30, 30))      # a red gauge
d.rectangle([400, 300, 439, 339], fill=(30, 40, 230))      # a blue one
d.rectangle([250, 380, 289, 419], fill=(200, 200, 200))    # a grey one: no hue
art.save(w + "/dump/FIX.ppm")
mask = Image.new("RGB", (640, 480), (0, 0, 0))
m = ImageDraw.Draw(mask)
for box in ([100, 300, 139, 339], [400, 300, 439, 339], [250, 380, 289, 419]):
    m.rectangle(box, fill=(0, 255, 0))
mask.save(w + "/masks/FIX/dash_mask.png")
mask.save(w + "/masks/HAS/dash_mask.png")
Image.new("RGBA", (640, 480), (255, 128, 0, 255)).save(w + "/masks/HAS/dash_color.png")
PY
cp "$work/dump/FIX.ppm" "$work/dump/HAS.ppm"

"$py" "$tool" "$work/dump" "$work/masks" "$work/out" >"$work/log" 2>&1 || fail "the fixture draft: $(cat "$work/log")"
[ -f "$work/out/FIX/dash_color.png" ] && [ -f "$work/out/FIX/dash_mask.png" ] && [ -f "$work/out/FIX/sheet.png" ] || fail "no draft, mask copy or sheet for FIX"
[ ! -e "$work/out/HAS" ] || fail "a dash that has a colour file was drafted without --all"
"$py" - "$work" <<'PY' || fail "the drafted colours are not the art's hues on the lit pixels"
import sys
from PIL import Image
w = sys.argv[1]
c = Image.open(w + "/out/FIX/dash_color.png").convert("RGBA")
def at(x, y): return c.getpixel((x, y))
red, blue, grey = at(120, 320), at(420, 320), at(270, 400)
assert red[3] == 255 and red[0] == 255 and red[1] < 20 and red[2] < 20, red
assert blue[3] == 255 and blue[2] == 255 and blue[0] < 20, blue
assert grey[:3] == (255, 128, 0) and grey[3] == 255, grey         # no hue: orange
assert at(98, 298)[3] == 255 and at(96, 296)[3] == 0                # two pixels round the lit ones, no more
assert at(10, 10)[3] == 0 and at(200, 100)[3] == 0                  # nothing elsewhere
colours = {p[:3] for n, p in c.getcolors(1 << 20) if p[3] == 255}
assert len(colours) == 3, colours
PY
cmp -s "$work/masks/FIX/dash_mask.png" "$work/out/FIX/dash_mask.png" || fail "the mask copy is not the mask"

"$py" "$tool" "$work/dump" "$work/masks" "$work/out" >"$work/log" 2>&1 && fail "a second draft went over the first"
grep -q "never goes over a file" "$work/log" || fail "no word about the file that is there: $(cat "$work/log")"
"$py" "$tool" --all --default-colour 00cc66 "$work/dump" "$work/masks" "$work/out2" >"$work/log" 2>&1 || fail "--all: $(cat "$work/log")"
[ -f "$work/out2/HAS/dash_color.png" ] || fail "--all did not draft the dash that has a colour file"
"$py" - "$work" <<'PY' || fail "--default-colour is not the colour of the grey gauge"
import sys
from PIL import Image
c = Image.open(sys.argv[1] + "/out2/FIX/dash_color.png").convert("RGBA")
assert c.getpixel((270, 400))[:3] == (0, 204, 102), c.getpixel((270, 400))
PY
mkdir -p "$work/data" "$work/art"
for bad in "$work/data/out" "$work/art/out"; do
    "$py" "$tool" "$work/dump" "$work/masks" "$bad" >"$work/log" 2>&1 && fail "an output inside $bad was taken"
    [ ! -e "$bad" ] || fail "something was written in $bad"
done
"$py" "$tool" "$work/nodump" "$work/masks" "$work/out3" >"$work/log" 2>&1 && fail "a missing dump was taken"
"$py" "$tool" --default-colour red "$work/dump" "$work/masks" "$work/out4" >"$work/log" 2>&1 && fail "a bad --default-colour was taken"
[ ! -e "$work/out4" ] && [ ! -e "$work/out3" ] || fail "a refused run wrote something"
cp -R "$work/masks" "$work/masks-before"
"$py" "$tool" --all "$work/dump" "$work/masks" "$work/out5" >/dev/null 2>&1
diff -r "$work/masks" "$work/masks-before" >/dev/null || fail "the dash masks folder was written"

# the real game: two dashes dumped (headless, at night, in the cockpit) and drafted, each with its three-panel sheet
if [ -n "$orig" ] && [ -r "$orig/SIMDATA/CARFAMS/F512TR.CFM" ] && [ -d "$real_masks/F512TR" ]; then
    "$py" "$tool" --game "$tnfs" --data "$orig" --stems F512TR,CZR1 "$work/real-dump" "$real_masks" "$work/real" >"$work/log" 2>&1 || fail "the real dashes: $(cat "$work/log")"
    for car in F512TR CZR1; do
        [ -s "$work/real/$car/dash_color.png" ] && [ -s "$work/real/$car/sheet.png" ] || fail "no draft or sheet for $car"
    done
    "$py" - "$work" <<'PY' || fail "the 512TR's sheet is not four panels, or its draft has no colour"
import sys
from PIL import Image
w = sys.argv[1]
s = Image.open(w + "/real/F512TR/sheet.png")
assert s.size == (2560, 480), s.size
c = Image.open(w + "/real/F512TR/dash_color.png").convert("RGBA")
assert any(p[3] == 255 for n, p in c.getcolors(1 << 20))
PY
    [ ! -e "$real_masks/F512TR/dash_color.png" ] || fail "the draft was written in the dash masks folder"
else
    echo "dash_color_draft: the real-dash part needs original/: skipped"
fi

[ "$failures" -eq 0 ] && echo "dash_color_draft: ok"
exit "$failures"
