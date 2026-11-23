#!/bin/sh
# The Track light window, headless (--ask light): it opens on a light folder, takes the photo of the working copy through the game (a fake one here, which records the
# light files it was given), and leaves the folder as it was; a folder with no track_light/ leaves the start screen up with the reason. No CD data and no real game.
# usage: editor_light_window.sh EDITOR REPO MAKE_FAKE_TRI
set -u
editor=$1 repo=$2 make_tri=$3
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }

mkdir -p "$work/light/track_light" "$work/cd/SIMDATA/MISC" "$work/bare" "$work/tmp"
"$make_tri" "$work/cd/SIMDATA/MISC/AL1.TRI" 30 || { echo "FAIL: no fake TRI" >&2; exit 1; }   # the Studio reads the track's node count from the TRI: 30 segments are 120 nodes
export TMPDIR="$work/tmp/"
# AL1's light file in the new format: two tunnels (the second with a look of its own at day), a day sun
cat > "$work/light/track_light/AL1.txt" <<'TXT'
# tnfs track light 2
track AL1
tunnel 10 20
tunnel 60 71

time B
  sun 150 50 55
  tunnel_look 100
  tunnel_look 60 71 90 lamps 150
TXT
cat > "$work/fake_game" <<'SH'
#!/bin/sh
shot= mods=
while [ $# -gt 0 ]; do case $1 in --screenshot) shot=$2; shift;; --mods) mods=$2; shift;; esac; shift; done
{ echo "time=$TNFS_LIGHT_TIME view=$TNFS_VIEW nohud=${TNFS_NO_HUD:-0}"; head -2 "$mods"/track_light/*.txt; } > "$FAKE_RECORD"
printf 'P6\n1280 960\n255\n' > "$shot"
head -c 3686400 /dev/zero | LC_ALL=C tr '\000' '\100' >> "$shot"
SH
chmod +x "$work/fake_game"
export FAKE_RECORD="$work/record"
before=$(cksum < "$work/light/track_light/AL1.txt")

"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --window 1280x800 --frames 200 --shot "$work/window.png" > "$work/out" 2>&1 ||
    fail "the window did not run: $(tail -3 "$work/out")"
[ -s "$work/window.png" ] || fail "no picture of the window"
grep -q "^time=day view=1 nohud=1$" "$work/record" 2>/dev/null || fail "the photo was not taken at day, at the bumper (kind 1), without the HUD: $(cat "$work/record" 2>/dev/null)"
grep -q "^track AL1$" "$work/record" 2>/dev/null || fail "the game was not given AL1's file: $(cat "$work/record" 2>/dev/null)"
[ "$(cksum < "$work/light/track_light/AL1.txt")" = "$before" ] || fail "the light file changed without a Save"
[ -z "$(ls "$work/tmp")" ] || fail "the photo left its scratch folder behind: $(ls "$work/tmp")"

# the View selector: each --light-view is the game's chase kind (Bumper 1, Dash 0, near 2, mid 3, far 4, High 12), and the choice is kept in the ini for the next run
i=0
for want in 1 0 2 3 4 12; do
    rm -f "$work/record"
    "$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --light-view "$i" --window 1280x800 --frames 80 --user "$work/viewuser$i" --shot "$work/view$i.png" > "$work/outv" 2>&1 ||
        fail "--light-view $i did not run: $(tail -3 "$work/outv")"
    grep -q "^time=day view=$want nohud=1$" "$work/record" 2>/dev/null || fail "--light-view $i: the game was not given TNFS_VIEW=$want: $(cat "$work/record" 2>/dev/null)"
    i=$((i + 1))
done
grep -q "^LightView=5$" "$work/viewuser5/track-editor.ini" 2>/dev/null || fail "the High view was not kept in the ini: $(cat "$work/viewuser5/track-editor.ini" 2>/dev/null)"
rm -f "$work/record"
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --window 1280x800 --frames 80 --user "$work/viewuser5" --shot "$work/viewkept.png" > "$work/outv" 2>&1 ||
    fail "the run on the kept view did not run: $(tail -3 "$work/outv")"
grep -q "^time=day view=12 nohud=1$" "$work/record" 2>/dev/null || fail "the kept view (High) was not used by the next run: $(cat "$work/record" 2>/dev/null)"
# the HUD checkbox: off by default (nohud=1 above), --light-hud 1 puts it back, kept in the ini like the view
rm -f "$work/record"
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --light-hud 1 --window 1280x800 --frames 80 --user "$work/huduser" --shot "$work/hud.png" > "$work/outv" 2>&1 ||
    fail "--light-hud 1 did not run: $(tail -3 "$work/outv")"
grep -q "^time=day view=1 nohud=0$" "$work/record" 2>/dev/null || fail "--light-hud 1: the game still got TNFS_NO_HUD: $(cat "$work/record" 2>/dev/null)"
grep -q "^LightHud=1$" "$work/huduser/track-editor.ini" 2>/dev/null || fail "the HUD choice was not kept in the ini"
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --light-hud 2 --frames 5 > "$work/outbad" 2>&1 && fail "--light-hud 2 was accepted"
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --light-view 6 --frames 5 > "$work/outbad" 2>&1 && fail "--light-view 6 was accepted"

"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/bare" --ask light --window 1280x800 --frames 20 --shot "$work/bare.png" > "$work/out2" 2>&1 ||
    fail "a folder with no track_light/ ended the editor: $(tail -3 "$work/out2")"
[ -s "$work/bare.png" ] || fail "no picture for a folder with no track_light/"
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --window 1280x800 --frames 30 --steps print-panels > "$work/panels" 2>&1 ||
    fail "print-panels in the Track light window: $(tail -3 "$work/panels")"
[ "$(grep -c '^panel [0-9]*: .*: open, drawn' "$work/panels")" = 3 ] && grep -q '^unlisted:$' "$work/panels" || fail "the Track light window's panels are not all in the Window menu: $(cat "$work/panels")"
# the Tunnels section: AL1's two tunnels (the facts) with the day's look of each; a lamps strength set on the second one rewrites only its tunnel_look line, Save writes the file
# with every other line as it was
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --frames 30 --steps print-tunnels,tunnel-lamps:1,220,print-tunnels,light-print,save > "$work/tunnels" 2>&1 ||
    fail "the Tunnels steps: $(tail -3 "$work/tunnels")"
grep -q "^tunnels: 2$" "$work/tunnels" || fail "the file's two tunnels are not listed: $(grep '^tunnels' "$work/tunnels")"
grep -q "^default: light 100 lamps - " "$work/tunnels" || fail "the default row is not the day's tunnel_look without nodes: $(grep '^default' "$work/tunnels")"
grep -q "^tunnel 1: 60-71 own: light 90 lamps 150 " "$work/tunnels" || fail "the second tunnel does not have its own look: $(grep '^tunnel ' "$work/tunnels")"
grep -q "^tunnel 1: 60-71 own: light 90 lamps 220 " "$work/tunnels" || fail "the lamps strength was not set on the second tunnel: $(grep '^tunnel ' "$work/tunnels")"
grep -q "^light: AL1 nodes 120 time B (unsaved) tunnels 2 looks 2 " "$work/tunnels" || fail "light-print: $(grep '^light:' "$work/tunnels")"
grep -q "^  tunnel_look 60 71 90 lamps 220$" "$work/light/track_light/AL1.txt" || fail "Save did not write the tunnel's look: $(cat "$work/light/track_light/AL1.txt")"
[ "$(diff "$work/light/track_light/AL1.txt" - <<'TXT' | grep -c '^[<>]'
# tnfs track light 2
track AL1
tunnel 10 20
tunnel 60 71

time B
  sun 150 50 55
  tunnel_look 100
  tunnel_look 60 71 90 lamps 150
TXT
)" = 2 ] || fail "Save changed more than the one line: $(cat "$work/light/track_light/AL1.txt")"
# the sun of the time: set, Save, the file has the line; a node range outside the road is refused
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --frames 30 --steps sun:120,30,70,light-print,save > "$work/sun" 2>&1 || fail "the sun step: $(tail -3 "$work/sun")"
grep -q "^light: AL1 nodes 120 time B (unsaved) .* sun 120 30 70$" "$work/sun" || fail "the sun is not in the window's model: $(grep '^light:' "$work/sun")"
grep -q "^  sun 120 30 70$" "$work/light/track_light/AL1.txt" || fail "Save did not write the sun line: $(cat "$work/light/track_light/AL1.txt")"
# the panel's own widgets, clicked with the mouse (ui_probe.h): the sun's Direction slider, Add brightness and its strip bar dragged over points 1..5, Add tone and its Left shortcut,
# the Save button; the file has the lines, a new window reads them again, the Remove button takes a line away and Save writes that too
cp "$work/light/track_light/AL1.txt" "$work/ui_before.txt"
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --window 1280x800 --frames 30 \
    --steps ui-at:azimuth_slider,0.25,ui-click:add_brightness,ui-span:brightness0_bar,0.55,0.95,ui-click:add_tone,ui-click:tone0_Left,light-print,ui-click:save,light-print > "$work/ui" 2>&1 ||
    fail "the clicked flow: $(tail -3 "$work/ui")"
grep -q "^light: AL1 nodes 120 time B (unsaved) .*brightness 1 tones 1 " "$work/ui" || fail "the clicks did not make the lines: $(grep '^light:' "$work/ui")"
[ "$(grep '^light:' "$work/ui" | tail -1 | grep -c unsaved)" = 0 ] || fail "the Save button left the unsaved mark: $(grep '^light:' "$work/ui")"
az=$(sed -n 's/^  sun \([0-9]*\) 30 70$/\1/p' "$work/light/track_light/AL1.txt")
[ -n "$az" ] && [ "$az" -ge 70 ] && [ "$az" -le 110 ] || fail "the slider's click did not set the direction near 90: $(cat "$work/light/track_light/AL1.txt")"
grep -q "^  brightness [0-9]* [0-9]* 100 right$" "$work/light/track_light/AL1.txt" || fail "the bar dragged over points 1..5 is not a right strip: $(cat "$work/light/track_light/AL1.txt")"
grep -q "^  tone [0-9]* [0-9]* left ffe080:50$" "$work/light/track_light/AL1.txt" || fail "the Left shortcut did not make a left tone: $(cat "$work/light/track_light/AL1.txt")"
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --window 1280x800 --frames 30 --steps light-print,ui-click:tone0_remove,light-print,ui-click:save,light-print > "$work/ui2" 2>&1 ||
    fail "the clicked flow, reopened: $(tail -3 "$work/ui2")"
[ "$(grep '^light:' "$work/ui2" | head -1 | grep -c 'brightness 1 tones 1 ')" = 1 ] || fail "the saved lines were not read again: $(grep '^light:' "$work/ui2")"
grep '^light:' "$work/ui2" | tail -1 | grep -q 'brightness 1 tones 0 ' || fail "the Remove button did not take the tone away: $(grep '^light:' "$work/ui2")"
grep -q "^  tone " "$work/light/track_light/AL1.txt" && fail "the removed tone is still in the file"
cksum < "$work/light/track_light/AL1.txt" > "$work/ui_cksum"
# the window's other widgets, clicked: the node buttons move the photo's node, Add tunnel adds a tunnel, a moment's button keeps the one file's edits, Revert puts the file back as it was opened.
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --window 1280x800 --frames 40 \
    --steps light-node,ui-click:node_p10,ui-click:node_p10,ui-click:node_p1,light-node,ui-click:node_m10,ui-click:node_m1,light-node,ui-at:node_slider,0.9,light-node,ui-click:add_tunnel,light-print,ui-click:time_C,light-print,ui-click:revert,light-print,ui-click:tunnel0_remove,light-print,ui-click:time_B,light-print > "$work/ui4" 2>&1 ||
    fail "the clicked node, tunnel, moment and Revert flow: $(tail -3 "$work/ui4")"
nodes=$(grep '^light node:' "$work/ui4" | awk '{print $3}' | tr '\n' ' ')
set -- $nodes
[ "$#" = 4 ] && [ "$(($2 - $1))" = 21 ] && [ "$(($3 - $2))" = -11 ] && [ "$4" -gt "$3" ] || fail "the node buttons did not move the photo's node by +21, -11 and the slider to the right: $nodes"
lines=$(grep '^light: ' "$work/ui4")
[ "$(echo "$lines" | sed -n 1p | grep -c 'time B (unsaved) tunnels 3 ')" = 1 ] || fail "Add tunnel did not add a tunnel: $(echo "$lines" | sed -n 1p)"
[ "$(echo "$lines" | sed -n 2p | grep -c 'time C (unsaved) tunnels 3 ')" = 1 ] || fail "the Dusk button lost the unsaved tunnel: $(echo "$lines" | sed -n 2p)"
echo "$lines" | sed -n 3p | grep -q 'tunnels 2 ' && ! echo "$lines" | sed -n 3p | grep -q 'unsaved' || fail "Revert did not put the file back as it was opened: $(echo "$lines" | sed -n 3p)"
echo "$lines" | sed -n 4p | grep -q 'time C (unsaved) tunnels 1 ' || fail "a tunnel row's Remove (the table's first column, reachable at 1280 px) did not take a tunnel away: $(echo "$lines" | sed -n 4p)"
echo "$lines" | sed -n 5p | grep -q 'time B (unsaved) tunnels 1 looks 2 ' || fail "the Day button did not bring the Day block back with the edit: $(echo "$lines" | sed -n 5p)"
[ "$(cksum < "$work/light/track_light/AL1.txt")" = "$(cat "$work/ui_cksum")" ] || fail "an unsaved click flow changed the file"
# the Lamps section, clicked (it starts closed with no lamp): opened, Add lamp, the node and light boxes typed into, a Strip choice from the combo, Save writes the line, Remove takes it away and Save writes the file without it
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --window 1280x800 --frames 40 \
    --steps light-print,ui-click:lamps_header,ui-click:add_lamp,light-print,ui-click:lamp0_node,ui-text:80,ui-click:lamp0_pct,ui-text:60,ui-click:lamp0_strip,ui-click:lamp0_strip_left,ui-click:save,light-print > "$work/lamps1" 2>&1 ||
    fail "the clicked Lamps flow: $(tail -3 "$work/lamps1")"
l1=$(grep '^light: ' "$work/lamps1" | sed -n 1p | sed 's/.* lamps \([0-9]*\) .*/\1/'); l2=$(grep '^light: ' "$work/lamps1" | sed -n 2p | sed 's/.* lamps \([0-9]*\) .*/\1/')
[ "$((l2 - l1))" = 1 ] || fail "Add lamp did not add a lamp: $l1 -> $l2"
grep -q '^  lamp 80 60 left$' "$work/light/track_light/AL1.txt" || fail "the typed node and light and the Strip choice are not in the saved file: $(grep '^  lamp' "$work/light/track_light/AL1.txt")"
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --window 1280x800 --frames 40 \
    --steps light-print,ui-click:lamp0_remove,light-print,ui-click:save,light-print > "$work/lamps2" 2>&1 || fail "the clicked Lamps Remove: $(tail -3 "$work/lamps2")"
m1=$(grep '^light: ' "$work/lamps2" | sed -n 1p | sed 's/.* lamps \([0-9]*\) .*/\1/'); m2=$(grep '^light: ' "$work/lamps2" | sed -n 2p | sed 's/.* lamps \([0-9]*\) .*/\1/')
[ "$((m1 - m2))" = 1 ] || fail "a lamp row's Remove did not take the lamp away: $m1 -> $m2"
grep -q '^  lamp 80 60' "$work/light/track_light/AL1.txt" && fail "the removed lamp is still in the saved file"
# the Tunnels table, clicked: the nodes typed into From, the default row's and a tunnel's Light %, Lamps % and Reach (a reach out of 0.5..4 is refused with its reason), the Kind combo; Save writes them
sed -i.bak '/^  lamp /d' "$work/light/track_light/AL1.txt" && rm -f "$work/light/track_light/AL1.txt.bak"
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --window 1280x800 --frames 80 \
    --steps ui-click:time_B,ui-click:tunnel0_from,ui-text:12,key:Return,ui-click:tunnel1_reach,ui-text:20,key:Return,ui-click:tunnel1_reach,ui-text:3,key:Return,ui-click:tunnel0_lamps,ui-text:120,key:Return,ui-click:tunnel1_light,ui-text:65,key:Return,ui-click:tunneldefault_light,ui-text:85,key:Return,ui-click:save,light-print > "$work/tcells" 2>&1 ||
    fail "the clicked tunnel cells flow: $(tail -3 "$work/tcells")"
f="$work/light/track_light/AL1.txt"
grep -q '^tunnel 12 20' "$f" || fail "the typed From node is not in the saved file: $(grep '^tunnel' "$f")"
grep -q '^  tunnel_look 85$' "$f" || fail "the default row's Light % is not in the saved file: $(grep tunnel_look "$f")"
grep -q '^  tunnel_look 60 71 65 .*lamps 220' "$f" || fail "the tunnel's Light % (its Lamps % kept) is not in the saved file: $(grep tunnel_look "$f")"
grep -q '^  tunnel_look 12 20 .*lamps 120' "$f" || fail "the first tunnel's Lamps % is not in the saved file: $(grep tunnel_look "$f")"
grep -Eq '^  tunnel_look 60 71 .*reach 3' "$f" || fail "the typed Reach is not in the saved file (20 is out of 0.5..4: the second entry, 3, is): $(grep tunnel_look "$f")"
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --window 1280x800 --frames 60 \
    --steps ui-click:tunnel0_from,ui-click:tunnel0_kind,ui-click:tunnel0_kind_hall,ui-click:save,light-print > "$work/tkind" 2>&1 || fail "the clicked Kind combo: $(tail -3 "$work/tkind")"
grep -q '^tunnel 12 20 lit' "$f" || fail "the Kind combo's hall is not in the saved file: $(grep '^tunnel' "$f")"
# the Sun's other two sliders and the three "auto" ticks, clicked: Height and Shadow strength set (the slider's ends are +-76 and the click at 0.9 of its width 76), each part unticked in turn (a "-" in the
# line: the generator's own), and a sun with every part auto is no line at all; the file is saved after each flow
sun_flow() { "$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --window 1280x800 --frames 30 --steps "$1" > "$work/sunout" 2>&1 || fail "the sun flow $1: $(tail -3 "$work/sunout")"; }
sun_flow ui-at:elevation_slider,0.9,ui-at:shadow_slider,0.2,ui-click:save
grep -Eq '^  sun [0-9]+ 7[0-9] (1[5-9]|2[0-5])$' "$f" || fail "the Height and Shadow strength sliders did not set the sun's height near 76 and shadow near 20: $(grep '  sun' "$f")"
sun_flow ui-click:shadow_tick,ui-click:save
grep -Eq '^  sun [0-9]+ 7[0-9] -$' "$f" || fail "unticking Shadow strength did not leave it auto (-): $(grep '  sun' "$f")"
sun_flow ui-click:elevation_tick,ui-click:save
grep -Eq '^  sun [0-9]+ - -$' "$f" || fail "unticking Height did not leave it auto (-): $(grep '  sun' "$f")"
sun_flow ui-click:azimuth_tick,light-print,ui-click:save
grep -q '^  sun ' "$f" && fail "a sun with every part auto is still a line in the file: $(grep '  sun' "$f")"
grep '^light:' "$work/sunout" | head -1 | grep -q ' sun ' && fail "the window's model still has a sun with every part auto: $(grep '^light:' "$work/sunout")"
sun_flow ui-click:azimuth_tick,ui-click:shadow_tick,ui-click:save
grep -Eq '^  sun [0-9]+ - 55$' "$f" || fail "ticking Direction and Shadow strength from no sun did not make 'sun AZ - 55': $(grep '  sun' "$f")"
# the window's other widgets, clicked (ui_probe.h; light-state prints what the panel has): the View combo's item and the HUD box (the photo's camera and HUD), the Light compare radios, the Test drive button
# (it starts a drive on this track, time and node), a brightness line's Remove, and the track combo: a file with changes is not left for another track ("Save or Revert"), and after Revert it is asked
light_flow() { "$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --window 1280x800 --frames 40 --steps "$1" > "$work/lout" 2>&1 || fail "the flow $1: $(tail -3 "$work/lout")"; }
light_flow light-state,ui-click:node_p1,ui-click:view_combo,ui-click:view_4,light-state,ui-click:hud,light-state
[ "$(grep -c '^light state: track AL1 view 0 hud 0 compare off' "$work/lout")" = 1 ] || fail "the window did not open on the Bumper view, no HUD, compare off: $(grep '^light state' "$work/lout")"
grep -q '^light state: track AL1 view 4 hud 0 ' "$work/lout" || fail "the View combo's Chase far item did not set view 4: $(grep '^light state' "$work/lout")"
grep -q '^light state: track AL1 view 4 hud 1 ' "$work/lout" || fail "the HUD box did not turn the HUD on: $(grep '^light state' "$work/lout")"
light_flow ui-click:node_p1,ui-click:compare_1,light-state,ui-click:compare_2,light-state,ui-click:compare_0,light-state
[ "$(grep '^light state' "$work/lout" | sed 's/.* compare \([a-z]*\) .*/\1/' | tr '\n' ' ')" = "before four off " ] || fail "the Light compare radios (Before, Four times, Off) did not set the mode: $(grep '^light state' "$work/lout")"
light_flow ui-click:node_p1,ui-click:drive,print-drive,light-state
# a game that stays up (the fake one ends at once, and a second click then starts a new drive): the same button, now Stop test drive, stops it
printf '#!/bin/sh\nexec sleep 30\n' > "$work/slow_game"
chmod +x "$work/slow_game"
"$editor" --game "$work/slow_game" --cd "$work/cd" --light-dir "$work/light" --ask light --window 1280x800 --frames 40 --steps ui-click:node_p1,ui-click:drive,light-state,ui-click:drive,light-state > "$work/lout2" 2>&1 || fail "the Test drive / Stop flow: $(tail -3 "$work/lout2")"
[ "$(grep '^light state' "$work/lout2" | sed 's/.* drive \([a-z]*\) .*/\1/' | tr '\n' ' ')" = "running stopped " ] || fail "Test drive then Stop test drive did not start and stop the drive: $(grep '^light state' "$work/lout2")"
grep -q '^drive: running AL1 B node ' "$work/lout" || fail "the Test drive button did not start a drive on AL1's Day: $(grep '^drive' "$work/lout")"
grep -q '^light state: .* drive running ' "$work/lout" || fail "the panel does not show the drive as running: $(grep '^light state' "$work/lout")"
light_flow ui-click:add_brightness,light-print,ui-click:brightness0_remove,light-print
counts=$(grep '^light:' "$work/lout" | sed 's/.* brightness \([0-9]*\) .*/\1/' | tr '\n' ' ')
[ "$(echo "$counts" | awk '{ print ($2 + 1 == $1) ? "yes" : "no" }')" = yes ] || fail "a brightness line's Remove did not take the line away (counts after Add, after Remove: $counts): $(grep '^light:' "$work/lout")"
light_flow ui-click:add_brightness,ui-click:track_combo,ui-click:track_CY1,light-state,ui-click:revert,ui-click:track_combo,ui-click:track_CY1,light-state
grep '^light state' "$work/lout" | sed -n 1p | grep -q 'track AL1 .* error: the file has unsaved changes: Save or Revert before another track$' || fail "a track chosen with unsaved changes was not refused with Save or Revert: $(grep '^light state' "$work/lout")"
grep '^light state' "$work/lout" | sed -n 2p | grep -q 'Save or Revert' && fail "after Revert the track change was still refused for unsaved changes: $(grep '^light state' "$work/lout")"
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --window 1280x800 --frames 30 --steps ui-click:no_such_widget > "$work/ui3" 2>&1 && fail "a click on a widget that is not there went"
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --frames 30 --steps tunnel-lamps:5,100 > "$work/tunnels" 2>&1 && fail "a tunnel that is not there went"
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --window 1280x800 --frames 40 --shot "$work/tunnels.png" > "$work/tunnels" 2>&1 || fail "the window with the new sections: $(tail -3 "$work/tunnels")"
[ "$failures" = 0 ] && echo "ok"
exit "$failures"
