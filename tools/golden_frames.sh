#!/bin/zsh
# golden_frames.sh BUILD OUT.md5: writes one "md5  <track>_v<view>.ppm" line per frame of the reference
# set, in a fixed order, so the following tasks can diff their own frames against it.
#
# The frame of each run: TNFS_VIEW=v --drive TRACK --entry 400 --screenshot (track_viewer's headless frame
# path: 400 steps at full throttle and then one picture), headless with the SDL dummy drivers and timeout -k,
# in a temp dir of its own that is deleted at the end (also when a frame is missing). rc != 0 and nothing
# written to OUT.md5 if any of the 15 frames is missing.
#
# The captures are deterministic, so the same build gives the same 15 hashes every time.
#
# BUILD: the dir holding tnfs (as `nix develop -c cmake --build build/release` leaves it); the game's data
#        (original/) is taken from $PWD/original, as tnfs's default is.
# OUT.md5: where the lines go; "-" writes them to stdout.
set -u
export PATH=/nix/var/nix/profiles/default/bin:$PATH   # timeout (coreutils), as the other tools/*.sh do

build=${1:?usage: golden_frames.sh BUILD OUT.md5}
out=${2:?usage: golden_frames.sh BUILD OUT.md5}
bin=${build:a}/tnfs
data=${PWD}/original

TRACKS=(TR1 CY1 AL1)
VIEWS=(0 1 2 5 9)
ticks=400
secs=60

if [[ ! -x $bin ]]; then print -u2 -- "golden_frames.sh: no tnfs in $build"; exit 1; fi
if [[ ! -d $data ]]; then print -u2 -- "golden_frames.sh: no data in $data"; exit 1; fi

# `md5 -q` is macOS's; md5sum prints the hash and the name itself, so its output is cut down to the hash.
# `md5 -q` is asked of /dev/null rather than read off a help text: on a box whose md5 is a coreutils md5sum
# under the name, the help lists --quiet but the option is not there.
if [[ -n ${MD5_CMD:-} ]]; then
  md5of() { eval $MD5_CMD; }
elif [[ $(md5 -q /dev/null 2>/dev/null) == [0-9a-f](#c32) ]]; then
  md5of() { md5 -q "$1"; }
else
  md5of() { md5sum "$1" | cut -d' ' -f1; }
fi

tmp=$(mktemp -d -t golden_frames.XXXXXX) || exit 1
trap 'rm -rf $tmp' EXIT INT TERM   # zsh: $tmp is read when the trap fires, so the dir is always the one above
# the player's own files stay out: an empty --user dir holds them, so the frames are Stock '96's, the exe's
mkdir -p $tmp/mods || exit 1
missing=0
lines=()

for track in $TRACKS; do
  for view in $VIEWS; do
    ppm=$tmp/${track}_v${view}.ppm
    log=$tmp/${track}_v${view}.log
    SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_VERBOSE=1 TNFS_VIEW=$view timeout -k 5 $secs \
      $bin --user $tmp/mods --drive $track --entry $ticks --screenshot $ppm >$log 2>&1
    if [[ ! -s $ppm ]]; then
      print -u2 -- "golden_frames.sh: no ${track}_v${view}.ppm (see below)"
      cat $log >&2
      missing=1
      continue
    fi
    lines+=("$(md5of $ppm)  ${track}_v${view}.ppm")
  done
done

if [[ $missing == 0 ]]; then
  if [[ $out == - ]]; then
    print -l -- $lines
  else
    mkdir -p ${out:h} || exit 1
    print -l -- $lines >$out
  fi
  print -u2 -- "golden_frames.sh: ${#lines} frames -> ${out:--}"
else
  print -u2 -- "golden_frames.sh: ${#lines} of $(( ${#TRACKS} * ${#VIEWS} )) frames, nothing written to $out"
fi
exit $missing