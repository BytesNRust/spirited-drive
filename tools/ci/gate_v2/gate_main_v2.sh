#!/bin/zsh
# gate_main_v2.sh [-n]: the gate of gate_main.sh + gate.sh, with a visible lock, the tests picked from the diff and ccache.
#   usage, as before:  cd sd-gate && GATE_REPO=$PWD zsh .../v2/gate_main_v2.sh > .../work/gate-4h-<x>.log 2>&1
#   -n / --dry-run     prints every heavy command (+ ...) instead of running it; takes no lock; still reads the diff
#                      and the trees' test lists and prints the scope and the test counts
# What it does, in order: lock, load and jobs, ccache on when installed, build release, build asan56 (warnings),
#   scope and test lists (gate_select.py: its header has the rules), ctest release, menu_compare beside ctest ASan,
#   golden, frame_compare, window with bands, a verdict line, ccache -s, the `times` line (same keys as gate.sh).
#   golden, frame_compare, window and menu_compare are skipped when the diff does not touch the game (scope editor/none).
# Exit 0 when the verdict is OK, 1 when something failed or the lock is held (gate.sh always ended 0).
# Environment:
#   GATE_REPO  GATE_WORK      as before (the checkout; the scratch folder)
#   GATE_SCOPE                auto (default) | full | editor | game | none: forces the scope
#   GATE_BASE                 the diff's base (default origin/main; the merge-base with HEAD is used)
#   GATE_LOCK                 the lock file (default .../work/gate.lock, also when GATE_WORK is elsewhere)
#   GATE_LOCK_KIND            gate (default) | nightly      GATE_LOCK_WAIT  seconds to wait for a held lock
#                             (default: 0 against another gate, 7200 against the nightly sweep)
#   GATE_MODS  GATE_PREV      the mods folder of the window run (default $GATE_WORK/mods); menu_compare's reference
#                             binary (default $GATE_WORK/prev_tnfs_main)
#   GATE_ASAN_REDUCED  GATE_CORE_E2E   the two policies below
DRY=0; [[ $1 == -n || $1 == --dry-run ]] && DRY=1

# ---- policies, easy to change (tnfs-35, 2026-11-24; review after two weeks of nightly sweeps) ----
# The scopes whose ASan run is cut to "every unit test + the tests from the diff + the unlabelled ones": a pure editor
# change and a change of only docs or tests. In every other scope (game, core, mixed) ASan runs EVERY test. "" cuts nothing.
export GATE_ASAN_REDUCED=${GATE_ASAN_REDUCED-"editor none"}
# A core (or mixed) change runs everything in the gate: gate. With night, its e2e tests wait for the nightly sweep.
export GATE_CORE_E2E=${GATE_CORE_E2E:-gate}
# ---------------------------------------------------------------------------------------------------

HERE=${0:A:h}
TOOLS=${WORKSPACE_ROOT:-$HOME/workspace}/bytes-n-rust/notes/tnfs-35-tools
cd ${GATE_REPO:-${WORKSPACE_ROOT:-$HOME/workspace}/bytes-n-rust/dev/spirited-drive} || exit 1
S=${GATE_WORK:-$TOOLS/work}
export GATE_LOCK=${GATE_LOCK:-$TOOLS/work/gate.lock}
export PATH=${GATE_NIX_BIN:-/nix/var/nix/profiles/default/bin}:$PATH   # GATE_NIX_BIN: tests/gate_v2_flow.sh puts its fake nix here
source $HERE/gate_lock.zsh
heavy() { if (( DRY )); then print -r -- "+ $1"; else eval "$1"; fi }   # one string: pipes and redirections stay inside
FAILS=(); bad() { FAILS+=("$1"); }

l=$(sysctl -n vm.loadavg | awk '{print int($2)}')   # no load wait: the owner asked (2026-11-08) that gates start at once
echo "start load $l $(date +%H:%M)$( (( DRY )) && echo '  DRY RUN: nothing is built or run')"

# the lock: one heavy run on this Mac; the nightly sweep waits for a gate, a gate waits for the sweep
KIND=${GATE_LOCK_KIND:-gate}
if (( DRY )); then
    echo "+ lock: $GATE_LOCK would hold kind=$KIND branch=$(git rev-parse --abbrev-ref HEAD) commit=$(git rev-parse --short HEAD)"
    [[ -e $GATE_LOCK ]] && echo "lock now: held by $(lock_field kind) $(lock_field branch) $(lock_field commit) since $(lock_field start) (pid $(lock_field pid))"
else
    w=0; [[ $(lock_field kind) == nightly ]] && w=7200
    lock_take $KIND ${GATE_LOCK_WAIT:-$w} || exit 1
    trap '[[ -n $MENU_PID ]] && kill $MENU_PID 2>/dev/null; lock_drop' EXIT
    trap 'exit 130' INT TERM
fi

# jobs: 2 only when heavy work is running right now (the load average lags a finished run by minutes; starts stay immediate)
HEAVY=$(ps -Ao comm= | grep -cE '(^|/)(clang|clang-[0-9]+|ninja|ctest|cc1|ld|tnfs)$')
J=4; (( HEAVY >= 3 )) && J=2
echo "jobs $J (heavy $HEAVY)"
T0=$SECONDS; TIMES=""; tm() { TIMES="$TIMES $1=$((SECONDS-T0))s"; T0=$SECONDS; }

# ccache, when it is installed (brew, or in the flake's devShell): the launcher is set once per tree, the counters zeroed
CCACHE=0
if nix develop -c sh -c 'command -v ccache' >/dev/null 2>&1; then
    CCACHE=1; export CCACHE_COMPILERCHECK=content   # nix's clang: every file of the store has the same date
    for b in build/release build/asan56; do
        grep -q '^CMAKE_C_COMPILER_LAUNCHER:[A-Z]*=.*ccache' $b/CMakeCache.txt 2>/dev/null ||
            heavy "nix develop -c cmake -S . -B $b -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > $S/gate_ccache_cfg.log 2>&1 || { tail -5 $S/gate_ccache_cfg.log; exit 1; }"
    done
    heavy "nix develop -c ccache -z > /dev/null"
fi
echo "ccache $( (( CCACHE )) && echo on || echo 'off (not installed)')"

# the diff against the base: what gate_select.py reads
BASE=${GATE_BASE:-origin/main}
MB=$(git merge-base $BASE HEAD 2>/dev/null)
git diff -U0 $MB HEAD -- CMakeLists.txt > $S/gate_cmake.patch 2>/dev/null   # gate_select.py: a CMakeLists.txt that only adds tests
if [[ -n $MB ]]; then git diff --name-only $MB HEAD > $S/gate_diff.txt; else : > $S/gate_diff.txt; echo "no merge-base with $BASE: full"; fi

PREV=${GATE_PREV:-$S/prev_tnfs_main}   # the build before this gate: menu_compare's reference
heavy "if [[ -x $PREV ]]; then cp -p $PREV $S/prev_tnfs; else cp -p build/release/tnfs $S/prev_tnfs 2>/dev/null; fi"
heavy "nix develop -c cmake --build build/release -j$J > $S/gate_rel.log 2>&1 && echo build_ok || { tail -20 $S/gate_rel.log; exit 1; }"
tm build_release
heavy "nix develop -c cmake --build build/asan56 -j$J > $S/gate_asan.log 2>&1 || { tail -20 $S/gate_asan.log; exit 1; }"
if (( ! DRY )); then
    W=$(grep -h 'warning:' $S/gate_rel.log $S/gate_asan.log | grep -v central_draw.c | sort -u | wc -l)
    echo "warnings $W"; (( W > 0 )) && bad warnings
fi
tm build_asan

# the scope and the two test lists, from the trees just built (their test lists and labels are this branch's)
nix develop -c ctest --test-dir build/release --show-only=json-v1 > $S/gate_tests_rel.json 2>/dev/null
nix develop -c ctest --test-dir build/asan56 --show-only=json-v1 > $S/gate_tests_asan.json 2>/dev/null
if ! python3 $HERE/gate_select.py --cmake-patch $S/gate_cmake.patch $S/gate_diff.txt $S/gate_tests_rel.json $S/gate_tests_asan.json $S/gate_sel ${GATE_SCOPE:-auto}; then
    echo "scope full (gate_select.py failed)"
    print full > $S/gate_sel.scope; print ALL > $S/gate_sel.rel; print ALL > $S/gate_sel.asan
fi
SCOPE=$(<$S/gate_sel.scope)
(( ! DRY )) && print -r -- "scope=$SCOPE" >> $GATE_LOCK
only() { [[ $(head -1 $1) == ALL ]] || print -r -- " --tests-from-file $1"; }   # every test, or the list's
GAME=1; [[ $SCOPE == editor || $SCOPE == none ]] && GAME=0                      # the game's checks below

heavy "(cd build/release && nix develop -c ctest -j$J$(only $S/gate_sel.rel) 2>&1 | grep -E 'tests passed|Failed|\*\*\*') | tee $S/gate_ctest_rel.out"
(( ! DRY )) && { grep -q '^100% tests passed' $S/gate_ctest_rel.out || bad ctest_release; }
tm ctest_release

# menu_compare runs beside the ASan ctest: it only reads build/release, already built and tested
: > $S/gate_menu.out
if (( GAME )) && [[ -x tools/menu_compare.sh ]] && { (( DRY )) || [[ -x $S/prev_tnfs ]]; }; then
    if (( DRY )); then echo "+ (in the background) JOBS=2 timeout -k 10 1500 nix shell --inputs-from . nixpkgs#imagemagick -c zsh tools/menu_compare.sh $S/prev_tnfs build/release/tnfs $S/menus"
    else (rm -rf $S/menus; JOBS=2 timeout -k 10 1500 nix shell --inputs-from . nixpkgs#imagemagick -c zsh tools/menu_compare.sh $S/prev_tnfs build/release/tnfs $S/menus 2>&1 | tail -1 > $S/gate_menu.out; [[ ${pipestatus[1]} == 124 ]] && echo 'MENU_COMPARE_TIMEOUT (25 min)' >> $S/gate_menu.out) & MENU_PID=$!
    fi
fi
echo -n "asan: "
heavy "(cd build/asan56 && UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 nix develop -c ctest -j$J$(only $S/gate_sel.asan) 2>&1 | grep -E 'tests passed|Failed|\*\*\*') | tee $S/gate_ctest_asan.out"
(( ! DRY )) && { grep -q '^100% tests passed' $S/gate_ctest_asan.out || bad ctest_asan; }
tm ctest_asan

# ui_kit_check (the UI kit's measures against the real binary and the CD): its walks read the release build, and its checkers want ImageMagick on the PATH, which the flake's devShell
# does not give (it broke 13 editor tests there), so it runs in its own nix shell. A FAIL line, a non-zero status or a skip (77: no CD) fails the gate; the count of ok lines is printed
ui_kit() {
    nix shell --inputs-from . nixpkgs#imagemagick -c sh tools/ui_kit_check.sh build/release/tnfs $PWD $PWD/original > $S/gate_uikit.out 2>&1; local rc=$?
    local ok=$(grep -c '^ok ' $S/gate_uikit.out) nf=$(grep -c '^FAIL' $S/gate_uikit.out)
    echo "ui_kit_check: status $rc, $ok ok, $nf FAIL"
    grep '^FAIL' $S/gate_uikit.out | head -5
    (( rc == 0 && nf == 0 && ok > 0 )) || { (( rc == 0 && ok == 0 )) && echo "ui_kit_check: no ok line: nothing was measured"; (( rc != 77 )) || echo "ui_kit_check: skipped (no CD in original/): not proven"; bad ui_kit_check; }
}
if (( GAME )); then
    if (( DRY )); then echo "+ nix shell --inputs-from . nixpkgs#imagemagick -c sh tools/ui_kit_check.sh build/release/tnfs \$PWD \$PWD/original (a FAIL line fails the gate; the ok count is printed)"
    elif [[ -x tools/ui_kit_check.sh ]]; then ui_kit
    else echo "ui_kit_check: not run (no tools/ui_kit_check.sh)"; fi
    tm ui_kit
    heavy "zsh tools/golden_frames.sh build/release $S/gate.md5 2>/dev/null && cmp -s $S/gate.md5 tests/golden/frames.md5 && echo golden_ok || { echo GOLDEN_DIFF; bad golden; }"
    tm golden
    heavy "zsh tools/frame_compare.sh build/release 'TNFS_BANDS=0' 'TNFS_BANDS=4' 2>&1 | tail -1 | tee $S/gate_frames.out"
    (( ! DRY )) && { grep -q ', 0 differ, 0 missing' $S/gate_frames.out || bad frame_compare; }
    tm frame_compare
    G=$S/gmods.$$
    heavy "cp -R ${GATE_MODS:-$S/mods} $G; mkdir -p $G/user
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_BANDS=auto timeout -k 5 15 build/release/tnfs --data original --mods $G --user $G/user --drive TR1 >/dev/null 2>&1; WRC=\$?; echo window_bands=\$WRC
rm -rf $G
[[ \$WRC == 124 || \$WRC == 0 ]] || bad window"   # 124: still driving when the 15 s ended, the usual pass
    tm window
    if (( ! DRY )); then
        wait $MENU_PID 2>/dev/null; MENU_PID=""; cat $S/gate_menu.out
        if [[ -s $S/gate_menu.out ]]; then { grep -q 'TIMEOUT' $S/gate_menu.out || ! grep -q ' 0 differ, 0 failed' $S/gate_menu.out; } && bad menu_compare
        else echo "menu_compare: not run (no reference binary or no tools/menu_compare.sh)"; fi
    fi
    tm menu_compare_wait
else
    echo "golden, frame_compare, window, menu_compare: skipped (scope $SCOPE: the diff does not touch the game)"
    tm golden; tm frame_compare; tm window; tm menu_compare_wait
fi
echo "apple_universal: now in GitHub CI (check the branch: gh run list -R BytesNRust/spirited-drive --workflow CI -b <branch> -L 1)"
if (( DRY )); then echo "+ verdict OK | verdict FAIL: <what failed>"
elif (( $#FAILS )); then echo "verdict FAIL: ${(j:, :)FAILS}"
else echo "verdict OK (scope $SCOPE)"; fi
(( CCACHE )) && heavy "nix develop -c ccache -s | grep -E '^ *(Hits|Misses):' | head -2 | tr -s ' ' | tr '\n' ' ' | sed 's/^/ccache/'; echo"
echo "times$TIMES"
(( $#FAILS == 0 ))
