#!/bin/zsh
# frame_compare.sh BUILD "ENV_A" "ENV_B" [OUT_DIR]: renders the same set of frames twice, once with the environment
# words of ENV_A and once with ENV_B (each a quoted string of VAR=value words, "" for none), and compares the pictures
# byte for byte (cmp on the PPMs, not a hash of them). Exit 1 and a DIFF line per picture that differs.
#
# For the render bands (re/notes/render-bands-design.md): frame_compare.sh build/release "TNFS_BANDS=0" "TNFS_BANDS=4".
#
# The set is the golden frames' way of drawing (track_viewer's --screenshot after 400 steps at full throttle,
# headless, SDL dummy drivers, timeout -k, an empty --mods dir of its own per run) over
#   tracks   TR1 CY1 AL1            (TRACKS=... overrides)
#   views    0 (in the car) 1 (chase) 2 5 9      (VIEWS=...)
#   formats  4:3 16:9 21:9          (FORMATS=...; TNFS_SCREEN)
#   ticks    400 (TICKS=...: the race tick the picture is taken at)
#   looks    exe, modern (Stock with TNFS_MODERN_3D=1 TNFS_TEXTURE_FILTER=1 TNFS_FOG=1)   (LOOKS=...)
# 90 frames each way with the defaults; set the lists shorter for a quick check.
#
# DATA: the game's data, $TNFS_DATA or $PWD/original. OUT_DIR keeps the pictures when the check finds a difference or a
# missing one (the default is a temp dir deleted at the end); a check that passes deletes its PPMs and keeps the logs
# (90 pairs are about 400 MB, and nobody looks at pictures that are identical).
set -u
export PATH=/nix/var/nix/profiles/default/bin:$PATH

build=${1:?usage: frame_compare.sh BUILD "ENV_A" "ENV_B" [OUT_DIR]}
env_a=${2-}
env_b=${3-}
keep=${4-}
bin=${build:a}/tnfs
data=${TNFS_DATA:-$PWD/original}

TRACKS=(${=TRACKS:-TR1 CY1 AL1})
VIEWS=(${=VIEWS:-0 1 2 5 9})
FORMATS=(${=FORMATS:-4:3 16:9 21:9})
LOOKS=(${=LOOKS:-exe modern})
ticks=${TICKS:-400}
secs=60

if [[ ! -x $bin ]]; then print -u2 -- "frame_compare.sh: no tnfs in $build"; exit 1; fi
if [[ ! -d $data ]]; then print -u2 -- "frame_compare.sh: no data in $data"; exit 1; fi

if [[ -n $keep ]]; then tmp=${keep:a}; mkdir -p $tmp || exit 1
else tmp=$(mktemp -d -t frame_compare.XXXXXX) || exit 1; trap 'rm -rf $tmp' EXIT INT TERM; fi
# the player's folder is this run's own: a tnfs with no --user would write its log, settings and replay in the real one
export TNFS_PREF_DIR=$tmp/pref

# one picture: render TRACK VIEW FORMAT LOOK "ENV words" OUT.ppm
render() {
  local track=$1 view=$2 format=$3 look=$4 envs=$5 ppm=$6
  local mods=$tmp/mods_${ppm:t:r}
  mkdir -p $mods/user || return 1
  [[ $look == modern ]] && envs="TNFS_MODERN_3D=1 TNFS_TEXTURE_FILTER=1 TNFS_FOG=1 $envs"
  env SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_VIEW=$view TNFS_SCREEN=$format ${=envs} \
    timeout -k 5 $secs $bin --data $data --mods $mods --drive $track --entry $ticks --screenshot $ppm \
    >$ppm.log 2>&1
  [[ -s $ppm ]]
}

different=0 total=0 missing=0
for track in $TRACKS; do for view in $VIEWS; do for format in $FORMATS; do for look in $LOOKS; do
  name=${track}_v${view}_${format/:/x}_${look}
  total=$(( total + 1 ))
  if ! render $track $view $format $look "$env_a" $tmp/A_$name.ppm || ! render $track $view $format $look "$env_b" $tmp/B_$name.ppm; then
    print -u2 -- "MISSING $name (see $tmp/A_$name.ppm.log, B_$name.ppm.log)"
    missing=$(( missing + 1 ))
    continue
  fi
  if cmp -s $tmp/A_$name.ppm $tmp/B_$name.ppm; then print -- "same  $name"
  else print -- "DIFF  $name"; different=$(( different + 1 )); fi
done; done; done; done

print -u2 -- "frame_compare.sh: $total pictures, $different differ, $missing missing (A: ${env_a:-none}, B: ${env_b:-none})"
if [[ $different == 0 && $missing == 0 ]]; then
  if [[ -n $keep ]]; then for ppm in $tmp/*.ppm(N); do rm -f -- $ppm; done; fi   # passed: the identical pictures are not kept (a failing check keeps them)
  exit 0
fi
exit 1
