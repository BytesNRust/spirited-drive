#!/bin/zsh
# tools/menu_compare.sh <binA> <binB> <outdir>: walks every menu target (tools/menu_walk.sh) with both binaries and compares the pictures,
# the menus' counterpart of frame_compare.sh for the races. A menu art change that was not meant (an edit of the original art, a layout shift, the
# old "ULFTRA" kind) shows as a target that differs.
#
# For each target one line:   <target>: same   |   <target>: differs <N> px (diff: <outdir>/diff/<target>.png)   |   <target>: walk failed (A|B|A+B), see <outdir>/<A|B>/<target>.log
#   |   <target>: new in B (menu_walk.sh's exit 6 from A: binA is from before that screen; B's picture kept, not compared, not a failure; a 6
#       from B is a failure)
#   or, for a target named in EXPECTED (a change meant, e.g. EXPECTED=game-mode for a new layout the owner approves):   <target>: differs <N> px (expected) ...
# then a summary line. Exit 0 every target the same (or its difference expected), 1 some differ, 3 some walk failed (a failure outranks a difference),
# 2 bad arguments or no magick.
#   TARGETS (default: the nine below, content the last; game-mode, central, showcase and the others of menu_walk.sh only when named here, e.g. TARGETS="game-mode central"; added-track needs MODS and EDITION in the environment, menu_walk.sh's: Control Central on an added track's card)   JOBS (default 2: walks running at once; each is one headless game)
#   DATA, STEP_TIMEOUT, SETTLE, FORMAT: as menu_walk.sh (FORMAT=21:9 compares the menus as a 21:9 screen shows them)
# pack_names (cd-setup-lit cd-setup-open added-setup cd-records-512 content-rows, after the nine, always run unless TARGETS names others): pictures of ONE game
# session each (menu_walk.sh's pack target), so the showcase's Diablo page with SETUP lit and its Car Setup modal, the added car's gallery with its modal
# the 512TR's RECORDS and CONTENT with the edition's cars cost two walks (one a binary) and not eight. The added car is a piece made here with tnfs-studio (STUDIO, default the one beside
# binB): without it added-setup is left out and the line says so. A picture a binary cannot make (cd-records-512 on a build from before RECORDS) is
# "new in B", like any screen binA never opened.
# The pictures are the walk's last frame (the pointer included), so a run of the same binary twice is "same". The nine targets take about 7 minutes
# on the 4-thread dev machine with JOBS=2 (18 walks of 40 s; content's walk on a binary from before CONTENT ends at its 60 s step wait).
set -u
export PATH=/nix/var/nix/profiles/default/bin:$PATH

bin_a=${1:-} bin_b=${2:-} out=${3:-}
if [[ -z $bin_a || -z $bin_b || -z $out ]]; then print -u2 -- "usage: $0 <binA> <binB> <outdir>"; exit 2; fi
for b in $bin_a $bin_b; do [[ -x $b ]] || { print -u2 -- "menu_compare.sh: no binary: $b"; exit 2; }; done
# without ImageMagick every walk "failed" with an empty log (the walk could not save its PNG): say so at once instead
command -v magick >/dev/null 2>&1 || { print -u2 -- "menu_compare.sh: magick missing (ImageMagick 7; on nix: nix shell --inputs-from . nixpkgs#imagemagick)"; exit 2; }
here=${0:A:h}
bin_a=${bin_a:a} bin_b=${bin_b:a}
pack_names=(cd-setup-lit cd-setup-open added-setup cd-records-512 content-rows)
targets=(${=TARGETS:-options controls controls-p2 controls-p3 graphics extras extras-p2 extras-p3 content})
[[ -z ${TARGETS:-} ]] && targets=($pack_names $targets)   # the pack's walks first: they are the longest, and start while the others fill the slots
jobs_max=${JOBS:-2}
mkdir -p $out/A $out/B $out/diff || exit 2
out=${out:a}

# the added car of the pack walk: a piece (the Viper's copy) and an edition whose cars end in it, made once; none without tnfs-studio and the CD data
piece_env=""
if (( ${targets[(Ie)added-setup]} )); then
    studio=${STUDIO:-${bin_b:h}/tnfs-studio}
    data=${DATA:-$here/../original}
    if [[ -x $studio && -d $data && -d $here/../data ]] && "$studio" car new --template viper --id test-car --stem TESTCAR --out $out/piece --name "Test Car" \
            --data $data --port-data $here/../data > $out/piece.log 2>&1; then
        chmod -R u+w $out/piece
        printf '%s\n' 'name=Walk' 'cars=rx7,nsx,supra,911,zr1,viper,512tr,diablo,test-car,warrior' 'tracks=city,coastal,alpine,rusty,autumn,burnt,vertigo,transtropolis,vegas' > $out/walk.ini
        piece_env="MODS=$out/piece EDITION=$out/walk.ini"
    else
        print -r -- "added-setup: left out (no car piece: see $out/piece.log; STUDIO=$studio)" >&2
        targets=(${targets:#added-setup})
    fi
fi

# one walk, its exit status in <outdir>/<A|B>/<target>.rc and its stderr in .log
walk() {   # which bin target
    if [[ $3 == pack ]]; then   # one session, several pictures: the walk's status file says how each came out (ok 0, never 6, else 3)
        env ${=piece_env} "$here/menu_walk.sh" "$2" "$out/$1" pack > "$out/$1/pack.log" 2>&1
        print -r -- $? > "$out/$1/pack.rc"
        local n st
        for n in $pack_names; do
            st=$(grep "^$n " "$out/$1/pack.status" 2>/dev/null | cut -d' ' -f2)
            print -r -- ${${${st:-failed}/ok/0}/never/6} | sed 's/failed/3/' > "$out/$1/$n.rc"
            cp "$out/$1/pack.log" "$out/$1/$n.log"
        done
        return
    fi
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
packed=0
for t in $targets; do
    walked=$t
    if (( ${pack_names[(Ie)$t]} )); then (( packed )) && continue; walked=pack packed=1; fi   # the pack's pictures come from one walk
    for which in A B; do
        bin=$bin_a; [[ $which == B ]] && bin=$bin_b
        while (( $(running) >= jobs_max )); do sleep 1; done
        walk $which $bin $walked &
        pids+=($!)
    done
done
wait

expected=(${=EXPECTED:-})
same=0 differ=0 failed=0 meant=0 fresh=0
for t in $targets; do
    if [[ $(cat $out/A/$t.rc 2>/dev/null) == 6 && $(cat $out/B/$t.rc 2>/dev/null) == 0 && -f $out/B/$t.png ]]; then
        print -r -- "$t: new in B (binA never opened it, see $out/A/$t.log); not compared"; fresh=$((fresh + 1)); continue
    fi
    bad=""
    [[ $(cat $out/A/$t.rc 2>/dev/null) == 0 && -f $out/A/$t.png ]] || bad+="A"
    [[ $(cat $out/B/$t.rc 2>/dev/null) == 0 && -f $out/B/$t.png ]] || bad+="${bad:+"+"}B"
    if [[ -n $bad ]]; then
        print -r -- "$t: walk failed ($bad), see $out/<A|B>/$t.log"; failed=$((failed + 1)); continue
    fi
    # AE prints "<pixels> (<share>)"; an HDRI ImageMagick (nixpkgs') prints the pixels as a fraction ("26.085 (8.49e-05)"): rounded. Anything else (the
    # error of pictures of two sizes) has no pixel count
    n=$(magick compare -metric AE $out/A/$t.png $out/B/$t.png $out/diff/$t.png 2>&1 >/dev/null)
    px=${n%% *}
    if [[ $px == <->(|.<->)(|[eE](|[-+])<->) ]]; then
        px=$(printf '%.0f' $px)
        if (( px == 0 )); then
            print -r -- "$t: same"; same=$((same + 1)); rm -f $out/diff/$t.png
        else
            if (( ${expected[(Ie)$t]} )); then print -r -- "$t: differs $px px (expected) (diff: $out/diff/$t.png)"; meant=$((meant + 1))
            else print -r -- "$t: differs $px px (diff: $out/diff/$t.png)"; differ=$((differ + 1)); fi
        fi
    else
        print -r -- "$t: differs, no pixel count (${n:-compare said nothing}); diff: $out/diff/$t.png"; differ=$((differ + 1))
    fi
done
print -r -- "menu_compare: $same same, $differ differ, $failed failed${expected:+, $meant expected}$( (( fresh )) && print -rn -- ", $fresh new in B"), of ${#targets}"
(( failed > 0 )) && exit 3
(( differ > 0 )) && exit 1
exit 0
