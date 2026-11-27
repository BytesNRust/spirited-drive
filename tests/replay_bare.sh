#!/bin/bash
# port (replay bare): a double click on the picture while a replay plays hides the replay's controls and the pointer, and another
# shows them again (game/race/replay_bare.h). The CD's demo replay is played, the log says when the controls go and come back, and
# three pictures are compared on the controls' strip (the bottom 32 of 480 rows): with the controls back it is the strip it was
# (the clock's digits and the pointer aside), bare it is the race's picture.
# TNFS_INPUT_SETTLE=1: the screen never stands still in a replay, so the clicks are not kept waiting for it (the two of a double
# click must come within REPLAY_BARE_DOUBLE_MS).
# usage: replay_bare.sh TNFS REPO WORK [DATA]
set -u
TNFS=$1 REPO=$2 WORK=$3 DATA=${4:-$2/original}
RPL=$DATA/GAMEDATA/REPLAY/REPLAY1.RPL
[ -f "$RPL" ] || { echo "skip (no $RPL)"; exit 0; }
mkdir -p "$WORK"
WORK=$(mktemp -d "$WORK/run.XXXXXX")
trap 'rc=$?; if [ "$rc" -ne 0 ] && [ "${TNFS_KEEP_RUN:-0}" = 1 ]; then echo "kept: $WORK" >&2; else find "$WORK" -delete; fi' EXIT
mkdir -p "$WORK/mods" "$WORK/user" "$WORK/shots"
fail() { echo "FAIL: $1"; exit 1; }

printf 'profile=restomod\n' > "$WORK/user/port.cfg"
SCRIPT="0:WAIT:mark:spray_v4=1:40,500:SHOT:shown"
SCRIPT="$SCRIPT,0:CLICK:320:200,0:CLICK:320:200,0:WAIT:replay: the controls are hidden:10,300:SHOT:bare"
SCRIPT="$SCRIPT,0:CLICK:320:460,0:CLICK:320:460,0:WAIT:replay: the controls are shown:10,300:SHOT:again,500:ESC"
env TNFS_REPLAY_EXIT=1 TNFS_INPUT_SETTLE=1 TNFS_INPUT_SCRIPT="$SCRIPT" TNFS_SHOT_DIR="$WORK/shots" \
    SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy \
    timeout -k 5 80 "$TNFS" --data "$DATA" --user "$WORK/user" --mods "$WORK/mods" --replay "$RPL" > "$WORK/run.out" 2>&1
rc=$?
LOG=$WORK/user/spirited-drive.log
grep -a 'bad TNFS_INPUT_SCRIPT item' "$LOG" "$WORK/run.out" && fail "the script has a bad item"
[ "$rc" -eq 0 ] || fail "the replay did not end by itself (status $rc): $(grep -a 'input script' "$LOG" | tail -2) $(tail -2 "$WORK/run.out")"
[ "$(grep -ac 'replay: the controls are hidden (double click)' "$LOG")" = 1 ] || fail "the controls were not hidden once"
[ "$(grep -ac 'replay: the controls are shown (double click)' "$LOG")" = 1 ] || fail "the controls were not shown again once"

python3 - "$WORK/shots" <<'P' || fail "the pictures' strips"
import sys
def strip(name):
    d = open(f"{sys.argv[1]}/{name}.ppm", "rb").read()
    head = d.split(b"\n", 3)
    assert head[0] == b"P6", head[0]
    w, h = map(int, head[1].split())
    px = head[3]
    rows = h * 32 // 480
    return w, px[(h - rows) * w * 3:]
def same(a, b):
    (w, pa), (_, pb) = a, b
    n = len(pa) // 3
    return sum(pa[i * 3:i * 3 + 3] == pb[i * 3:i * 3 + 3] for i in range(n)) / n
shown, bare, again = strip("shown"), strip("bare"), strip("again")
back, gone = same(shown, again), same(shown, bare)
print(f"strip: shown and shown again {back:.2f} the same, shown and bare {gone:.2f}")
ok = back >= 0.85 and gone <= 0.5
if not ok: print("the strip should be the same with the controls back (>= 0.85) and another picture bare (<= 0.5)")
sys.exit(0 if ok else 1)
P
echo "ok"
