#!/bin/bash
# Two races in one process (the menus' way: a track, then another): the second must register its own masks for the reflectors ("reflectors: N" with the second track's count), not keep the first's.
# The masks are made here (all-red PNGs for a few textures of each track): nothing of the game's art is in the repo.
# usage: two_tracks_reflectors.sh TNFS REPO WORK [DATA]
set -u
TNFS=$1 REPO=$2 WORK=$3 DATA=${4:-$2/original}
[ -d "$DATA/SIMDATA" ] || { echo "skip (no $DATA/SIMDATA)"; exit 0; }
mkdir -p "$WORK"
WORK=$(mktemp -d "$WORK/run.XXXXXX")
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
printf 'profile=restomod\npsx_light=1\npsx_headlights=1\n' > "$WORK/user/port.cfg"
env TNFS_REFLECTORS=1 TNFS_TRACK_MASKS="$WORK/masks" TNFS_PSX_TIME=night TNFS_WEATHER=clear TNFS_DRIVE_THEN=CY2 TNFS_FRAMES=20 \
    SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
    timeout -k 5 200 "$TNFS" --data "$DATA" --user "$WORK/user" --mods "$WORK/mods" --drive CY1 --type 0 --car 2 --node 20 --screenshot "$WORK/shot.ppm" > "$WORK/run.out" 2>&1
LOG="$WORK/user/tnfs.log"
lines=$(grep -a "^reflectors: " "$LOG")
echo "$lines"
[ "$(echo "$lines" | grep -c .)" -ge 2 ] || { echo "FAIL: the second track did not register its masks (one 'reflectors:' line)"; exit 1; }
first=$(echo "$lines" | sed -n 1p) second=$(echo "$lines" | sed -n 2p)
[ "$first" = "reflectors: 3 textures with a mask" ] || { echo "FAIL: CY1: '$first'"; exit 1; }
[ "$second" = "reflectors: 1 textures with a mask" ] || { echo "FAIL: CY2: '$second'"; exit 1; }
echo PASS
