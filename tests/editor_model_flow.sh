#!/bin/sh
# The Model window's controls clicked as a person does (ui_probe names the widgets, the steps ui-click send the mouse): Enlarge floats it and Dock back puts it where it was, the camera kept; the Soft edges
# switch is on screen and toggles; at render scale 1 the switch is grey (a click changes nothing) and at the player's own scale it works. Needs the player's game files (original/): skipped without them.
# usage: editor_model_flow.sh EDITOR REPO
. "$(dirname "$0")/editor_headless_common.sh"
repo=$2
[ -f "$repo/original/SIMDATA/CARFAMS/DVIPER.CFM" ] || { echo "skip: no original/"; exit 0; }
ws="$work/ws"
mkdir -p "$ws"
kit="$ws/cars/car-supra"   # made by the first run, opened by the others
run() { u=$1; shift; "$editor" --workspace "$ws" --user "$work/$u" --cd "${CD:-$repo/original}" --port-data "$repo/data" --window 1280x800 "$@"; }
state() { echo "$out" | grep '^model: ' | sed -n "$1p"; }

# Enlarge then Dock back: floating, then docked again, the camera the same
out=$(run u1 --frames 60 --steps new-car:0,psd-wait:3,camera:0,camera:3,ui-click:model_enlarge,psd-wait:3,camera:3,ui-click:model_enlarge,psd-wait:3,camera:3,camera:0 2>&1) || fail "enlarge flow: $out"
echo "$out" | grep -q 'no such widget' && fail "the Enlarge button was not on screen: $out"
[ "$(state 1 | cut -d' ' -f2-5)" = "big 0 docked 1" ] || fail "the Model window did not start docked: $(state 1)"
[ "$(state 2 | cut -d' ' -f2-5)" = "big 1 docked 0" ] || fail "Enlarge did not float it: $(state 2)"
[ "$(state 3 | cut -d' ' -f2-5)" = "big 0 docked 1" ] || fail "Dock back did not put it back in the dock: $(state 3)"
cams=$(echo "$out" | grep '^camera: ')
[ "$(echo "$cams" | sed -n 1p)" = "$(echo "$cams" | sed -n 2p)" ] || fail "the camera changed through Enlarge and Dock back: $cams"

# the Soft edges switch is on screen, starts as the player's setting (Restomod's on) and toggles; the Render scale combo is on screen
out=$(run u2 --frames 60 "$kit" --steps psd-wait:3,camera:3,ui-click:model_soft,psd-wait:3,camera:3,ui-click:model_soft,psd-wait:3,camera:3 2>&1) || fail "soft flow: $out"
echo "$out" | grep -q 'no such widget' && fail "the Soft edges switch was not on screen: $out"
[ "$(state 1 | cut -d' ' -f6-9)" = "scale 2 soft 1" ] || fail "the Model did not start at the player's render scale 2 with Soft edges on: $(state 1)"
[ "$(state 2 | cut -d' ' -f8-9)" = "soft 0" ] || fail "the switch did not turn Soft edges off: $(state 2)"
[ "$(state 3 | cut -d' ' -f8-9)" = "soft 1" ] || fail "the switch did not turn Soft edges on again: $(state 3)"
out=$(run u2 --frames 40 "$kit" --steps psd-wait:3,ui-click:model_render_scale,psd-wait:3 2>&1) || fail "render scale combo: $out"
echo "$out" | grep -q 'no such widget' && fail "the Render scale combo was not on screen: $out"

# render scale 1: the switch is grey, a click changes nothing
out=$(run u3 --render-scale 1 --frames 60 "$kit" --steps psd-wait:3,camera:3,ui-click:model_soft,psd-wait:3,camera:3 2>&1) || fail "scale 1 flow: $out"
[ "$(state 1 | cut -d' ' -f6-9)" = "scale 1 soft 1" ] || fail "--render-scale 1 did not set the scale: $(state 1)"
[ "$(state 2 | cut -d' ' -f8-9)" = "soft 1" ] || fail "a click on the grey switch changed it: $(state 2)"

# the player's own render scale: restomod.cfg's
mkdir -p "$work/u4"
printf 'render_scale=3\n' > "$work/u4/restomod.cfg"
out=$(run u4 --frames 40 "$kit" --steps psd-wait:3,camera:3 2>&1) || fail "player scale: $out"
[ "$(state 1 | cut -d' ' -f6-7)" = "scale 3" ] || fail "the Model did not start at the player's render_scale 3: $(state 1)"

# the picture is the game's frame at the render scale whatever the panel's size (read from the buffer, not the screen's copy: the software renderer cannot show it), and the soft edges are in it
# from scale 2 and not at 1
for sc in 1 2; do
    for sw in on off; do
        out=$(run u5 --render-scale $sc --soft $sw --shine on --paint 1 --frames 40 "$kit" --steps psd-wait:3 --model-shot "$work/pic_${sc}_$sw.png" 2>&1) || fail "model shot scale $sc soft $sw: $out"
    done
done
echo "$out" | grep -q '^model picture: 1280 x 960, [1-9][0-9]* drawn$' || fail "the scale 2 picture is not the game's 1280 x 960 frame with a car in it: $out"
out1=$(run u5 --render-scale 1 --soft on --shine on --paint 1 --frames 40 "$kit" --steps psd-wait:3 --model-shot "$work/pic_1_on2.png" 2>&1)
echo "$out1" | grep -q '^model picture: 640 x 480, [1-9][0-9]* drawn$' || fail "the scale 1 picture is not the game's 640 x 480 frame with a car in it: $out1"
cmp -s "$work/pic_1_on.png" "$work/pic_1_off.png" || fail "at render scale 1 the soft edges changed the picture"
cmp -s "$work/pic_2_on.png" "$work/pic_2_off.png" && fail "at render scale 2 the soft edges did not change the picture"

# a layout saved with the Model floating (Enlarge, then the window closed): Dock back has no node it came from, and docks beside the other panels; Enlarge and Dock back work again after it
run u6 --frames 40 "$kit" --big --steps psd-wait:3 >/dev/null 2>&1 || fail "the floating layout was not made"
awk '/^\[Window\]\[Model\]/ { m = 1; next } /^\[/ { m = 0 } m && /^DockId=/ { found = 1 } END { exit found }' "$work/u6/track-editor.ini" || fail "the saved layout has the Model docked: the floating case is not tested"
out=$(run u6 --frames 90 "$kit" --steps psd-wait:3,camera:3,ui-click:model_enlarge,psd-wait:4,camera:3,ui-click:model_enlarge,psd-wait:4,camera:3,ui-click:model_enlarge,psd-wait:4,camera:3 2>&1) || fail "floating layout: $out"
[ "$(state 1 | cut -d' ' -f2-5)" = "big 0 docked 0" ] || fail "the saved layout did not start the Model floating: $(state 1)"
[ "$(state 2 | cut -d' ' -f2-5)" = "big 0 docked 1" ] || fail "Dock back did nothing with the Model floating from a saved layout: $(state 2)"
[ "$(state 3 | cut -d' ' -f2-5)" = "big 1 docked 0" ] || fail "Enlarge after it did not float the window: $(state 3)"
[ "$(state 4 | cut -d' ' -f2-5)" = "big 0 docked 1" ] || fail "Dock back after Enlarge did not dock it: $(state 4)"

# Reset layout with the window enlarged: it is docked again, not enlarged, and Enlarge works from there
out=$(run u7 --frames 90 "$kit" --steps psd-wait:3,ui-click:model_enlarge,psd-wait:4,camera:3,reset-layout,psd-wait:25,camera:3,ui-click:model_enlarge,psd-wait:4,camera:3 2>&1) || fail "reset layout: $out"
[ "$(state 1 | cut -d' ' -f2-5)" = "big 1 docked 0" ] || fail "Enlarge did not float it before the reset: $(state 1)"
[ "$(state 2 | cut -d' ' -f2-5)" = "big 0 docked 1" ] || fail "Reset layout left the Model enlarged or floating: $(state 2)"
[ "$(state 3 | cut -d' ' -f2-5)" = "big 1 docked 0" ] || fail "Enlarge did not work after Reset layout: $(state 3)"

# the soft edges and render scale are the process's (the Preview, the card photos and the captures read them): the Model's own choices (scale 1, Soft edges off, a hand moving the camera) leave them as they were
out=$(run u8 --render-scale 1 --soft off --frames 60 "$kit" --steps psd-wait:3,camera:1,psd-wait:3,camera:3 2>&1) || fail "studio state: $out"
[ "$(state 1 | cut -d" " -f6-14)" = "scale 1 soft 0 studio scale 2 soft 1" ] || fail "the Model's own scale or Soft edges reached the rest of the Studio: $(state 1)"

# what moves is drawn on the scale 1 frame (Auto-rotate here), and the full frame when it stops
out=$(run u9 --render-scale 2 --frames 60 "$kit" --steps psd-wait:3,ui-click:model_auto_rotate,psd-wait:6 --model-shot "$work/rot_on.png" 2>&1) || fail "auto-rotate shot: $out"
echo "$out" | grep -q '^model picture: 640 x 480, ' || fail "with Auto-rotate on the picture is not the scale 1 frame: $out"
out=$(run u9 --render-scale 2 --frames 60 "$kit" --steps psd-wait:3,ui-click:model_auto_rotate,psd-wait:6,ui-click:model_auto_rotate,psd-wait:6 --model-shot "$work/rot_off.png" 2>&1) || fail "auto-rotate off shot: $out"
echo "$out" | grep -q '^model picture: 1280 x 960, ' || fail "with Auto-rotate off again the picture is not the full frame: $out"

# the car whole inside the drawing frame from a side (Side is wider than the first view: the framing is made again for it): no row or column at the frame's edge has a car pixel, at render scale 1, 2 and 3
cat > "$work/whole.py" <<'PY'
import struct, sys, zlib
d = open(sys.argv[1], "rb").read()
w, h, depth, ctype = struct.unpack(">IIBB", d[16:26])
assert depth == 8 and ctype == 6, (depth, ctype)
i, idat = 8, b""
while i < len(d):
    n, t = struct.unpack(">I4s", d[i:i + 8])
    if t == b"IDAT": idat += d[i + 8:i + 8 + n]
    i += 12 + n
raw, stride, prev, p = zlib.decompress(idat), w * 4, bytearray(w * 4), 0
xs, ys = [], []
for y in range(h):
    f, line = raw[p], bytearray(raw[p + 1:p + 1 + stride])
    p += 1 + stride
    for x in range(stride):
        a = line[x - 4] if x >= 4 else 0; b = prev[x]; c = prev[x - 4] if x >= 4 else 0
        if f == 1: line[x] = (line[x] + a) & 255
        elif f == 2: line[x] = (line[x] + b) & 255
        elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
        elif f == 4:
            pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
            line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
    prev = line
    cols = [x for x in range(w) if line[4 * x + 3] != 0]
    if cols: ys.append(y); xs += [cols[0], cols[-1]]
if not xs: sys.exit("no car in the picture")
print("car box %d..%d x %d..%d in %d x %d" % (min(xs), max(xs), ys[0], ys[-1], w, h))
sys.exit(0 if min(xs) > 0 and max(xs) < w - 1 and ys[0] > 0 and ys[-1] < h - 1 else 1)
PY
for sc in 1 2 3; do
    out=$(run u11 --render-scale $sc --shine on --frames 60 "$kit" --steps psd-wait:3,ui-click:model_side,psd-wait:4 --model-shot "$work/side_$sc.png" 2>&1) || fail "side view at scale $sc: $out"
    echo "$out" | grep -q 'no such widget' && fail "the Side button was not on screen: $out"
    python3 -I "$work/whole.py" "$work/side_$sc.png" || fail "the car is cut by the frame's edge in the Side view at render scale $sc"
done
# an ini from an older Studio (before the dock was kept per car kind: its windows are floating, there are no dock nodes at all): the layout is built, the Model is docked and Enlarge / Dock back work
mkdir -p "$work/u10"
printf '[Window][Model]\nPos=40,40\nSize=1100,700\nCollapsed=0\n\n[Window][Car]\nPos=60,60\nSize=500,500\nCollapsed=0\n' > "$work/u10/track-editor.ini"
out=$(run u10 --frames 90 "$kit" --steps psd-wait:3,camera:3,ui-click:model_enlarge,psd-wait:4,camera:3,ui-click:model_enlarge,psd-wait:4,camera:3 2>&1) || fail "old ini: $out"
[ "$(state 1 | cut -d' ' -f2-9)" = "big 0 docked 1 scale 2 soft 1" ] || fail "an older ini did not give the Model its dock and the player's scale and soft edges: $(state 1)"
[ "$(state 2 | cut -d' ' -f2-5)" = "big 1 docked 0" ] || fail "Enlarge did not work with an older ini: $(state 2)"
[ "$(state 3 | cut -d' ' -f2-5)" = "big 0 docked 1" ] || fail "Dock back did not work with an older ini: $(state 3)"

# a colour changed in the Color box again and again (a drag does it every frame): the Model makes the scene again for it, at most 10 a second (model_view's rule, checked by test_editor_model_view), and it is
# made again for the last one (the step camera:4 counts the scenes; a headless frame is slower than the 100 ms, so here every change may take one: never more than the changes)
mkdir -p "$work/u12"
run u12 --frames 40 "$kit" --steps psd-wait:3,ui-click:add_color,psd-wait:3,save >/dev/null 2>&1 || fail "a color was not added to the kit"
steps=psd-wait:6,camera:4; for n in 1 2 3 4 5 6; do steps="$steps,paint-rgb:0,$((n * 30)),50,90,psd-wait:1"; done
out=$(run u12 --render-scale 2 --paint 1 --frames 120 "$kit" --steps "$steps,psd-wait:30,camera:4,paint-print" 2>&1) || fail "colour changes: $out"
first=$(echo "$out" | grep '^model rebuilds: ' | sed -n 1p | cut -d' ' -f3); last=$(echo "$out" | grep '^model rebuilds: ' | sed -n 2p | cut -d' ' -f3)
[ -n "$last" ] && [ $((last - first)) -ge 1 ] && [ $((last - first)) -le 6 ] || fail "the Model did not make the scene again for 6 colours in 1..6 times: $first -> $last"
echo "$out" | grep -q '^paints: .*#b4325a$' || fail "the last colour was not the card's: $(echo "$out" | grep '^paints:')"

[ "$failures" = 0 ] || exit 1
