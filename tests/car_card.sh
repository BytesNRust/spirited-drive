#!/bin/sh
# port (dep59 step 3): tnfs-studio car card. A piece of car new gets its pictures from the game's renderer: card.png 309x166,
# rival.png 284x117, gallery.png 320x200 (the PNG's IHDR); the generic card.png car new wrote is replaced without --force, a picture
# the capture wrote is not; another node gives other pictures; the piece still validates; --generic writes the plain card again.
# Skipped without the CD or when the game takes no car id in --car.
# usage: car_card.sh TNFS STUDIO ORIGINAL_DIR PORT_DATA_DIR
set -u
tnfs=$1
studio=$2
orig=$3
port=$4
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }

[ -f "$orig/SIMDATA/CARFAMS/DVIPER.CFM" ] || { echo "skip: no DVIPER.CFM"; exit 0; }
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0
mkdir -p "$work/probe" && echo "profile=restomod" > "$work/probe/port.cfg"
timeout -k 5 60 "$tnfs" --data "$orig" --user "$work/probe" --drive TR1 --car no-such-car --node 1 --screenshot "$work/probe.ppm" \
    >/dev/null 2>"$work/err"
grep -q "no car no-such-car" "$work/err" || { echo "car_card: the game's --car takes no id: skipped"; exit 0; }

size() { od -An -tu1 -j16 -N8 "$1" 2>/dev/null | tr -s ' \n' ' ' | awk '{ printf "%dx%d", (($1 * 256 + $2) * 256 + $3) * 256 + $4, (($5 * 256 + $6) * 256 + $7) * 256 + $8 }'; }

p="$work/p"
"$studio" car new --template viper --id card-car --stem CARDCAR --out "$p" --data "$orig" --port-data "$port" >/dev/null 2>&1 || fail "car new"
cp "$p/card.png" "$work/generic.png"
timeout -k 5 150 "$studio" car card "$p" --tnfs "$tnfs" --data "$orig" >"$work/log" 2>&1 || fail "car card: $(cat "$work/log")"
grep -qE "ERROR: AddressSanitizer|runtime error:" "$work/log" && fail "car card: a sanitizer report"
for pic in card.png:309x166 rival.png:284x117 gallery.png:320x200; do
    f=${pic%%:*} want=${pic#*:}
    [ "$(size "$p/$f")" = "$want" ] || fail "$f measures $(size "$p/$f"), not $want"
done
cmp -s "$p/card.png" "$work/generic.png" && fail "card.png is still the generic one"
"$studio" validate "$p" >"$work/log" 2>&1 || fail "validate after car card: $(cat "$work/log")"
grep -q "no findings" "$work/log" || fail "the piece has findings after car card: $(cat "$work/log")"

# a captured card is kept without --force; another node with --force gives other pictures
cp "$p/card.png" "$work/first.png"
"$studio" car card "$p" --tnfs "$tnfs" --data "$orig" --node 600 >/dev/null 2>"$work/err" && fail "car card replaced a captured card without --force"
grep -q -- "--force" "$work/err" || fail "the refusal does not mention --force: $(cat "$work/err")"
timeout -k 5 150 "$studio" car card "$p" --tnfs "$tnfs" --data "$orig" --node 600 --force >"$work/log" 2>&1 || fail "car card --node 600: $(cat "$work/log")"
cmp -s "$p/card.png" "$work/first.png" && fail "node 600 gives the card of node 200"

# --generic writes the plain card again
"$studio" car card "$p" --generic --force >/dev/null 2>&1 || fail "car card --generic --force"
cmp -s "$p/card.png" "$work/generic.png" || fail "--generic does not give car new's card back"
"$studio" car card "$p" --tnfs "$tnfs" --data "$orig" --time noon >/dev/null 2>"$work/err" && fail "--time noon was accepted"
pgrep -f "$work" >/dev/null 2>&1 && fail "a game run was left behind"

[ "$failures" -eq 0 ] || exit 1
echo "car_card: ok"
