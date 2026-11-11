#!/bin/zsh
# light_time_layout_check.sh BUILD: turning on the baked light's time of day must not move the HUD. With the time
# at A and no LGS/LGT file to read (neutral tint), the frames at 4:3 must equal the frames at day, byte for byte: the same
# 640 layout, only the wide path's composition differs. TR1 views 1 and 0, AL1 view 0 (the in-car dash and mirror).
set -u
export PATH=/nix/var/nix/profiles/default/bin:$PATH
bin=${1:?usage: light_time_layout_check.sh BUILD}/tnfs
data=${TNFS_DATA:-$PWD/original}
work=$(mktemp -d)
mkdir -p $work/mods/user $work/cd
bad=0
for case in "TR1 1 400" "TR1 0 400" "AL1 0 1200"; do
    set -- ${=case}
    for time in day a; do
        printf "psx_dir=$work/cd\n" > $work/mods/user/port.cfg
        env SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_LIGHT_EFFECTS=1 TNFS_LIGHT_TIME=$time TNFS_VIEW=$2 timeout -k 5 120 ${bin:a} --data $data --user $work/mods/user --drive $1 --entry $3 --screenshot $work/$1_$2_$time.ppm >/dev/null 2>&1
    done
    if cmp -s $work/$1_$2_day.ppm $work/$1_$2_a.ppm; then echo "same  $1 view $2"; else echo "DIFF  $1 view $2"; bad=1; fi
done
rm -r $work
exit $bad
