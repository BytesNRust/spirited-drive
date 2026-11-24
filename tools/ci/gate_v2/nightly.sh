#!/bin/zsh
# nightly.sh [-n]: the full sweep of main, every night at 05:00 (launchd: com.bytesnrust.gate-nightly.plist).
#   Release + the whole ASan + golden + frame_compare + window + menu_compare: gate_main_v2.sh with the scope forced to
#   full, under the same lock as the gates (kind=nightly). While a gate holds the lock the sweep waits (NIGHTLY_WAIT,
#   default 3 h) and then gives up. A gate that starts during the sweep waits for it.
#   It works in its own checkout (NIGHTLY_REPO, default dev/sd-nightly: a worktree with build/release, build/asan56 and
#   the original link), detached at origin/main, so sd-gate and its branch are never touched. Its scratch files are in
#   work/nightly/. menu_compare's reference is the binary of the last night that passed.
#   A night whose origin/main is the commit of the last night that passed is not run again.
# Output: work/nightly-YYYYMMDD.log, ending in ONE line to grep:
#     NIGHTLY_OK <date> <commit> <minutes> min     |     NIGHTLY_FAIL <date> <commit>: <what failed>
#   See the last nights:  grep -h '^NIGHTLY_' work/nightly-*.log | tail -5
#   -n / --dry-run: prints what it would do (to the terminal, no log file, no fetch, no checkout, no lock).
DRY=0; ARGS=(); [[ $1 == -n || $1 == --dry-run ]] && { DRY=1; ARGS=(-n); }
HERE=${0:A:h}
W=${NIGHTLY_WORK:-${WORKSPACE_ROOT:-$HOME/workspace}/bytes-n-rust/notes/tnfs-35-tools/work}
REPO=${NIGHTLY_REPO:-${WORKSPACE_ROOT:-$HOME/workspace}/bytes-n-rust/dev/sd-nightly}
NW=$W/nightly
DAY=$(date +%Y-%m-%d)
export PATH=/nix/var/nix/profiles/default/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin   # launchd gives almost none
mkdir -p $NW
(( DRY )) || exec >> $W/nightly-$(date +%Y%m%d).log 2>&1
T0=$SECONDS
commit="?"
fail() { echo "NIGHTLY_FAIL $DAY $commit: $1"; exit 1; }

echo "nightly $DAY $(date +%H:%M) repo $REPO$( (( DRY )) && echo '  DRY RUN')"
cd $REPO 2>/dev/null || fail "no checkout at $REPO"
if (( DRY )); then
    echo "+ git fetch -q origin main && git checkout -q --detach origin/main"
    [[ -n $(git status --porcelain --untracked-files=no) ]] && echo "(this checkout has local changes: the real run would stop here)"
else
    [[ -z $(git status --porcelain --untracked-files=no) ]] || fail "the checkout has local changes"
    git fetch -q origin main || fail "git fetch failed"
    git checkout -q --detach origin/main || fail "git checkout origin/main failed"
fi
commit=$(git rev-parse --short origin/main)
if [[ -f $NW/last_ok && $(<$NW/last_ok) == $commit ]]; then
    echo "NIGHTLY_OK $DAY $commit 0 min (main is the commit of the last night that passed: not run again)"
    exit 0
fi

GATE_REPO=$REPO GATE_WORK=$NW GATE_MODS=$W/mods GATE_PREV=$NW/prev_tnfs_night GATE_SCOPE=full \
GATE_ASAN_REDUCED="" GATE_CORE_E2E=gate GATE_LOCK_KIND=nightly GATE_LOCK_WAIT=${NIGHTLY_WAIT:-10800} \
    zsh $HERE/gate_main_v2.sh $ARGS 2>&1 | tee $NW/run.out
rc=${pipestatus[1]}
(( DRY )) && { echo "+ NIGHTLY_OK $DAY $commit <minutes> min | NIGHTLY_FAIL $DAY $commit: <what failed>"; exit 0; }

verdict=$(grep '^verdict' $NW/run.out | tail -1)
if (( rc == 0 )) && [[ $verdict == "verdict OK"* ]]; then
    cp -p build/release/tnfs $NW/prev_tnfs_night
    print -r -- $commit > $NW/last_ok
    echo "NIGHTLY_OK $DAY $commit $(( (SECONDS - T0) / 60 )) min"
else
    why=${verdict#verdict FAIL: }
    [[ -z $verdict ]] && why=$(grep -E '^lock: held' $NW/run.out | tail -1)
    fail "${why:-the run stopped before its verdict (a build error? see the lines above)}"
fi
