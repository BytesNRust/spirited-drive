#!/bin/zsh
# psx_dash_check.sh BUILD: the PS1 light's time of day on an in-car frame (AL1 entry 1200, view 0), with a synthetic tint of 0.5 a
# channel: a dash-art pixel goes to 0.75 of its day colour (half the tint), a tacho needle pixel and a top-bar pixel do not change,
# a mirror-picture pixel goes to 0.5 (the whole tint). Synthetic LGT/LGS files only.
set -u
export PATH=/nix/var/nix/profiles/default/bin:$PATH
here=${0:a:h}
bin=${1:?usage: psx_dash_check.sh BUILD}/tnfs
data=${TNFS_DATA:-$PWD/original}
work=$(mktemp -d)
# the player's folder is this run's own: a tnfs with no --user would write its log, settings and replay in the real one
export TNFS_PREF_DIR=$work/pref
mkdir -p $work/mods/user $work/psx
python3 $here/psx_lgs_synthetic.py $work/psx/ZAL1A.LGS 64 64 64
python3 $here/psx_light_synthetic.py $work/psx/ZAL1A.LGT 2080 full
for time in day a; do
    printf "psx_dir=$work/psx\n" > $work/mods/user/port.cfg
    env SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PSX_LIGHT=1 TNFS_PSX_TIME=$time TNFS_VIEW=0 timeout -k 5 120 ${bin:a} --data $data --mods $work/mods --drive AL1 --entry 1200 --screenshot $work/$time.ppm >/dev/null 2>&1
done
python3 - $work/day.ppm $work/a.ppm <<'P'
import sys
def load(n): return open(n, "rb").read().split(b"\n", 3)[3]
day, a = load(sys.argv[1]), load(sys.argv[2])
def px(b, x, y): return tuple(b[(y * 640 + x) * 3:(y * 640 + x) * 3 + 3])
bad = 0
def check(name, ok, d, t):
    global bad
    print(("ok   " if ok else "FAIL ") + name, d, t)
    bad |= not ok
near = lambda x, y: max(abs(p - q) for p, q in zip(x, y)) <= 1
for name, (x, y), scale in (("top bar", (300, 15), 1.0), ("tacho needle", (330, 350), 1.0), ("dash art", (100, 430), 0.75),
                            ("wheel", (300, 400), 0.75), ("mirror picture", (580, 105), 0.5)):
    d, t = px(day, x, y), px(a, x, y)
    check(name, near(t, tuple(round(c * scale) for c in d)), d, t)
sys.exit(bad)
P
rc=$?
rm -r $work
exit $rc
