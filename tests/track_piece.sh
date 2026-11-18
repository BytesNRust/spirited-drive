#!/bin/sh
# An added track's whole life outside the menus (dep59 step 5, T8): a piece made with track new --piece runs to its finish and through its
# results screens (--drive <id>), keeps its record in race_records_tracks.dat by id and never in the original's file, is kept as Replay.rpx
# with the track's block, which plays back with the piece as it was and, without it or with it changed, does not play and says why (the
# text names the track's rules, not the cars').
#   usage: track_piece.sh TNFS STUDIO ORIGINAL_DIR [PART...]   (no PART: finish replay refusals stock; each part needs the ones before it)
set -u
abs() { case $1 in /*) echo "$1" ;; *) echo "$PWD/$1" ;; esac; }
tnfs=$(abs "$1")
studio=$(abs "$2")
orig=$(abs "$3")
shift 3
parts=${*:-finish replay refusals stock}
repo=$(cd "$(dirname "$0")/.." && pwd)
[ -f "$orig/SIMDATA/MISC/AL1.TRI" ] || { echo "skip: no AL1.TRI in $orig"; exit 0; }
work=$(mktemp -d)
trap '[ -n "${KEEP:-}" ] && echo "kept $work" >&2 || find "$work" -delete' EXIT
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
export SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0

printf 'section 24\nsection 40 turn=90 grade=3 bank=6 width=50\nsection 30 turn=-120 grade=-3 bank=-6\nsection 30 bank=0 grade=0 width=40\n' > "$work/loop.trk"
"$studio" track new "$work/loop.trk" --template AL1 --piece --id dune-run --stem DUNERUN --out "$work/piece" --data "$orig" >"$work/studio.log" 2>&1 ||
    { echo "FAIL: track new --piece: $(cat "$work/studio.log")" >&2; exit 1; }
piece=$work/piece
mkdir -p "$work/user" && echo "profile=restomod" > "$work/user/port.cfg"

# Enter on each results screen once it has faded in (its mark:results=NAME); headless the first screen needs a second Enter (the pointer is not over Done yet)
through() {
    first=1
    for name in "$@"; do
        printf ',0:WAIT:mark:results=%s:120,300:ENTER' "$name"
        [ $first -eq 1 ] && printf ',300:ENTER'
        first=0
    done
}
log() { grep -v "^input script" "$work/user/spirited-drive.log" 2>/dev/null; }
replay() {   # the replay file $1, the mods folder $2 (none: no --mods): the status; stderr in err, the trace in replay.out
    script="0:WAIT:mark:spray_v4=1:120,30000:ESC,3000:ENTER,3000:ENTER,3000:ENTER,3000:ESC"
    file=$1
    if [ -n "$2" ]; then set -- --mods "$2"; else set --; fi
    TNFS_BANDS=0 TNFS_VERBOSE=1 TNFS_TRACE_PLAYER=30 TNFS_INPUT_SETTLE=300 TNFS_INPUT_SCRIPT=$script \
        timeout -k 5 120 "$tnfs" --data "$orig" --user "$work/user" "$@" --replay "$file" >"$work/replay.out" 2>"$work/err"
}

part_finish() {
    # the whole race to its finish (the autopilot, one lap) and through its three results screens; a clean end
    TNFS_AUTOPILOT=1 TNFS_LAPS=1 TNFS_BANDS=0 TNFS_VERBOSE=1 TNFS_TRACE_PLAYER=30 TNFS_INPUT_SETTLE=300 \
        TNFS_INPUT_SCRIPT="0:WAIT:mark:spray_v4=1:120$(through segment track best_totals)" \
        timeout -k 5 300 "$tnfs" --data "$orig" --mods "$piece" --user "$work/user" --drive dune-run --car 5 --type 0 >"$work/race.out" 2>"$work/err"
    rc=$?
    [ "$rc" -eq 0 ] || fail "finish: the race ended with status $rc: $(tail -3 "$work/err")"
    log | grep -q "^finish: 0=" || fail "finish: the race did not finish"
    # its record by id, in the file of added tracks; the original's file has none of it
    [ -s "$work/user/race_records_tracks.dat" ] || fail "finish: race_records_tracks.dat was not made"
    [ -s "$work/user/Replay.rpx" ] || fail "finish: no Replay.rpx"
    find "$work/user" -iname 'replay.rpl' | grep -q . && fail "finish: Replay.rpl was written"
    [ "$(tail -c 45 "$work/user/Replay.rpx" | head -c 8)" = "TNFSTRK1" ] || fail "finish: Replay.rpx does not end with the track's block"
    tail -c 45 "$work/user/Replay.rpx" | grep -q "dune-run" || fail "finish: the track's block does not name dune-run"
    cp "$work/user/Replay.rpx" "$work/kept.rpx"
    cp "$work/race.out" "$work/race.kept"
}

part_replay() {
    # its replay with the piece plays the race it recorded (the trace of every tick both logged is the same), then Esc and Enter end it
    [ -s "$work/kept.rpx" ] || { fail "replay: no replay from finish"; return; }
    replay "$work/kept.rpx" "$piece"
    rc=$?
    [ "$rc" -eq 0 ] || fail "replay: with the piece it ended with status $rc: $(tail -3 "$work/err")"
    python3 "$repo/tools/replay_sync.py" race "$work/race.kept" "$work/replay.out" >"$work/sync" 2>&1 || fail "replay: not in sync: $(cat "$work/sync")"
}

part_refusals() {
    [ -s "$work/kept.rpx" ] || { fail "refusals: no replay from finish"; return; }
    # without the piece: it names it
    replay "$work/kept.rpx" ""
    rc=$?
    { [ "$rc" -ne 0 ] && [ "$rc" -lt 128 ]; } || fail "refusals: without the piece: status $rc, not a clean failure"
    grep -q "replay: needs the mod dune-run" "$work/err" || fail "refusals: without the piece it did not name dune-run: $(tail -2 "$work/err")"
    # with the piece changed (another verge in its card), it does not play,
    # and the text names the track's rules, not the cars'
    changed=$work/changed
    cp -R "$piece" "$changed"
    chmod -R u+w "$changed"
    sed 's/^edge=grass$/edge=sand/' "$piece/mod.ini" > "$changed/mod.ini"   # a valid change the fingerprint takes: the verge
    replay "$work/kept.rpx" "$changed"
    rc=$?
    { [ "$rc" -ne 0 ] && [ "$rc" -lt 128 ]; } || fail "refusals: with the piece changed: status $rc, not a clean failure ($(tail -2 "$work/err"))"
    grep -q "replay: the mod dune-run has changed since this replay was made, or the game's rules for added tracks have" "$work/err" ||
        fail "refusals: with the piece changed the text is not the track's: $(tail -2 "$work/err")"
}

part_stock() {
    # Stock and Restored do not race it: no record file made by them, the replay file as it was
    for profile in stock restored; do
        echo "profile=$profile" > "$work/user/port.cfg"
        touch "$work/stock.marker"
        sleep 1
        timeout -k 5 120 "$tnfs" --data "$orig" --mods "$piece" --user "$work/user" --drive dune-run --car 5 --node 200 --screenshot "$work/s.ppm" >/dev/null 2>"$work/err" &&
            fail "stock: $profile raced an added track"
        cmp -s "$work/kept.rpx" "$work/user/Replay.rpx" || fail "stock: $profile changed Replay.rpx"
        [ -n "$(find "$work/user" -name 'race_records_tracks.dat' -newer "$work/stock.marker")" ] && fail "stock: $profile wrote race_records_tracks.dat"
    done
    echo "profile=restomod" > "$work/user/port.cfg"
}

for part in $parts; do
    case $part in
    finish|replay|refusals|stock) "part_$part" ;;
    *) echo "FAIL: no part $part" >&2; exit 2 ;;
    esac
done
[ "$failures" -eq 0 ] && echo "track_piece ($parts): all checks passed"
exit "$failures"
