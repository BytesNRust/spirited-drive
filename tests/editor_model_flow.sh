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
out=$(run u1 --frames 60 --steps new-car:0,poll-wait:1,camera:0,camera:3,ui-click:model_enlarge,poll-wait:1,camera:3,ui-click:model_enlarge,poll-wait:1,camera:3,camera:0 2>&1) || fail "enlarge flow: $out"
echo "$out" | grep -q 'no such widget' && fail "the Enlarge button was not on screen: $out"
[ "$(state 1 | cut -d' ' -f2-5)" = "big 0 docked 1" ] || fail "the Model window did not start docked: $(state 1)"
[ "$(state 2 | cut -d' ' -f2-5)" = "big 1 docked 0" ] || fail "Enlarge did not float it: $(state 2)"
[ "$(state 3 | cut -d' ' -f2-5)" = "big 0 docked 1" ] || fail "Dock back did not put it back in the dock: $(state 3)"
cams=$(echo "$out" | grep '^camera: ')
[ "$(echo "$cams" | sed -n 1p)" = "$(echo "$cams" | sed -n 2p)" ] || fail "the camera changed through Enlarge and Dock back: $cams"

# the Soft edges switch is on screen, starts as the player's setting (Restomod's on) and toggles; the Render scale combo is on screen
out=$(run u2 --frames 60 "$kit" --steps poll-wait:1,camera:3,ui-click:model_soft,poll-wait:1,camera:3,ui-click:model_soft,poll-wait:1,camera:3 2>&1) || fail "soft flow: $out"
echo "$out" | grep -q 'no such widget' && fail "the Soft edges switch was not on screen: $out"
[ "$(state 1 | cut -d' ' -f6-9)" = "scale 2 soft 1" ] || fail "the Model did not start at the player's render scale 2 with Soft edges on: $(state 1)"
[ "$(state 2 | cut -d' ' -f8-9)" = "soft 0" ] || fail "the switch did not turn Soft edges off: $(state 2)"
[ "$(state 3 | cut -d' ' -f8-9)" = "soft 1" ] || fail "the switch did not turn Soft edges on again: $(state 3)"
out=$(run u2 --frames 40 "$kit" --steps poll-wait:1,ui-click:model_render_scale,poll-wait:1 2>&1) || fail "render scale combo: $out"
echo "$out" | grep -q 'no such widget' && fail "the Render scale combo was not on screen: $out"

# render scale 1: the switch is grey, a click changes nothing
out=$(run u3 --render-scale 1 --frames 60 "$kit" --steps poll-wait:1,camera:3,ui-click:model_soft,poll-wait:1,camera:3 2>&1) || fail "scale 1 flow: $out"
[ "$(state 1 | cut -d' ' -f6-9)" = "scale 1 soft 1" ] || fail "--render-scale 1 did not set the scale: $(state 1)"
[ "$(state 2 | cut -d' ' -f8-9)" = "soft 1" ] || fail "a click on the grey switch changed it: $(state 2)"

# the player's own render scale: restomod.cfg's
mkdir -p "$work/u4"
printf 'render_scale=3\n' > "$work/u4/restomod.cfg"
out=$(run u4 --frames 40 "$kit" --steps poll-wait:1,camera:3 2>&1) || fail "player scale: $out"
[ "$(state 1 | cut -d' ' -f6-7)" = "scale 3" ] || fail "the Model did not start at the player's render_scale 3: $(state 1)"

# the picture is the game's frame at the render scale whatever the panel's size (read from the buffer, not the screen's copy: the software renderer cannot show it), and the soft edges are in it
# from scale 2 and not at 1
for sc in 1 2; do
    for sw in on off; do
        out=$(run u5 --render-scale $sc --soft $sw --shine on --paint 1 --frames 40 "$kit" --steps poll-wait:1 --model-shot "$work/pic_${sc}_$sw.png" 2>&1) || fail "model shot scale $sc soft $sw: $out"
    done
done
echo "$out" | grep -q '^model picture: 1280 x 960, [1-9][0-9]* drawn$' || fail "the scale 2 picture is not the game's 1280 x 960 frame with a car in it: $out"
out1=$(run u5 --render-scale 1 --soft on --shine on --paint 1 --frames 40 "$kit" --steps poll-wait:1 --model-shot "$work/pic_1_on2.png" 2>&1)
echo "$out1" | grep -q '^model picture: 640 x 480, [1-9][0-9]* drawn$' || fail "the scale 1 picture is not the game's 640 x 480 frame with a car in it: $out1"
cmp -s "$work/pic_1_on.png" "$work/pic_1_off.png" || fail "at render scale 1 the soft edges changed the picture"
cmp -s "$work/pic_2_on.png" "$work/pic_2_off.png" && fail "at render scale 2 the soft edges did not change the picture"

[ "$failures" = 0 ] || exit 1
