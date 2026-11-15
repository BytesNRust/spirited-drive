#!/bin/sh
# tnfs-studio car new (dep59 tools, stage A): a car piece made from one of the game's cars. The piece's three files are the
# template's under the new stem, its mod.ini says what it is, the port's masks come along; a stem of the CD's or a folder already
# holding the piece is refused before anything is written, and nothing in the data roots is touched. validate checks a piece as the
# game will load it (car_piece_check), and a card it cannot read is an error. mod pack makes the zip the game installs, the same bytes
# each time, refusing a file the installation would refuse and writing nothing then. With the game: the piece races in Restomod and
# draws the frame the Viper draws (skipped when the game does not take a car's id in --car).
# usage: car_new.sh STUDIO ORIGINAL_DIR PORT_DATA_DIR [TNFS]
set -u
studio=$1
orig=$2
port=$3
tnfs=${4:-}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }

[ -f "$orig/SIMDATA/CARFAMS/DVIPER.CFM" ] || { echo "skip: no DVIPER.CFM"; exit 0; }
touch "$work/marker"
sleep 1

# 1. the Viper as the template
p="$work/p"
"$studio" car new --template viper --id test-car --stem TESTCAR --out "$p" --data "$orig" --port-data "$port" >"$work/log" 2>&1 ||
    fail "car new: $(cat "$work/log")"
[ -f "$p/mod.ini" ] || fail "no mod.ini"
for f in SIMDATA/CARFAMS/TESTCAR.CFM SIMDATA/CARFAMS/TESTCAR.PDN SIMDATA/CARSPECS/TESTCAR.PBS; do
    [ -f "$p/$f" ] || fail "no $f"
done
for line in type=car stem=TESTCAR sound_from=viper ai_from=viper id=test-car; do
    grep -qx "$line" "$p/mod.ini" 2>/dev/null || fail "mod.ini lacks the line $line"
done
cmp -s "$p/SIMDATA/CARFAMS/TESTCAR.CFM" "$orig/SIMDATA/CARFAMS/DVIPER.CFM" || fail "the CFM is not the Viper's"
cmp -s "$p/SIMDATA/CARSPECS/TESTCAR.PBS" "$orig/SIMDATA/CARSPECS/DVIPER.PBS" || fail "the PBS is not the Viper's"
[ -e "$p/SIMDATA/CARFAMS/MTESTCAR.CFM" ] && fail "the reduced CFM was copied"
if [ -d "$port/car_masks/DVIPER" ]; then
    (cd "$port/car_masks/DVIPER" && ls) > "$work/want"
    (cd "$p/car_masks/TESTCAR" 2>/dev/null && ls) > "$work/got"
    cmp -s "$work/want" "$work/got" || fail "car_masks/TESTCAR does not hold the Viper's masks"
fi
grep -q "test-car" "$work/log" || fail "the summary does not name the car: $(cat "$work/log")"

# 2. the template by its stem gives the same tree
"$studio" car new --template DVIPER --id test-car --stem TESTCAR --out "$work/q" --data "$orig" --port-data "$port" >/dev/null 2>&1 ||
    fail "car new --template DVIPER failed"
diff -r "$p" "$work/q" >/dev/null 2>&1 || fail "--template DVIPER gives another tree than --template viper"

# 3. a stem of the CD's is refused, named, and nothing is written
for stem in DVIPER mrx7 MTSUPRA; do
    out="$work/bad_$stem"
    "$studio" car new --template viper --id test-car --stem "$stem" --out "$out" --data "$orig" --port-data "$port" >/dev/null 2>"$work/log" &&
        fail "--stem $stem was accepted"
    grep -q "$stem" "$work/log" || fail "the refusal of --stem $stem does not name it: $(cat "$work/log")"
    [ -e "$out" ] && fail "--stem $stem left $out behind"
done

# 4. the same folder again: refused without --force and left as it was; --force writes it
find "$p" -type f -exec cksum {} + | sort > "$work/before"
"$studio" car new --template viper --id test-car --stem TESTCAR --out "$p" --data "$orig" --port-data "$port" >/dev/null 2>&1 &&
    fail "a second run over the piece was accepted without --force"
find "$p" -type f -exec cksum {} + | sort > "$work/after"
cmp -s "$work/before" "$work/after" || fail "a refused run changed the piece"
"$studio" car new --template viper --id test-car --stem TESTCAR --out "$p" --data "$orig" --port-data "$port" --force >/dev/null 2>&1 ||
    fail "--force was refused"

# 5. a bad id, an unknown template, no --out
"$studio" car new --template viper --id Bad_Id --stem TESTCAR --out "$work/r" --data "$orig" >/dev/null 2>&1 && fail "--id Bad_Id was accepted"
"$studio" car new --template lada --id test-car --stem TESTCAR --out "$work/r" --data "$orig" >/dev/null 2>&1 && fail "--template lada was accepted"
"$studio" car new --template viper --id test-car --stem TESTCAR --data "$orig" >/dev/null 2>"$work/log" && fail "no --out was accepted"
grep -q "usage" "$work/log" || fail "no --out does not print the usage: $(cat "$work/log")"
[ -e "$work/r" ] && fail "a refused run left $work/r behind"

# 6. the data roots are only read
newer=$(find -H "$orig" "$port" -newer "$work/marker" -print 2>/dev/null | head -3)
[ -z "$newer" ] || fail "car new wrote into a data root: $newer"

# validate: the piece passes; a PBS under another stem, a stem of the CD's, a card with no id are refused, the first two only as
# warnings with --port-only; a folder with no mod.ini is validated as before (studio_cli.sh)
"$studio" validate "$p" >"$work/log" 2>&1 || fail "validate of the new piece: $(cat "$work/log")"
grep -q "no findings" "$work/log" || fail "the new piece has findings: $(cat "$work/log")"
cp -R "$p" "$work/other"
mv "$work/other/SIMDATA/CARSPECS/TESTCAR.PBS" "$work/other/SIMDATA/CARSPECS/OTHER.PBS"
"$studio" validate "$work/other" >"$work/log" 2>&1 && fail "a piece with OTHER.PBS passed validate"
grep -q "PIECE.FILE_MISSING" "$work/log" || fail "no PIECE.FILE_MISSING for OTHER.PBS: $(cat "$work/log")"
grep -q "SIMDATA/CARSPECS/TESTCAR.PBS" "$work/log" || fail "the missing PBS is not named: $(cat "$work/log")"
"$studio" validate --port-only "$work/other" >"$work/log" 2>&1 || fail "--port-only refused the OTHER.PBS piece: $(cat "$work/log")"
grep -q "^warning PIECE.FILE_MISSING" "$work/log" || fail "--port-only did not warn of the missing PBS: $(cat "$work/log")"
cp -R "$p" "$work/cdstem"
sed 's/^stem=TESTCAR$/stem=DVIPER/' "$p/mod.ini" > "$work/cdstem/mod.ini"
"$studio" validate "$work/cdstem" >"$work/log" 2>&1 && fail "a piece with stem=DVIPER passed validate"
grep -q "PIECE.STEM" "$work/log" || fail "no PIECE.STEM for stem=DVIPER: $(cat "$work/log")"
"$studio" validate --port-only "$work/cdstem" >"$work/log" 2>&1 || fail "--port-only refused stem=DVIPER: $(cat "$work/log")"
grep -q "^warning PIECE.STEM" "$work/log" || fail "--port-only did not warn of stem=DVIPER: $(cat "$work/log")"
cp -R "$p" "$work/noid"
grep -v '^id=' "$p/mod.ini" > "$work/noid/mod.ini"
"$studio" validate "$work/noid" >"$work/log" 2>&1 && fail "a card with no id passed validate"
grep -q "mod.ini" "$work/log" && grep -qi "id" "$work/log" || fail "the refusal of a card with no id does not say why: $(cat "$work/log")"

# mod pack: the piece's zip validates; mod.ini comes first; a file the game cannot take is refused and named, with no zip; the system's
# litter is left out; the same piece gives the same zip; an existing zip is kept without --force; a piece that fails validate is refused
"$studio" mod pack "$p" "$work/test-car.zip" >"$work/log" 2>&1 || fail "mod pack: $(cat "$work/log")"
"$studio" validate "$work/test-car.zip" >"$work/log" 2>&1 || fail "the packed zip does not validate: $(cat "$work/log")"
if command -v unzip >/dev/null 2>&1; then
    unzip -Z1 "$work/test-car.zip" > "$work/list" 2>/dev/null
    [ "$(head -1 "$work/list")" = "mod.ini" ] || fail "mod.ini is not the zip's first entry: $(head -3 "$work/list")"
    for f in SIMDATA/CARFAMS/TESTCAR.CFM SIMDATA/CARFAMS/TESTCAR.PDN SIMDATA/CARSPECS/TESTCAR.PBS; do
        grep -qx "$f" "$work/list" || fail "the zip lacks $f"
    done
    unzip -tq "$work/test-car.zip" >/dev/null 2>&1 || fail "unzip finds the zip damaged"
fi
cp -R "$p" "$work/docx"
echo notes > "$work/docx/notes.docx"
"$studio" mod pack "$work/docx" "$work/test-car2.zip" >/dev/null 2>"$work/log" && fail "a piece with notes.docx was packed"
grep -q "notes.docx" "$work/log" || fail "the refusal does not name notes.docx: $(cat "$work/log")"
[ -e "$work/test-car2.zip" ] && fail "a refused pack left test-car2.zip"
cp -R "$p" "$work/litter"
echo x > "$work/litter/.DS_Store"
"$studio" mod pack "$work/litter" "$work/litter.zip" >"$work/log" 2>&1 || fail "a .DS_Store refused the pack: $(cat "$work/log")"
if command -v unzip >/dev/null 2>&1; then
    unzip -Z1 "$work/litter.zip" 2>/dev/null | grep -q "DS_Store" && fail "the zip carries .DS_Store"
fi
"$studio" mod pack "$p" "$work/again.zip" >/dev/null 2>&1 || fail "the second pack failed"
cmp -s "$work/test-car.zip" "$work/again.zip" || fail "two packs of one piece differ"
cksum < "$work/test-car.zip" > "$work/before"
"$studio" mod pack "$work/litter" "$work/test-car.zip" >/dev/null 2>&1 && fail "an existing zip was overwritten without --force"
cksum < "$work/test-car.zip" | cmp -s - "$work/before" || fail "a refused pack changed the existing zip"
"$studio" mod pack "$p" "$work/test-car.zip" --force >/dev/null 2>&1 || fail "--force was refused"
"$studio" mod pack "$work/other" "$work/other.zip" >/dev/null 2>"$work/log" && fail "the OTHER.PBS piece was packed"
[ -e "$work/other.zip" ] && fail "a piece that fails validate left a zip"

# the game: the piece made from the Viper races with --car test-car and, alone on the road with the mode's liveries off (as
# car_piece_race.sh), draws the Viper's own frame; that is why car new copies the masks
export SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0
restomod() {
    rm -rf "$work/user"
    mkdir -p "$work/user" && echo "profile=restomod" > "$work/user/port.cfg" && echo "extra_liveries=0" > "$work/user/restomod.cfg"
}
# a game that takes an id in --car answers an unknown one with "no car <id>" (its --help shows only N)
takes_id=0
if [ -n "$tnfs" ]; then
    restomod
    timeout -k 5 60 "$tnfs" --data "$orig" --user "$work/user" --drive TR1 --car no-such-car --node 1 --screenshot "$work/probe.ppm" \
        >/dev/null 2>"$work/err"
    grep -q "no car no-such-car" "$work/err" && takes_id=1
fi
if [ "$takes_id" -eq 1 ]; then
    restomod
    timeout -k 5 120 "$tnfs" --data "$orig" --user "$work/user" --mods "$p" --drive TR1 --car test-car --racers 1 --node 200 \
        --screenshot "$work/a.ppm" >/dev/null 2>"$work/err" || fail "the piece did not race: $(tail -3 "$work/err")"
    [ -s "$work/a.ppm" ] || fail "no frame of the piece"
    restomod
    timeout -k 5 120 "$tnfs" --data "$orig" --user "$work/user" --drive TR1 --car 5 --racers 1 --node 200 \
        --screenshot "$work/b.ppm" >/dev/null 2>"$work/err" || fail "the Viper did not race: $(tail -3 "$work/err")"
    cmp -s "$work/a.ppm" "$work/b.ppm" || fail "the piece made from the Viper does not look like the Viper"
else
    echo "car_new: the game's --car takes no id (or no game given): the race is skipped"
fi

[ "$failures" -eq 0 ] || exit 1
echo "car_new: ok"
