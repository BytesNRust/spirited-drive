#!/bin/sh
# dep59 step 3: an added car in the menus, against the real binary and the CD data, every step waiting on the game's own marks
# (mark:car= / mark:opponent= from Control Central, mark:gallery= from the added car's gallery, mark:list_row= from the replay list).
# PART (one ctest each, so they run side by side):
#   central  Restomod: Control Central's car arrow twice from a fresh player's car 6 is the added car (16); its card.png is drawn (the
#            card's box follows the piece's card: another card, another box; the same in two runs).
#   gallery  a click on its card opens its gallery; Color one on (Red) shows its swatch, Skin one on (stripes) shows its name.
#   kept     Done keeps the pair and comes back to car 16; closed (Cancel) and started again, restomod.cfg keeps car=test-car and
#            look.test-car=stripes,Red, Control Central opens on car 16, and config.dat's car is an original (0..7).
#   rival    car 16 as the head to head rival: its rival card is drawn (the rival's box follows the piece's card), and a race of car 16
#            against car 16 runs from Race.
#   modes    Stock and Restored with the piece in --mods: no added car, the same picture as without it, no restomod.cfg.
#   replays  a whole race with car 16 from the menus: the replay list shows its Replay.rpx; without the piece the row says which mod it
#            needs.
# usage: car_card_menus.sh TNFS STUDIO REPO ORIGINAL_DIR [PART] (no PART: all of them, in turn)
set -u
abs() { case $1 in /*) echo "$1" ;; *) echo "$PWD/$1" ;; esac; }   # the walks run the game from the scratch folder
tnfs=$(abs "$1")
studio=$(abs "$2")
repo=$(abs "$3")
orig=$(abs "$4")
parts=${5:-central gallery kept rival modes replays}
[ -f "$orig/SIMDATA/CARSPECS/DVIPER.PBS" ] || { echo "skip: no CD in $orig"; exit 0; }
work=$(mktemp -d)
finish() { pkill -KILL -f "$work/" 2>/dev/null; sleep 1; if [ -n "${KEEP:-}" ]; then echo "kept $work"; else find "$work" -delete; fi; }
trap finish EXIT
trap 'exit 1' INT TERM   # a ctest timeout: the games stopped and the folder gone all the same
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
export SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0
step=60

# the piece: the Viper's copy, its figures and paints, a skin whose top is the Viper's shifted to another colour group, with its mask
piece=$work/piece
"$studio" car new --template viper --id test-car --stem TESTCAR --out "$piece" --name "Test Car" --data "$orig" --port-data "$repo/data" >/dev/null ||
    { echo "FAIL: tnfs-studio car new" >&2; exit 1; }
chmod -R u+w "$piece"
printf 'year=1992\npower=400 hp\nweight=1500 kg\npaints=Red:c8102e,Blue:1040c0\n' >> "$piece/mod.ini"
"$studio" unpack "$piece/SIMDATA/CARFAMS/TESTCAR.CFM" "$work/unpack" >/dev/null 2>&1 || { echo "FAIL: tnfs-studio unpack" >&2; exit 1; }
mkdir -p "$piece/skins/stripes"
cp "$work/unpack/c1/"*_topv.png "$piece/skins/stripes/topv.png"
python3 "$repo/tests/mods/reference/png_shift.py" "$piece/skins/stripes/topv.png" 64 || { echo "FAIL: png_shift" >&2; exit 1; }
cp "$piece/car_masks/TESTCAR/topv_mask.png" "$piece/skins/stripes/topv_mask.png"
"$studio" validate "$piece" >"$work/validate" 2>&1 || { echo "FAIL: the piece does not validate: $(cat "$work/validate")" >&2; exit 1; }
# the same piece with another card ("Other Car"): what the card's box shows follows card.png
other=$work/other
cp -R "$piece" "$other"
sed 's/^name=.*/name=Other Car/' "$piece/mod.ini" > "$other/mod.ini"
"$studio" car card "$other" --generic --force >/dev/null || { echo "FAIL: tnfs-studio car card --generic --force" >&2; exit 1; }
cmp -s "$piece/card.png" "$other/card.png" && { echo "FAIL: the other card is the same picture" >&2; exit 1; }
nomods=$work/nomods
mkdir -p "$nomods"

# a player folder in mode $1, fresh
user_in() {
    [ -d "$work/user" ] && find "$work/user" -delete
    mkdir -p "$work/user" && printf 'profile=%s\n' "$1" > "$work/user/port.cfg"
}
log() { grep -v "^input script" "$work/user/spirited-drive.log" 2>/dev/null; }
wait_for() { printf '0:WAIT:mark:%s:%s' "$1" "$step"; }
click() { printf '300:CLICK:%s:%s' "$1" "$2"; }
# the way in: our boot screen, EA's first movie, Game Mode, Single, the name box, Control Central (tools/menu_walk.sh's)
to_central="1500:ESC,$(wait_for movie=),500:ESC,$(wait_for screen=game_mode),$(click 310 217),$(wait_for screen=name_box),500:TEXT:R,500:ENTER,$(wait_for screen=central)"
car_on() { printf '%s,%s' "$(click 319 197)" "$(wait_for car=)"; }   # the car arrow's right (central_select.c), the mark its card logs
# a fresh player starts on car 6, the head to head rival 7; Restomod's list goes 0..7, the added cars, the Warrior (11): two on is car 16
car_16="$(car_on),$(click 319 197),$(wait_for car=16)"

# walk MODS SCRIPT DONE COUNT OUT: the game run with the input script until the log has COUNT lines starting with DONE, then its picture
# once the screen is still saved as OUT (none: no picture), the game stopped. 0 when they came and the picture was saved.
# Set for one walk: extra (more of the game's environment) and hold (seconds the game must go on running after DONE)
walk() {
    mods=$1 script=$2 done=$3 count=$4 out=$5
    [ -d "$work/cap" ] && find "$work/cap" -delete
    mkdir -p "$work/cap"
    capture=""   # the frames only for a walk that keeps a picture (each is 0.9 MB, sixty a second)
    [ -n "$out" ] && capture="TNFS_CAPTURE_DIR=$work/cap TNFS_CAPTURE_EVERY=1"
    rm -f "$work/status"
    ( (cd "$work" && env ${extra:-} $capture TNFS_VERBOSE=1 TNFS_INPUT_SETTLE=1500 TNFS_INPUT_SCRIPT="$script" \
        timeout -k 5 600 "$tnfs" --data "$orig" --mods "$mods" --user "$work/user" >"$work/out" 2>&1; echo $? >"$work/status") &)
    rc=1 waited=0 why="no $done in 600 s"
    while [ $waited -lt 600 ]; do
        sleep 1; waited=$((waited + 1))
        [ "$(log | grep -c "^$done")" -ge "$count" ] && { rc=0; break; }
        grep -q "input script: step.*timed out" "$work/user/spirited-drive.log" 2>/dev/null &&
            { why=$(grep -m1 "input script: step.*timed out" "$work/user/spirited-drive.log"); break; }
        [ -s "$work/status" ] && { why="the game ended with status $(cat "$work/status") after ${waited} s; its last words: $(tail -3 "$work/out")"; break; }
    done
    if grep -q "bad TNFS_INPUT_SCRIPT item" "$work/user/spirited-drive.log" 2>/dev/null; then   # a step dropped: the walk is not the one written
        rc=1; echo "walk: $(grep -m1 "bad TNFS_INPUT_SCRIPT item" "$work/user/spirited-drive.log")" >&2
    fi
    if [ $rc -eq 0 ] && [ -n "$out" ]; then still "$out" || rc=1; fi
    if [ $rc -eq 0 ] && [ "${hold:-0}" -gt 0 ]; then   # the game still running HOLD seconds later
        sleep "$hold"
        pgrep -f "$work/user" >/dev/null 2>&1 || { rc=1; echo "walk: the game ended within $hold s of $done" >&2; }
    fi
    pkill -TERM -f "$work/user" 2>/dev/null; sleep 1; pkill -KILL -f "$work/user" 2>/dev/null
    [ $rc -eq 0 ] || echo "walk: no $done: $why; the script's last steps: $(grep "^input script" "$work/user/spirited-drive.log" | tail -2); the log ends: $(log | tail -3)" >&2
    hold=0 extra=""   # sh keeps a function's prefix assignments: they are for one walk only
    return $rc
}
# the screen once it is still (as tools/menu_walk.sh: two seconds without a new frame, or two frames alike), saved as $1
still() {
    ticks=0 calm=0 seen=""
    while [ $ticks -lt 60 ]; do
        newest=$(ls "$work/cap" | sort | tail -1)
        if [ -n "$newest" ] && [ "$newest" = "$seen" ]; then
            calm=$((calm + 1))
            [ $calm -ge 4 ] && { cp "$work/cap/$newest" "$1"; return; }
        else calm=0 seen=$newest; fi
        set -- "$1" $(ls "$work/cap" | sort | tail -3)
        [ $# -eq 4 ] && cmp -s "$work/cap/$3" "$work/cap/$4" && { cp "$work/cap/$4" "$1"; return; }
        set -- "$1"
        sleep 0.5; ticks=$((ticks + 1))
    done
    return 1
}
# box A B X0 X1 Y0 Y1: 0 when the two frames (PPM, TNFS_CAPTURE_DIR) differ inside that box of the 640x480 screen (ends included)
box() {
    python3 -I - "$@" <<'PY'
import sys
def pixels(path):
    data = open(path, "rb").read()
    fields, at = [], 0
    while len(fields) < 4:   # P6, width, height, maximum, each after blanks
        while data[at:at + 1].isspace(): at += 1
        start = at
        while not data[at:at + 1].isspace(): at += 1
        fields.append(data[start:at])
    w = int(fields[1])
    return w, data[at + 1:]
a, b = sys.argv[1], sys.argv[2]
x0, x1, y0, y1 = (int(v, 0) for v in sys.argv[3:7])
(wa, pa), (wb, pb) = pixels(a), pixels(b)
rows = lambda w, p: [p[(y * w + x0) * 3:(y * w + x1 + 1) * 3] for y in range(y0, y1 + 1)]
sys.exit(0 if rows(wa, pa) != rows(wb, pb) else 1)
PY
}
card_box() { box "$1" "$2" 0x13 0x14a 0x70 0x11a; }    # the player's card (central_follow.c's hit box)
rival_box() { box "$1" "$2" 0x153 0x271 0x61 0xda; }   # the opponent's card

part_central() {
    user_in restomod
    walk "$piece" "$to_central,$car_16" "mark:car=16" 1 "$work/a.ppm" || fail "Restomod: the car arrow did not reach car 16"
    log | grep -q "car piece: test-car is car 16" || fail "the log does not say the piece is car 16"
    [ "$(log | grep -m1 "^mark:car=")" = "mark:car=6" ] || fail "a fresh player's Control Central does not open on car 6: $(log | grep -m1 "^mark:car=")"
    user_in restomod
    walk "$piece" "$to_central,$car_16" "mark:car=16" 1 "$work/a2.ppm" || fail "Restomod: the second walk did not reach car 16"
    card_box "$work/a.ppm" "$work/a2.ppm" && fail "car 16's card box is not the same in two runs"
    user_in restomod
    walk "$other" "$to_central,$car_16" "mark:car=16" 1 "$work/o.ppm" || fail "Restomod: the car arrow did not reach the other card's car 16"
    [ -s "$work/o.ppm" ] && ! card_box "$work/a.ppm" "$work/o.ppm" && fail "car 16's card box does not follow its card.png"
}

gallery="$to_central,$car_16,$(click 180 200),$(wait_for screen=added_gallery)"
key() { printf '300:%s,%s' "$1" "$(wait_for "gallery=$2")"; }   # a key, and the page the gallery shows after it: item, skin, paint
red="$gallery,$(key DOWN '1 0 0'),$(key RIGHT '1 0 1')"
striped="$red,$(key UP '0 0 1'),$(key RIGHT '0 1 1')"

part_gallery() {
    user_in restomod
    walk "$piece" "$gallery" "mark:gallery=0 0 0" 1 "$work/g0.ppm" || fail "a click on car 16's card did not open its gallery"
    user_in restomod
    walk "$piece" "$red" "mark:gallery=1 0 1" 1 "$work/g1.ppm" || fail "the gallery's Color did not go to Red"
    user_in restomod
    walk "$piece" "$striped" "mark:gallery=0 1 1" 1 "$work/g2.ppm" || fail "the gallery's Skin did not go to stripes"
    # the swatch (added_gallery_glue.c: 18 px at ARROW_RIGHT_X - 28, COLOUR_Y + 4..19) and the Skin row's value (SKIN_Y, past its arrow)
    [ -s "$work/g1.ppm" ] && ! box "$work/g0.ppm" "$work/g1.ppm" 442 459 360 375 && fail "Color Red shows no swatch"
    [ -s "$work/g2.ppm" ] && ! box "$work/g1.ppm" "$work/g2.ppm" 180 465 316 345 && fail "Skin stripes does not show its name"
}

part_kept() {
    user_in restomod
    done="$striped,$(key DOWN '1 1 1'),$(key DOWN '2 1 1'),$(key DOWN '3 1 1')"
    done="$done,$(key DOWN '4 1 1'),300:ENTER,$(wait_for car=16),$(click 571 453),$(wait_for screen=game_mode)"
    walk "$piece" "$done" "mark:screen=game_mode" 2 "" || fail "Done did not come back to Control Central on car 16, or Cancel did not close it"
    [ "$(log | grep "^mark:car=" | tail -1)" = "mark:car=16" ] || fail "Control Central was not on car 16 after the gallery"
    grep -q "^car=test-car$" "$work/user/restomod.cfg" 2>/dev/null || fail "restomod.cfg does not keep car=test-car: $(cat "$work/user/restomod.cfg" 2>/dev/null)"
    grep -q "^look.test-car=stripes,Red$" "$work/user/restomod.cfg" 2>/dev/null || fail "Done did not keep stripes,Red: $(cat "$work/user/restomod.cfg" 2>/dev/null)"
    kept=$(od -An -t d2 -j 155 -N 2 "$work/user/config.dat" 2>/dev/null | tr -d ' ')   # the player's car, 00524e8b (config_dat.c)
    case $kept in [0-7]) ;; *) fail "config.dat's car is not an original: '$kept'" ;; esac
    rm -f "$work/user/spirited-drive.log"
    walk "$piece" "$to_central" "mark:car=" 1 "" || fail "Control Central did not open again"
    [ "$(log | grep -m1 "^mark:car=")" = "mark:car=16" ] || fail "Control Central opened again on $(log | grep -m1 "^mark:car="), not car 16"
}

part_rival() {   # a fresh player's race type is head to head: the opponent arrow once from 7
    rival="$to_central,$car_16,$(click 614 156),$(wait_for opponent=16)"
    user_in restomod
    walk "$piece" "$rival" "mark:opponent=16" 1 "$work/r16.ppm" || fail "the opponent arrow did not reach car 16"
    user_in restomod
    walk "$other" "$rival" "mark:opponent=16" 1 "$work/o16.ppm" || fail "the opponent arrow did not reach the other card's car 16"
    [ -s "$work/o16.ppm" ] && ! rival_box "$work/r16.ppm" "$work/o16.ppm" && fail "car 16's rival card box does not follow its card.png"
    user_in restomod
    hold=10 walk "$piece" "$rival,$(click 56 453),$(wait_for spray_v4=1)" "mark:spray_v4=1" 1 "" || fail "Race with car 16 against car 16 did not run"
    grep -q "^opponent=test-car$" "$work/user/restomod.cfg" 2>/dev/null || fail "the race did not keep opponent=test-car"
}

part_modes() {
    two="$(car_on),$(car_on)"
    for mode in stock restored; do
        user_in $mode
        walk "$piece" "$to_central,$two" "mark:car=" 3 "$work/$mode.piece.ppm" || fail "$mode with the piece: the car arrow did not step twice"
        log | grep -q "^car piece:" && fail "$mode loaded the car piece: $(log | grep "^car piece:" | head -1)"
        log | grep -q "^mark:car=16" && fail "$mode offered car 16"
        [ -e "$work/user/restomod.cfg" ] && fail "$mode wrote restomod.cfg"
        user_in $mode
        walk "$nomods" "$to_central,$two" "mark:car=" 3 "$work/$mode.ppm" || fail "$mode without the piece: the car arrow did not step twice"
        cmp -s "$work/$mode.piece.ppm" "$work/$mode.ppm" || fail "$mode's Control Central is not the same with the piece in --mods"
    done
}

# the replay list: Options, Load/Save, View replay
replays="$(click 215 450),$(wait_for screen=options),$(click 240 324),3000:MOUSE:155:280,$(click 155 280),$(wait_for screen=game_list)"
# Rusty Springs (track 3: a circuit, one race; Coastal's three segments would go on), the track arrow twice from a fresh player's 1; one
# lap with the harness's autopilot; three Enters through the results to Control Central again
rusty="$(click 535 319),$(wait_for card=),$(click 535 319),$(wait_for card=3)"
results="0:WAIT:mark:race_over:300,5000:ENTER,5000:ENTER,5000:ENTER,$(wait_for car=16)"

part_replays() {
    user_in restomod
    extra="TNFS_AUTOPILOT=1 TNFS_LAPS=1" walk "$piece" "$to_central,$car_16,$rusty,$(click 56 453),$results,$replays" "mark:screen=game_list" 1 "" ||
        fail "a race with car 16 from the menus did not come back to the replay list"
    [ -s "$work/user/Replay.rpx" ] || fail "the race with car 16 from the menus kept no Replay.rpx"
    log | grep -q "^mark:list_row=Replay$" || fail "the replay list does not show Replay.rpx: $(log | grep "^mark:list_row=" | tr '\n' ' ')"
    rm -f "$work/user/spirited-drive.log"
    walk "$nomods" "$to_central,$replays" "mark:screen=game_list" 1 "" || fail "the replay list did not open without the piece"
    log | grep -q "^mark:list_row=Replay - needs test-car$" ||
        fail "without the piece the row does not say it needs test-car: $(log | grep "^mark:list_row=" | tr '\n' ' ')"
}

for part in $parts; do
    case $part in
    central|gallery|kept|rival|modes|replays) "part_$part" ;;
    *) echo "FAIL: no part $part" >&2; exit 2 ;;
    esac
done
[ "$failures" -eq 0 ] && echo "car_card_menus ($parts): all checks passed"
exit "$failures"
