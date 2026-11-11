#!/bin/bash
# Two races in one process (the menus' way: a track, then another): the second must register its own masks for the reflectors ("reflectors: N" with the second track's count), not keep the first's.
# The masks are made here (all-red PNGs for a few textures of each track): nothing of the game's art is in the repo.
# usage: two_tracks_reflectors.sh TNFS REPO WORK [DATA]
set -u
TNFS=$1 REPO=$2 WORK=$3 DATA=${4:-$2/original}
[ -d "$DATA/SIMDATA" ] || { echo "skip (no $DATA/SIMDATA)"; exit 0; }
mkdir -p "$WORK"
WORK=$(mktemp -d "$WORK/run.XXXXXX")
trap 'rc=$?; if [ "$rc" -ne 0 ] && [ "${TNFS_KEEP_RUN:-0}" = 1 ]; then echo "kept: $WORK" >&2; else find "$WORK" -delete; fi' EXIT   # its run dir goes with it; kept only on a failure with TNFS_KEEP_RUN=1
mkdir -p "$WORK/mods" "$WORK/user" "$WORK/masks/CY1" "$WORK/masks/CY2"
python3 - "$WORK/masks" <<'P'
import sys, struct, zlib
def png(path, w=8, h=8):
    raw = b"".join(b"\x00" + b"\xff\x00\x00" * w for _ in range(h))
    def chunk(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))
d = sys.argv[1]
for n in ("scenery_00", "scenery_02", "scenery_04"): png(f"{d}/CY1/{n}.png")   # CY1 has these boards (signs): three masks
png(f"{d}/CY2/scenery_00.png")                                                  # CY2 one
P
# "reflectors:" is a detail line (port_log_detail): the runs are verbose. A mask is looked up file by file (these first, then the port's data/track_masks), so each track's count is what that track gives alone: run each once by itself
race() {   # race TRACK THEN USERDIR: the "reflectors:" lines of a run of TRACK (then THEN, when not empty)
    mkdir -p "$3"
    printf 'profile=restomod\nlight_effects=1\nheadlight_beams=1\n' > "$3/port.cfg"
    env TNFS_VERBOSE=1 TNFS_REFLECTORS=1 TNFS_TRACK_MASKS="$WORK/masks" TNFS_LIGHT_TIME=night TNFS_WEATHER=clear ${2:+TNFS_DRIVE_THEN=$2} TNFS_FRAMES=20 \
        SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
        timeout -k 5 200 "$TNFS" --data "$DATA" --user "$3" --mods "$WORK/mods" --drive "$1" --type 0 --car 2 --node 20 --screenshot "$3/shot.ppm" > "$3/run.out" 2>&1
    grep -a "^reflectors: " "$3/spirited-drive.log"
}
alone1=$(race CY1 "" "$WORK/u1" | sed -n 1p) alone2=$(race CY2 "" "$WORK/u2" | sed -n 1p)
lines=$(race CY1 CY2 "$WORK/user")
echo "$lines"
[ -n "$alone1" ] && [ -n "$alone2" ] || { echo "FAIL: a lone race registered no masks ('$alone1', '$alone2')"; exit 1; }
[ "$alone1" != "$alone2" ] || { echo "FAIL: CY1 and CY2 give the same count ('$alone1'): the test cannot tell them apart"; exit 1; }
[ "$(echo "$lines" | grep -c .)" -ge 2 ] || { echo "FAIL: the second track did not register its masks (one 'reflectors:' line)"; exit 1; }
first=$(echo "$lines" | sed -n 1p) second=$(echo "$lines" | sed -n 2p)
[ "$first" = "$alone1" ] || { echo "FAIL: CY1: '$first', alone '$alone1'"; exit 1; }
[ "$second" = "$alone2" ] || { echo "FAIL: CY2: '$second', alone '$alone2'"; exit 1; }
echo PASS
