#!/bin/zsh
# tools/menu_compare.sh <binA> <binB> <outdir>: walks every menu target (tools/menu_walk.sh) with both binaries and compares the pictures,
# the menus' counterpart of frame_compare.sh for the races. A menu art change that was not meant (an edit of the original art, a layout shift, the
# old "ULFTRA" kind) shows as a target that differs.
#
# For each target one line:   <target>: same   |   <target>: differs <N> px (diff: <outdir>/diff/<target>.png)   |   <target>: walk failed (A|B|A+B), see <outdir>/<A|B>/<target>.log
# then a summary line. Exit 0 every target the same, 1 some differ, 3 some walk failed (a failure outranks a difference), 2 bad arguments.
#   TARGETS (default: every menu_walk.sh target)   JOBS (default 2: walks running at once; each is one headless game)
#   DATA, STEP_TIMEOUT, SETTLE: as menu_walk.sh
# The pictures are the walk's last frame (the pointer included), so a run of the same binary twice is "same". The eight targets take about 6 minutes
# on the 4-thread dev machine with JOBS=2 (16 walks of 40 s).
set -u
export PATH=/nix/var/nix/profiles/default/bin:$PATH

bin_a=${1:-} bin_b=${2:-} out=${3:-}
if [[ -z $bin_a || -z $bin_b || -z $out ]]; then print -u2 -- "usage: $0 <binA> <binB> <outdir>"; exit 2; fi
for b in $bin_a $bin_b; do [[ -x $b ]] || { print -u2 -- "menu_compare.sh: no binary: $b"; exit 2; }; done
here=${0:A:h}
bin_a=${bin_a:a} bin_b=${bin_b:a}
targets=(${=TARGETS:-options controls controls-p2 controls-p3 graphics extras extras-p2 extras-p3})
jobs_max=${JOBS:-2}
mkdir -p $out/A $out/B $out/diff || exit 2
out=${out:a}

# one walk, its exit status in <outdir>/<A|B>/<target>.rc and its stderr in .log
walk() {   # which bin target
    "$here/menu_walk.sh" "$2" "$out/$1" "$3" > "$out/$1/$3.log" 2>&1
    print -r -- $? > "$out/$1/$3.rc"
}
typeset -a pids
running() {   # how many launched walks are still going
    local live=() p
    for p in $pids; do kill -0 $p 2>/dev/null && live+=($p); done
    pids=($live)
    print -r -- ${#pids}
}
for t in $targets; do
    for which in A B; do
        bin=$bin_a; [[ $which == B ]] && bin=$bin_b
        while (( $(running) >= jobs_max )); do sleep 1; done
        walk $which $bin $t &
        pids+=($!)
    done
done
wait

same=0 differ=0 failed=0
for t in $targets; do
    bad=""
    [[ $(cat $out/A/$t.rc 2>/dev/null) == 0 && -f $out/A/$t.png ]] || bad+="A"
    [[ $(cat $out/B/$t.rc 2>/dev/null) == 0 && -f $out/B/$t.png ]] || bad+="${bad:+"+"}B"
    if [[ -n $bad ]]; then
        print -r -- "$t: walk failed ($bad), see $out/<A|B>/$t.log"; failed=$((failed + 1)); continue
    fi
    n=$(magick compare -metric AE $out/A/$t.png $out/B/$t.png $out/diff/$t.png 2>&1 >/dev/null)
    if [[ $n == 0 || $n == "0 "* ]]; then
        print -r -- "$t: same"; same=$((same + 1)); rm -f $out/diff/$t.png
    else
        [[ ${n%% *} == <-> ]] || n="? (the pictures' sizes differ)"
        print -r -- "$t: differs ${n%% *} px (diff: $out/diff/$t.png)"; differ=$((differ + 1))
    fi
done
print -r -- "menu_compare: $same same, $differ differ, $failed failed, of ${#targets}"
(( failed > 0 )) && exit 3
(( differ > 0 )) && exit 1
exit 0
