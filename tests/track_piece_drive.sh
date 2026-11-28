#!/bin/sh
# A track piece races (dep59 step 5, T4): track new --piece makes one, --drive <id> runs it in Restomod from the first stage and draws the frame;
# Stock and Restored do not have it; a nonsense id is refused by name.
# usage: track_piece_drive.sh TNFS STUDIO ORIGINAL_DIR
set -u
tnfs=$1
studio=$2
orig=$3
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
export SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIODRIVER=dummy

[ -f "$orig/SIMDATA/MISC/AL1.TRI" ] || { echo "skip: no AL1.TRI"; exit 77; }
printf 'section 24\nsection 40 turn=90 grade=3 bank=6 width=50\nsection 30 turn=-120 grade=-3 bank=-6\nsection 30 bank=0 grade=0 width=40\n' > "$work/loop.trk"
"$studio" track new "$work/loop.trk" --template AL1 --piece --id demo-stage --stem DEMOST --out "$work/piece" --data "$orig" >"$work/log" 2>&1 || { echo "FAIL: track new --piece: $(cat "$work/log")" >&2; exit 1; }

run() {   # PROFILE, then the arguments
    profile=$1; shift
    mkdir -p "$work/user_$profile" && echo "profile=$profile" > "$work/user_$profile/port.cfg"
    timeout -k 5 120 "$tnfs" --data "$orig" --user "$work/user_$profile" "$@" >"$work/out" 2>"$work/err"
}

run restomod --mods "$work/piece" --drive demo-stage --car 5 --racers 1 --node 200 --screenshot "$work/a.ppm" || fail "the piece did not race: $(tail -3 "$work/err")"
[ -s "$work/a.ppm" ] || fail "no frame from the piece"
grep -q "track piece: demo-stage is track 16" "$work/user_restomod/spirited-drive.log" || fail "the log does not say the piece is track 16"
run restomod --mods "$work/piece" --drive demo-stage --car 5 --racers 1 --node 400 --screenshot "$work/b.ppm" || fail "the piece did not race to node 400"
[ "$(cksum < "$work/a.ppm")" != "$(cksum < "$work/b.ppm")" ] || fail "the frames at 200 and 400 are the same"
run restomod --mods "$work/piece" --drive no-such-track --car 5 --racers 1 --node 200 --screenshot "$work/c.ppm" && fail "an unknown id raced"
grep -q "no-such-track" "$work/err" || fail "the refusal does not name the id: $(cat "$work/err")"
for profile in stock restored; do
    run "$profile" --mods "$work/piece" --drive demo-stage --car 5 --racers 1 --node 200 --screenshot "$work/d.ppm" && fail "$profile raced an added track"
done

[ "$failures" -eq 0 ] && echo "track_piece_drive: ok"
exit $((failures != 0))
