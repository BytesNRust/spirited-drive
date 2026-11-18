#!/bin/sh
# dep59 step 3: an added car in the menus, against the real binary and the CD data. Every step waits on the game's own marks
# (mark:car= / mark:opponent= from Control Central, mark:gallery=<item> <skin> <paint> from the added car's gallery, mark:list_row= from the
# replay list), and the pictures are the script's SHOTs (the frame on screen once it is still); one game session walks through several checks.
# PART (one ctest each, so they run side by side):
#   cards    Restomod: Control Central's car arrow twice from a fresh player's car 6 is the added car (16); its card.png is drawn (the card's
#            box follows the piece's card: another name, another box, the game drawing it; the same box when Control Central opens
#            again; card_name=off draws none); as the head to head rival its rival card is drawn (its box follows the name too). A click on its card opens its gallery: Color one on (Red)
#            shows its swatch, Skin one on (stripes) shows its name; Done comes back to car 16 and Cancel closes Control Central. Started
#            again, restomod.cfg keeps car=test-car and look.test-car=stripes,Red, Control Central opens on car 16, and config.dat's car is an
#            original (0..7).
#   modes    Stock and Restored with the piece in --mods: no added car, the same picture as without it, no restomod.cfg.
#   steps    Restomod: the gallery's Previous and Next go to the next car's gallery, whatever it is. Previous clicked from car 16 opens
#            the Diablo's showcase, which stays open (the click's button, still down, is not taken there as Done) until Escape; Next from
#            car 16 passes over the Warrior (no gallery) to the RX-7's showcase rather than back to Control Central.
#   replays  a whole race of car 16 against car 16 from the menus (Race): the replay list shows its Replay.rpx, restomod.cfg keeps the
#            rival; without the piece the row says which mod it needs.
# usage: car_card_menus.sh TNFS STUDIO REPO ORIGINAL_DIR [PART] (no PART: all of them, in turn)
set -u
abs() { case $1 in /*) echo "$1" ;; *) echo "$PWD/$1" ;; esac; }   # the walks run the game from the scratch folder
tnfs=$(abs "$1")
studio=$(abs "$2")
repo=$(abs "$3")
orig=$(abs "$4")
parts=${5:-cards modes steps replays}
[ -f "$orig/SIMDATA/CARSPECS/DVIPER.PBS" ] || { echo "skip: no CD in $orig"; exit 0; }
work=$(mktemp -d)
trap 'find "$work" -delete' EXIT
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
# the same piece and card.png with another name ("Other Car"): the card's box follows the name the game draws over it (showcase plan
# section 1), and with card_name=off ("plain") it holds none: the same box as Other Car's but in the name's rows
other=$work/other
cp -R "$piece" "$other"
sed 's/^name=.*/name=Other Car/' "$piece/mod.ini" > "$other/mod.ini"
cmp -s "$piece/card.png" "$other/card.png" || { echo "FAIL: the other piece's card.png is not the same picture" >&2; exit 1; }
plain=$work/plain
cp -R "$other" "$plain"
printf 'card_name=off\n' >> "$plain/mod.ini"
nomods=$work/nomods
mkdir -p "$nomods" "$work/shots"

# a player folder in mode $1, fresh. In Restomod with the edition "walk" (dep59 step 4: the arrows step through an edition's cars, the
# standard edition's are the original's): the eight, the piece, the Warrior, as the arrows went before editions
user_in() {
    [ -d "$work/user" ] && find "$work/user" -delete
    mkdir -p "$work/user" && printf 'profile=%s\n' "$1" > "$work/user/port.cfg"
    [ "$1" = restomod ] || return 0
    mkdir -p "$work/user/editions" && printf 'edition=walk\n' > "$work/user/restomod.cfg"
    printf 'name=Walk\ncars=rx7,nsx,supra,911,zr1,viper,512tr,diablo,test-car,warrior\ntracks=city,coastal,alpine,rusty,autumn,burnt,vertigo,transtropolis,vegas\n' \
        > "$work/user/editions/walk.ini"
}
log() { grep -v "^input script" "$work/user/spirited-drive.log" 2>/dev/null; }
wait_for() { printf '0:WAIT:mark:%s:%s' "$1" "$step"; }
click() { printf '300:CLICK:%s:%s' "$1" "$2"; }
shot() { printf '0:SHOT:%s' "$1"; }   # the frame on screen once it is still, as $work/shots/$1.ppm
# the way in: our boot screen, EA's first movie, the title (a key ends its wait), Game Mode, Single, the name box, Control Central
# (the boot screen logs its mark before the script's first step is armed: the first key goes at a time)
to_central="1500:ESC,$(wait_for movie=),300:ESC,$(wait_for screen=title),300:ESC,$(wait_for screen=game_mode)"
to_central="$to_central,$(click 310 215),$(wait_for screen=name_box),300:TEXT:R,300:ENTER,$(wait_for screen=central)"
car_on() { printf '%s,%s' "$(click 319 197)" "$(wait_for car=)"; }   # the car arrow's right (central_select.c), the mark its card logs
# a fresh player starts on car 6, the head to head rival 7; Restomod's list goes 0..7, the added cars, the Warrior (11): two on is car 16
car_16="$(car_on),$(click 319 197),$(wait_for car=16)"
rival_16="$(click 614 156),$(wait_for opponent=16)"   # the opponent arrow's right, once from 7 (a fresh player's race type: head to head)

# walk MODS SCRIPT: the game run with the input script, which ends on a WAIT for a mark that never comes, a second long: the game stops there
# (status 3, that step named), every step before it taken. Set for one walk: extra (more of the game's environment). 0 when so
walk() {
    mods=$1 script="$2,0:WAIT:mark:walk_end:1"
    last=$(printf '%s' "$script" | tr ',' '\n' | grep -c .)
    (cd "$work" && env ${extra:-} TNFS_VERBOSE=1 TNFS_INPUT_SETTLE=500 TNFS_SHOT_DIR="$work/shots" TNFS_INPUT_SCRIPT="$script" \
        timeout -k 5 600 "$tnfs" --data "$orig" --mods "$mods" --user "$work/user" >"$work/out" 2>&1)
    rc=$? extra=""   # sh keeps a function's prefix assignments: extra is for one walk only
    if [ $rc -eq 3 ] && grep -q "input script: step $last: WAIT mark:walk_end timed out" "$work/user/spirited-drive.log"; then
        grep -q "bad TNFS_INPUT_SCRIPT item" "$work/user/spirited-drive.log" || return 0
        echo "walk: $(grep -m1 "bad TNFS_INPUT_SCRIPT item" "$work/user/spirited-drive.log")" >&2   # a step dropped: not the walk written
        return 1
    fi
    echo "walk: status $rc; $(grep "^input script: step.*\(timed out\|not written\)" "$work/user/spirited-drive.log" | head -1); the log ends: $(log | tail -3); the game said: $(tail -2 "$work/out")" >&2
    return 1
}
# box A B X0 X1 Y0 Y1: 0 when the two shots (PPM) differ inside that box of the 640x480 screen (ends included)
box() {
    python3 -I - "$work/shots/$1.ppm" "$work/shots/$2.ppm" "$3" "$4" "$5" "$6" <<'PY'
import sys
def pixels(path):
    data = open(path, "rb").read()
    fields, at = [], 0
    while len(fields) < 4:   # P6, width, height, maximum, each after blanks
        while data[at:at + 1].isspace(): at += 1
        start = at
        while not data[at:at + 1].isspace(): at += 1
        fields.append(data[start:at])
    return int(fields[1]), data[at + 1:]
x0, x1, y0, y1 = (int(v, 0) for v in sys.argv[3:7])
(wa, pa), (wb, pb) = pixels(sys.argv[1]), pixels(sys.argv[2])
rows = lambda w, p: [p[(y * w + x0) * 3:(y * w + x1 + 1) * 3] for y in range(y0, y1 + 1)]
sys.exit(0 if rows(wa, pa) != rows(wb, pb) else 1)
PY
}
card_box() { box "$1" "$2" 0x13 0x14a 0x70 0x11a; }    # the player's card (central_follow.c's hit box)
rival_box() { box "$1" "$2" 0x153 0x271 0x61 0xda; }   # the opponent's card
has_shot() { [ -s "$work/shots/$1.ppm" ]; }

key() { printf '300:%s,%s' "$1" "$(wait_for "gallery=$2")"; }   # a key, and the page the gallery shows after it: item, skin, paint

part_cards() {
    # one session: car 16 and its card, car 16 as the rival and its card, its gallery (Red, then stripes), Done, Cancel
    # the SETUP panel (Enter on the tile): Color's row steps to Red, Skin's to the stripes, Escape closes the panel and not the page; then the
    # tiles' and the bottom row's items: History, Previous, Next, Done
    gallery="$(click 180 200),$(wait_for screen=added_gallery),$(shot g0),300:ENTER,$(wait_for 'gallery=0 0 0'),$(shot g1a),$(key RIGHT '0 0 1'),$(shot g1b)"
    gallery="$gallery,$(key DOWN '0 0 1'),$(key RIGHT '0 1 1'),$(shot g2),$(key ESC '0 1 1'),$(shot g3),$(key DOWN '6 1 1'),$(key DOWN '2 1 1'),$(key DOWN '3 1 1')"
    gallery="$gallery,$(key DOWN '4 1 1'),300:ENTER,$(wait_for car=16),$(click 571 453),$(wait_for screen=game_mode)"
    user_in restomod
    walk "$piece" "$to_central,$car_16,$(shot a),$rival_16,$(shot r16),$gallery" || fail "Restomod: car 16, its rival card, its gallery, Done and Cancel"
    log | grep -q "car piece: test-car is car 16" || fail "the log does not say the piece is car 16"
    [ "$(log | grep -m1 "^mark:car=")" = "mark:car=6" ] || fail "a fresh player's Control Central does not open on car 6: $(log | grep -m1 "^mark:car=")"
    [ "$(log | grep "^mark:car=" | tail -1)" = "mark:car=16" ] || fail "Control Central was not on car 16 after the gallery"
    # the SETUP panel (gallery_layout.h: GL_SETUP_X 57, GL_SETUP_Y 100, 300x170; its rows from +44 down by 24): opening it changes the tile and
    # General's top; Color's value (row 0, y 144..158) shows Red; Skin's (row 1, y 168..182) its stripes; Escape closes it (the tile as it was)
    has_shot g1a && ! box g0 g1a 60 295 105 265 && fail "Enter on SETUP did not open the panel"
    has_shot g1b && ! box g1a g1b 150 295 144 158 && fail "Color Red does not show in the panel"
    has_shot g2 && ! box g1b g2 150 295 168 182 && fail "Skin stripes does not show in the panel"
    has_shot g3 && box g0 g3 60 295 105 265 && fail "Escape did not close the panel"
    grep -q "^car=test-car$" "$work/user/restomod.cfg" 2>/dev/null || fail "restomod.cfg does not keep car=test-car: $(cat "$work/user/restomod.cfg" 2>/dev/null)"
    grep -q "^look.test-car=stripes,Red$" "$work/user/restomod.cfg" 2>/dev/null || fail "Done did not keep stripes,Red: $(cat "$work/user/restomod.cfg" 2>/dev/null)"
    kept=$(od -An -t d2 -j 155 -N 2 "$work/user/config.dat" 2>/dev/null | tr -d ' ')   # the player's car, 00524e8b (config_dat.c)
    case $kept in [0-7]) ;; *) fail "config.dat's car is not an original: '$kept'" ;; esac
    # started again: Control Central opens on car 16, its card's box as before
    rm -f "$work/user/spirited-drive.log"
    # (the pointer over the car arrow, as for shot a: it lights the card's frame)
    walk "$piece" "$to_central,300:MOUSE:319:197,$(shot a2)" || fail "Control Central did not open again"
    [ "$(log | grep -m1 "^mark:car=")" = "mark:car=16" ] || fail "Control Central opened again on $(log | grep -m1 "^mark:car="), not car 16"
    has_shot a2 && card_box a a2 && fail "car 16's card box is not the same when Control Central opens again"
    # another name on the same card.png: car 16's card box and its rival box are other pictures (the game draws the name)
    user_in restomod
    walk "$other" "$to_central,$car_16,$(shot o),$rival_16,$(shot o16)" || fail "Restomod: the other name's car 16 and its rival card"
    has_shot o && ! card_box a o && fail "car 16's card box does not follow its name"
    has_shot o16 && ! rival_box r16 o16 && fail "car 16's rival card box does not follow its name"
    # card_name=off: no name; the card the same as Other Car's above the name's rows (the card's 151..159 from y 114, the rival card's
    # 102..110 from y 99, each with its shadow), not in them
    user_in restomod
    walk "$plain" "$to_central,$car_16,$(shot p),$rival_16,$(shot p16)" || fail "Restomod: card_name=off's car 16 and its rival card"
    has_shot p && box o p 0x13 0x14a 0x70 260 && fail "card_name=off: the card differs above its name's rows"
    has_shot p && ! box o p 0x13 0x14a 264 275 && fail "card_name=off: the card's name is still drawn"
    has_shot p16 && box o16 p16 0x153 0x271 0x61 196 && fail "card_name=off: the rival card differs above its name's rows"
    has_shot p16 && ! box o16 p16 0x153 0x271 200 210 && fail "card_name=off: the rival card's name is still drawn"
}

part_steps() {
    # Previous by a click on the bottom row's left arrow (x 250..259, y 447..461: over the showcase's own Previous, 200..260 x 430..480):
    # the Diablo's showcase, left by Escape only. The click taken again by the showcase would step it to the 512TR, and the WAIT for
    # car=7, armed by the Escape, would never come
    steps="$(click 180 200),$(wait_for screen=added_gallery),$(click 254 454),$(wait_for screen=showcase),1500:ESC,$(wait_for car=7)"
    # Next by the keys: Color, Previous, Next, Enter; the Warrior's page (dep59 section 8: its look fixed, it opens on Next: the mark
    # item 3, skin 0, paint 1, its purple), Enter there: the RX-7's showcase, Escape back to Control Central on it
    steps="$steps,$(car_on),$(click 180 200),$(wait_for screen=added_gallery),$(key DOWN '6 0 0'),$(key DOWN '2 0 0'),$(key DOWN '3 0 0')"
    steps="$steps,300:ENTER,$(wait_for 'gallery=3 0 1'),300:ENTER,$(wait_for screen=showcase),1500:ESC,$(wait_for car=0)"
    # the carousel (dep59 section 4, Restomod): the RX-7's showcase, its own Previous (200..260 x 430..480) leaves it for the car before
    # the RX-7 in the edition's order, round to the Warrior's page, which opens on Previous (the step it came by: item 2); Escape:
    # Control Central on car 11
    steps="$steps,$(click 180 200),$(wait_for screen=showcase),$(click 230 455),$(wait_for 'gallery=2 0 1'),300:ESC,$(wait_for car=11)"
    user_in restomod
    walk "$piece" "$to_central,$car_16,$steps" || fail "Restomod: the gallery's Previous and Next open the next car's gallery, the showcase's the next car's page"
    log | grep -q "the Warrior's \(figures\|card\)" && fail "the Warrior's page: $(log | grep -m1 "the Warrior's")"
}

part_modes() {
    for mode in stock restored; do
        user_in $mode
        walk "$piece" "$to_central,$(car_on),$(car_on),$(shot $mode-piece)" || fail "$mode with the piece: the car arrow did not step twice"
        log | grep -q "^car piece:" && fail "$mode loaded the car piece: $(log | grep "^car piece:" | head -1)"
        log | grep -q "^mark:car=16" && fail "$mode offered car 16"
        [ -e "$work/user/restomod.cfg" ] && fail "$mode wrote restomod.cfg"
        user_in $mode
        walk "$nomods" "$to_central,$(car_on),$(car_on),$(shot $mode)" || fail "$mode without the piece: the car arrow did not step twice"
        cmp -s "$work/shots/$mode-piece.ppm" "$work/shots/$mode.ppm" || fail "$mode's Control Central is not the same with the piece in --mods"
    done
}

# the replay list: Options, Load/Save, View replay
replays="$(click 215 450),$(wait_for screen=options),$(click 240 324),300:MOUSE:155:280,$(click 155 280),$(wait_for screen=game_list)"
# Rusty Springs (track 3: a circuit, one race; Coastal's three segments would go on), the track arrow twice from a fresh player's 1; one
# lap with the harness's autopilot; Enter on each results screen once it has faded in (its mark:results=NAME), to Control Central again
# (headless the pointer is not over Done at first: on the first screen one Enter wakes it there and a second takes Done)
rusty="$(click 535 319),$(wait_for card=),$(click 535 319),$(wait_for card=3)"
results="0:WAIT:mark:results=circuit:300,300:ENTER,300:ENTER,$(wait_for results=best_totals),300:ENTER,$(wait_for car=16)"

part_replays() {
    user_in restomod
    extra="TNFS_AUTOPILOT=1 TNFS_LAPS=1" walk "$piece" "$to_central,$car_16,$rival_16,$rusty,$(click 56 453),$results,$replays" ||
        fail "a race of car 16 against car 16 from the menus did not come back to the replay list"
    log | grep -q "^finish: 0=TESTCAR" || fail "the race of car 16 against car 16 did not finish"
    grep -q "^opponent=test-car$" "$work/user/restomod.cfg" 2>/dev/null || fail "the race did not keep opponent=test-car"
    [ -s "$work/user/Replay.rpx" ] || fail "the race with car 16 from the menus kept no Replay.rpx"
    log | grep -q "^mark:list_row=Replay$" || fail "the replay list does not show Replay.rpx: $(log | grep "^mark:list_row=" | tr '\n' ' ')"
    rm -f "$work/user/spirited-drive.log"
    walk "$nomods" "$to_central,$replays" || fail "the replay list did not open without the piece"
    log | grep -q "^mark:list_row=Replay - needs test-car$" ||
        fail "without the piece the row does not say it needs test-car: $(log | grep "^mark:list_row=" | tr '\n' ' ')"
}

for part in $parts; do
    case $part in
    cards|modes|steps|replays) "part_$part" ;;
    *) echo "FAIL: no part $part" >&2; exit 2 ;;
    esac
done
[ "$failures" -eq 0 ] && echo "car_card_menus ($parts): all checks passed"
exit "$failures"
