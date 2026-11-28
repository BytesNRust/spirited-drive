#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# The picture checks of a gate: the golden frames against tests/golden/frames.md5, the render bands' frames against a run without bands, a window run with bands, and every menu
# target walked with the reference binary (the previous main's) and with this build's, the pictures compared.
# The CI log is public and the pictures, the game's logs and the tools' messages carry what they read from the CD: this script prints its own verdict lines only (a check or a
# target's name, same / DIFF / failed, counts). Everything else goes to files in WORKDIR, which the job deletes. To see why one failed, run it on a machine that has the data.
# usage: gate_pictures.sh REF_TNFS BUILD_DIR WORKDIR        (BUILD_DIR holds this build's tnfs and tnfs-studio)
#   EXPECTED  menu targets whose difference is meant (words of a-z, 0-9, -)
#   GATE_TOOLS, GATE_SH, GATE_DATA  the folder of the checks, the shell that runs them and the CD data, for tests/gate_pictures.sh
set -u
ref=${1:?usage: gate_pictures.sh REF_TNFS BUILD_DIR WORKDIR}
build=${2:?usage: gate_pictures.sh REF_TNFS BUILD_DIR WORKDIR}
work=${3:?usage: gate_pictures.sh REF_TNFS BUILD_DIR WORKDIR}
abs() { case $1 in /*) echo "$1" ;; *) echo "$PWD/$1" ;; esac; }
ref=$(abs "$ref") build=$(abs "$build") work=$(abs "$work")
repo=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd) || exit 2
tools=${GATE_TOOLS:-$repo/tools}
sh=${GATE_SH:-zsh}
data=${GATE_DATA:-$repo/original}
expected=${EXPECTED:-}
case $expected in *[!a-z0-9\ -]*) echo "gate_pictures: EXPECTED holds more than target names"; exit 2 ;; esac
[ -x "$ref" ] || { echo "gate_pictures: no reference binary"; exit 2; }
[ -x "$build/tnfs" ] || { echo "gate_pictures: no tnfs in the build"; exit 2; }
[ -d "$data" ] || { echo "gate_pictures: no CD data"; exit 2; }
cd "$repo" || exit 2
mkdir -p "$work/window/mods" "$work/window/user" || exit 2
failures=0
bad() { echo "$*"; failures=$((failures + 1)); }

# the golden frames
"$sh" "$tools/golden_frames.sh" "$build" "$work/golden.md5" > "$work/golden.log" 2>&1
same=0 diff=0 missing=0
while read -r sum name; do
    got=$(awk -v n="$name" '$2 == n { print $1 }' "$work/golden.md5" 2>/dev/null)
    if [ -z "$got" ]; then missing=$((missing + 1))
    elif [ "$got" = "$sum" ]; then same=$((same + 1))
    else diff=$((diff + 1)); echo "golden DIFF ${name%.ppm}"
    fi
done < "$repo/tests/golden/frames.md5"
echo "golden: $same same, $diff DIFF, $missing missing"
[ "$same" -gt 0 ] && [ "$diff" -eq 0 ] && [ "$missing" -eq 0 ] || failures=$((failures + 1))

# the render bands: the same pictures with four as with none
TNFS_DATA=$data "$sh" "$tools/frame_compare.sh" "$build" "TNFS_BANDS=0" "TNFS_BANDS=4" > "$work/frames.log" 2> "$work/frames.err"
status=$?
sed -n 's/^DIFF  \([A-Za-z0-9_]*\)$/frame DIFF \1/p' "$work/frames.log"
same=$(grep -c '^same  ' "$work/frames.log")
diff=$(grep -c '^DIFF  ' "$work/frames.log")
echo "frame_compare: $same same, $diff DIFF of 90"
[ "$status" -eq 0 ] && [ "$same" -eq 90 ] || failures=$((failures + 1))

# a window run with bands: still running when its time is up (timeout's 124), neither crashed nor gone
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_BANDS=auto timeout -k 5 "${GATE_WINDOW_SECONDS:-15}" \
    "$build/tnfs" --data "$data" --mods "$work/window/mods" --user "$work/window/user" --drive TR1 > "$work/window.log" 2>&1
status=$?
if [ "$status" -eq 124 ]; then echo "window_bands: ok"; else bad "window_bands: FAILED (status $status)"; fi

# the menus, the previous main's binary against this one
DATA=$data JOBS=${JOBS:-4} EXPECTED=$expected timeout -k 10 1500 "$sh" "$tools/menu_compare.sh" "$ref" "$build/tnfs" "$work/menus" > "$work/menus.log" 2> "$work/menus.err"
status=$?
sed -n \
    -e 's/^\([a-z0-9-]*\): same$/menu \1: same/p' \
    -e 's/^\([a-z0-9-]*\): differs \([0-9]*\) px (expected).*/menu \1: differs \2 px (expected)/p' \
    -e 's/^\([a-z0-9-]*\): differs \([0-9]*\) px (diff:.*/menu \1: differs \2 px/p' \
    -e 's/^\([a-z0-9-]*\): differs, no pixel count.*/menu \1: differs, no pixel count/p' \
    -e 's/^\([a-z0-9-]*\): walk failed (\([AB+]*\)).*/menu \1: walk failed (\2)/p' \
    -e 's/^\([a-z0-9-]*\): new in B .*/menu \1: new in B/p' \
    "$work/menus.log"
# a target left out for want of its car piece is coverage lost, not a pass
if grep -q '^added-setup: left out' "$work/menus.err"; then bad "menu added-setup: left out"; fi
summary=$(grep -xE 'menu_compare: [0-9]+ same, [0-9]+ differ, [0-9]+ failed(, [0-9]+ expected)?(, [0-9]+ new in B)?, of [0-9]+' "$work/menus.log")
if [ -n "$summary" ]; then echo "$summary"; fi
if [ "$status" -ne 0 ]; then bad "menu_compare: FAILED (status $status)"
elif [ -z "$summary" ]; then bad "menu_compare: FAILED (status 0, no summary)"
fi

if [ "$failures" -eq 0 ]; then echo "gate_pictures: ok"; exit 0; fi
echo "gate_pictures: FAILED"
exit 1
