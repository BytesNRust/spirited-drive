#!/bin/sh
# tnfs-studio car new (dep59 tools, stage A): a car piece made from one of the game's cars. The piece's three files are the
# template's under the new stem, its mod.ini says what it is, the port's masks come along; a stem of the CD's or a folder already
# holding the piece is refused before anything is written, and nothing in the data roots is touched.
# usage: car_new.sh STUDIO ORIGINAL_DIR PORT_DATA_DIR
set -u
studio=$1
orig=$2
port=$3
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

[ "$failures" -eq 0 ] || exit 1
echo "car_new: ok"
