#!/bin/zsh
# bench_matrix.sh BIN [REPO]: prints a TSV on stdout; CPU and wall ms per drawn frame over tracks x views x looks x TNFS_BANDS x TNFS_RENDER_SCALE.
# Each cell: the same 1000 steps with TNFS_FRAMES=0 and =600; (t600 - t0) / 600; min of 3 for each of the two runs.
# Env filters: TRACKS="TR1 CY1" CASES="0:21:9 1:4:3" LOOKS="exe modern" BANDS="0 2 4" SCALES="1 2 3" ROUNDS=3
emulate -L zsh
set -u
export PATH=/nix/var/nix/profiles/default/bin:$PATH
bin=${1:?usage: bench_matrix.sh BIN [REPO]}; bin=${bin:A}; repo=${2:-${0:A:h:h}}   # REPO: where original/ is
S=$(mktemp -d -t bench_matrix.XXXXXX)
TRACKS=(${=TRACKS:-TR1 CY1}); CASES=(${=CASES:-0:21:9 1:4:3}); LOOKS=(${=LOOKS:-exe modern})
BANDS=(${=BANDS:-0 2 auto}); SCALES=(${=SCALES:-1 2 3}); ROUNDS=${ROUNDS:-3}
TIMEFMT='%U %S %E'
mods() {   # mods FORMAT MODERN
  local d=$S/mods_matrix; [[ -d $d ]] && find $d -delete; mkdir -p $d/user
  print 'profile=restomod' > $d/user/port.cfg   # the real mode, with only what this case changes
  printf 'format=%s\nscale=integer\nrefresh_hz=120\nrender_scale=1\n' $1 > $d/user/restomod.cfg
  if (( ! $2 )); then printf 'perspective=0\ntexture_filter=0\nfog=0\n' >> $d/user/restomod.cfg; fi
  print -r -- $d
}
one() {   # one MODS TRACK VIEW FRAMES BANDS SCALE -> "cpu_s wall_s"
  local t
  t=$( { time ( cd $repo && SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_VIEW=$3 TNFS_FRAMES=$4 TNFS_BANDS=$5 TNFS_RENDER_SCALE=$6 \
        timeout -k 5 600 $bin --data original --mods $1 --drive $2 --entry 1000 --screenshot $S/matrix_shot.ppm >/dev/null 2>&1 ) } 2>&1 )
  print -r -- $t | awk '{printf "%.4f %.4f\n", $1+$2, $3}'
}
printf "track\tview\tfmt\tlook\tbands\tscale\tcpu_ms\twall_ms\tload\n"
for tr in $TRACKS; do for c in $CASES; do view=${c%%:*}; fmt=${c#*:}; for look in $LOOKS; do
  m=$(mods $fmt $([[ $look == modern ]] && echo 1 || echo 0))
  for b in $BANDS; do for sc in $SCALES; do
    c0=999 w0=999 c6=999 w6=999
    for r in $(seq $ROUNDS); do
      a=($(one $m $tr $view 0 $b $sc)); (( a[1] < c0 )) && c0=$a[1]; (( a[2] < w0 )) && w0=$a[2]
      a=($(one $m $tr $view 600 $b $sc)); (( a[1] < c6 )) && c6=$a[1]; (( a[2] < w6 )) && w6=$a[2]
    done
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%.2f\t%.2f\t%s\n' $tr $view $fmt $look $b $sc $(( (c6-c0)*1000/600 )) $(( (w6-w0)*1000/600 )) "$(uptime | sed 's/.*averages: //' | awk '{print $1}')"
  done; done
done; done; done
find $S -delete
