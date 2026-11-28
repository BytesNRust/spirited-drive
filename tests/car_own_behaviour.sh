#!/bin/sh
# dep59 3b: an added car races as its own, against the real binary and the CD data.
# 1. `car new` from the Viper, its dash_from line taken out: validate finds nothing; asked for the cockpit's view (TNFS_VIEW=0) it draws
#    no cockpit (not the Viper's cockpit frame, the same as its own first outside view).
# 2. Two pieces alike but for sound_from (viper, and 512tr, the one original whose lamp row is not the red: grey 6,6,6) draw the same
#    frames, lamps off and braking (TNFS_SCRIPT=brake): an added car's lamps are the standard red.
# 3. ai_level=top set on a piece after a run that kept its Replay.rpx: the replay is refused, naming the mod.
# usage: car_own_behaviour.sh TNFS STUDIO ORIGINAL_DIR
set -u
tnfs=$1
studio=$2
orig=$3
[ -f "$orig/SIMDATA/CARSPECS/DVIPER.PBS" ] || { echo "skip: no CD in $orig"; exit 77; }
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
export SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0
repo=$(cd "$(dirname "$0")/.." && pwd)

user_in() {   # a fresh player folder in Restomod, the mode's liveries off (an added car has no paint catalogue)
    rm -rf "$work/user"
    mkdir -p "$work/user" && echo "profile=restomod" > "$work/user/port.cfg" && echo "extra_liveries=0" > "$work/user/restomod.cfg"
}
shot() {   # the mods folder $1, the car $2, the frame $3 (env before: TNFS_VIEW, TNFS_SCRIPT)
    user_in
    timeout -k 5 120 "$tnfs" --data "$orig" --user "$work/user" ${1:+--mods "$1"} --drive TR1 --car "$2" --racers 1 --node 200 \
        --screenshot "$3" >/dev/null 2>"$work/err" || fail "$2 did not race for $3: $(tail -2 "$work/err")"
}
card_without() {   # the piece $1's mod.ini without the line starting $2
    grep -v "^$2" "$1/mod.ini" > "$work/card" && cp "$work/card" "$1/mod.ini"
}

# 1. car new, no dash_from: a good piece, and no cockpit
a=$work/a
"$studio" car new --template viper --id test-a --stem TESTA --out "$a" --data "$orig" --port-data "$repo/data" >"$work/log" 2>&1 ||
    fail "car new: $(cat "$work/log")"
card_without "$a" dash_from=
grep -q '^ai_from=' "$a/mod.ini" && fail "car new wrote ai_from"
"$studio" validate "$a" >"$work/log" 2>&1 || fail "validate of the piece without dash_from: $(cat "$work/log")"
TNFS_VIEW=0 shot "$a" test-a "$work/a0.ppm"
TNFS_VIEW=1 shot "$a" test-a "$work/a1.ppm"
cp "$work/user/Replay.rpx" "$work/kept.rpx" 2>/dev/null || fail "the run with test-a kept no Replay.rpx (an added car's replay)"
TNFS_VIEW=0 shot "" 5 "$work/viper0.ppm"
cmp -s "$work/a0.ppm" "$work/a1.ppm" || fail "the piece without dash_from drew a cockpit view of its own"
cmp -s "$work/a0.ppm" "$work/viper0.ppm" && fail "the piece without dash_from drew the Viper's cockpit"

# 2. the same piece with sound_from=512tr: the same frames, lamps off and braking
b=$work/b
cp -R "$a" "$b"
sed 's/^sound_from=.*/sound_from=512tr/' "$a/mod.ini" > "$b/mod.ini"
for script in none brake; do
    TNFS_VIEW=1 TNFS_SCRIPT=$script shot "$a" test-a "$work/la_$script.ppm"
    TNFS_VIEW=1 TNFS_SCRIPT=$script shot "$b" test-a "$work/lb_$script.ppm"
    cmp -s "$work/la_$script.ppm" "$work/lb_$script.ppm" || fail "sound_from changed the frame ($script): the lamps follow the engine"
done
cmp -s "$work/la_none.ppm" "$work/la_brake.ppm" && fail "the brake changed nothing on the frame: the lamps' check sees no lamp"

# 3. step 1's run kept its replay; with ai_level=top set since, it is refused, naming the mod
echo "ai_level=top" >> "$a/mod.ini"
user_in
timeout -k 5 120 "$tnfs" --data "$orig" --user "$work/user" --mods "$a" --replay "$work/kept.rpx" >/dev/null 2>"$work/err"
rc=$?
{ [ "$rc" -ne 0 ] && [ "$rc" -lt 128 ]; } || fail "the replay after ai_level=top: status $rc, not a clean refusal"
grep -q "replay: the mod test-a has changed" "$work/err" || fail "the replay after ai_level=top did not name the mod: $(tail -2 "$work/err")"

[ "$failures" -eq 0 ] && echo "car_own_behaviour: ok"
exit $((failures > 0))
