#!/bin/zsh
# ultra_sweep.sh BUILD OUTDIR: the "ultra" frames of every track for the review of the view distance (plan task
# 9c): three points of each track, each one twice, with TNFS_VIEW_ULTRA=1 and 0, so the two can be compared.
#
#   TNFS_VIEW_ULTRA=<0|1> TNFS_VIEW=1 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy timeout -k 5 120 \
#       BUILD/tnfs --drive TRACK --entry N [--car C --racers 1 --type 0] --screenshot OUTDIR/<name>.ppm
#
# The frames are converted to PNG beside the .ppm (sips, macOS) when it is there, and the list of paths goes to
# stdout: one line per capture, so the reviewer can open them from the message. Nothing is written to stdout but
# that list. The 4:3 format is forced (TNFS_SCREEN=4:3), so the wide screens of port.cfg do not change the frames.
#
# The entries are where the car stands at the start (100), near the middle, and near the end; on a stage (CY, CL,
# AL) the end is the last ~200 nodes, on a circuit (TR) the entry just before the race goes over, since the ring
# is lapped and has no "last 200 nodes". They were measured per track (see the STATE note of 2026-10-20), not
# guessed: the car is left at full throttle as --drive's own frame path does, so on a few tracks it leaves the
# road or stops before the end. Where that happens the table carries another car (and, on two stages, a single
# car alone), the one that gets furthest there; AL3 is the exception, the best it reaches is its node 625 of 2168,
# so its middle and end are the same frame. These are the frames to review, not a perfect lap of every track.
#
# BUILD: the dir holding tnfs; OUTDIR: where the captures go (outside the repo). rc != 0 if any frame is missing.
set -u
export PATH=/nix/var/nix/profiles/default/bin:$PATH   # timeout (coreutils), as the other tools/*.sh do

build=${1:?usage: ultra_sweep.sh BUILD OUTDIR}
out=${2:?usage: ultra_sweep.sh BUILD OUTDIR}
bin=${build:a}/tnfs
data=${PWD}/original
view=1
secs=120

if [[ ! -x $bin ]]; then print -u2 -- "ultra_sweep.sh: no tnfs in $build"; exit 1; fi
if [[ ! -d $data ]]; then print -u2 -- "ultra_sweep.sh: no data in $data"; exit 1; fi
mkdir -p $out || exit 1

# track: start mid end and the car that gets furthest there ("-" is the default: --drive's own car and field)
POINTS=(
  'CY1 100 2905 5918 -'
  'CY2 100 4234 6881 4:solo'
  'CY3 100 6984 11718 4'
  'CL1 100 4178 9808 -'
  'CL2 100 6332 10988 -'
  'CL3 100 7543 13594 0:solo'
  'AL1 100 5130 10014 -'
  'AL2 100 7965 16174 3:solo'
  'AL3 100 25600 25600 1'
  'TR1 100 3000 5665 -'
  'TR2 100 28000 60000 1:solo'
  'TR3 100 6800 13010 -'
  'TR4 100 5200 9812 -'                 # no TR4_T01.FAM in the owner's data: its frames come out black
  'TR6 100 6000 11525 -'
  'TR7 100 3000 9164 5'
)
POINTS_AT=(start mid end)

have_sips=$(command -v sips)
missing=0
done=0

for row in $POINTS; do
  track=${row%% *}; rest=${row#* }
  car=${rest##* }                          # the last field is the car, "-" the default
  entries=(${=${rest% *}})                 # the three before it: start, middle, end
  car_args=()
  if [[ $car != - ]]; then
    car_args=(--car ${car%%:*})
    [[ ${car#*:} == solo ]] && car_args+=(--racers 1 --type 0)
  fi
  for (( p = 1; p <= 3; p++ )); do
    entry=${entries[$p]}
    point=${POINTS_AT[$p]}
    for ultra in 1 0; do
      name=${track}_${point}_u${ultra}
      ppm=$out/$name.ppm
      TNFS_VIEW_ULTRA=$ultra TNFS_VIEW=$view TNFS_SCREEN=4:3 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
        timeout -k 5 $secs $bin --drive $track --entry $entry $car_args --screenshot $ppm \
        >$out/$name.log 2>&1
      if [[ ! -s $ppm ]]; then
        print -u2 -- "ultra_sweep.sh: no $name.ppm (see $out/$name.log)"
        missing=1
        continue
      fi
      [[ -n $have_sips ]] && sips -s format png $ppm --out $out/$name.png >/dev/null 2>&1
      if [[ -s $out/$name.png ]]; then print -r -- "$out/$name.png"; else print -r -- "$ppm"; fi
      (( ++done ))
    done
  done
done

print -u2 -- "ultra_sweep.sh: $done captures in $out"
exit $missing