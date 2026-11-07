#!/bin/zsh
# esc_ghost.sh [BIN] [HZ]: Options -> Controls setup -> ESC (a scripted tap, latched as a finger's is) must land on
# Options and stay there (a phantom ESC would end on Central). Saves the last frame for a look; no automatic verdict.
# TNFS_EXTRAS_CFG=<extras.cfg> (optional) is copied into the scratch mods. Headless: dummy drivers, scratch --user, the repo's original/ as data. Needs magick for the frame check.
BIN=${1:-build/release/tnfs}; BIN=${BIN:A}; HZ=${2:-60}
DATA=${TNFS_DATA:-original}
M=$(mktemp -d); mkdir -p $M/user $M/cap
[ -n "$TNFS_EXTRAS_CFG" ] && cp "$TNFS_EXTRAS_CFG" $M/user/extras.cfg
printf "format=4:3\nview_ultra=1\npsx_dir=\n" > $M/user/port.cfg
W="12000:MOUSE:600:30,12300:CLICK:600:30,16000:MOUSE:310:145,17500:CLICK:310:145,21000:MOUSE:600:30,22500:CLICK:600:30,30000:CLICK:600:30,44000:MOUSE:215:450,46000:CLICK:215:450,56000:MOUSE:80:285,58000:CLICK:80:285,70000:ESC"
(cd $M && TNFS_REFRESH_HZ=$HZ SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_CAPTURE_DIR=$M/cap TNFS_CAPTURE_EVERY=1 \
  TNFS_INPUT_SCRIPT="$W" timeout -k 3 95 $BIN --data $DATA --user $M/user > $M/log 2>&1)
last=$(ls $M/cap/*.ppm 2>/dev/null | sort | tail -1)
[ -n "$last" ] || { echo "no frames captured (check --data and the binary path)"; cat $M/log | tail -5; exit 1; }
magick $last ${TMPDIR:-/tmp}/esc_ghost_last.png && echo "last frame: ${TMPDIR:-/tmp}/esc_ghost_last.png (Options expected)"
find $M -type f -delete; find $M -type d -empty -delete
