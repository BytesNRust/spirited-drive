#!/bin/zsh
# The gate v2 scripts (tools/ci/gate_v2) run end to end on a scratch repo with fake nix, cmake, ctest and ccache: the lock is dropped when a build fails or the run is killed, the verdict and its exit code,
# ccache switched on once, what ASan runs per scope, a held and a stale lock, the dry run. Nothing of the real gate is touched (GATE_WORK and GATE_LOCK are the scratch's). usage: gate_v2_flow.sh REPO
setopt no_unset 2>/dev/null; unsetopt no_unset
repo=${1:A}
V2=$repo/tools/ci/gate_v2
work=$(mktemp -d); trap 'rm -rf $work' EXIT
fails=0; fail() { print -u2 "FAIL: $*"; fails=$((fails+1)); }

bin=$work/bin; mkdir -p $bin
cat > $bin/nix <<'SH'
#!/bin/sh
# nix develop -c CMD... runs CMD; nix shell ... -c CMD... runs it too (menu_compare is not part of these cases)
[ "$1" = shell ] && echo "nix $*" >> "$FAKE_LOG"
while [ $# -gt 0 ] && [ "$1" != -c ]; do shift; done
shift
exec "$@"
SH
cat > $bin/cmake <<'SH'
#!/bin/sh
echo "cmake $*" >> "$FAKE_LOG"
case "$*" in
  *--build*) [ -n "$FAKE_BUILD_FAIL" ] && { echo "error: the build is broken"; exit 2; }; exit 0 ;;
  *-B*) b=$(echo "$*" | sed 's/.*-B \([^ ]*\).*/\1/'); mkdir -p "$b"; echo 'CMAKE_C_COMPILER_LAUNCHER:STRING=ccache' >> "$b/CMakeCache.txt"; exit 0 ;;
esac
SH
cat > $bin/ctest <<'SH'
#!/bin/sh
echo "ctest $PWD $*" >> "$FAKE_LOG"
case "$*" in
  *--show-only*) cat "$FAKE_JSON"; exit 0 ;;
esac
[ -n "$FAKE_CTEST_SLEEP" ] && sleep "$FAKE_CTEST_SLEEP"
case "$PWD" in *asan56) [ -n "$FAKE_ASAN_FAIL" ] && { echo "95% tests passed, 3 tests failed out of 60"; exit 8; } ;; esac
echo "100% tests passed, 0 tests failed out of 60"
SH
cat > $work/ccache.src <<'SH'
#!/bin/sh
echo "ccache $*" >> "$FAKE_LOG"
[ "$1" = -s ] && printf 'Cacheable calls:  10\n  Hits:  7 / 10 (70.00 %%)\n  Misses:  3 / 10\n'
exit 0
SH
chmod +x $bin/* $work/ccache.src
cat > $work/tests.json <<'JS'
{"tests":[
 {"name":"u1","command":["/b/u1"],"properties":[{"name":"LABELS","value":["unit","core"]}]},
 {"name":"g1","command":["/b/g1"],"properties":[{"name":"LABELS","value":["integration","game"]}]},
 {"name":"e1","command":["/b/e1"],"properties":[{"name":"LABELS","value":["integration","editor"]}]}]}
JS

# a scratch repo with an origin/main and a branch one commit ahead; FILE is the path the branch changes
make_repo() {
    rm -rf $work/origin.git $work/repo; git init -q --bare $work/origin.git
    git clone -q $work/origin.git $work/repo 2>/dev/null
    (cd $work/repo && git checkout -q -b main && mkdir -p docs editor game build/release build/asan56 && echo a > docs/a.md && echo a > editor/a.c && echo a > game/a.c &&
     git add . && git -c user.email=t@t -c user.name=t commit -q -m base && git push -q origin main 2>/dev/null && git checkout -q -b feat &&
     mkdir -p $(dirname $1) && echo b >> $1 && git add . && git -c user.email=t@t -c user.name=t commit -q -m change)
}
# gate [ENV=VALUE ...]: the gate on the scratch repo; output in $work/out, status in $rc
gate() {
    rm -f $work/gate.lock $work/fake.log; : > $work/fake.log
    env PATH=$bin:$PATH GATE_NIX_BIN=$bin FAKE_LOG=$work/fake.log FAKE_JSON=$work/tests.json GATE_REPO=$work/repo GATE_WORK=$work/s GATE_LOCK=$work/gate.lock "$@" zsh $V2/gate_main_v2.sh > $work/out 2>&1
    rc=$?
}
mkdir -p $work/s

# 1. a docs-only branch passes: verdict OK, exit 0, lock gone, the game's checks skipped, ASan cut to the units
make_repo docs/a.md
gate; [[ $rc == 0 ]] || fail "a passing gate exits $rc: $(tail -5 $work/out)"
grep -q '^verdict OK (scope none)' $work/out || fail "no 'verdict OK (scope none)': $(tail -5 $work/out)"
[[ ! -e $work/gate.lock ]] || fail "the lock was left behind after a pass"
grep -q 'skipped (scope none' $work/out || fail "the game's checks ran for a docs diff"
grep -q 'ctest .*asan56 .*--tests-from-file' $work/fake.log || fail "ASan ran every test for a docs diff: $(grep ctest $work/fake.log)"
grep -q '^times ' $work/out || fail "no times line"

# 2. a game change: ASan runs every test (no --tests-from-file), release the game's list
make_repo game/a.c
gate GATE_SCOPE=auto
grep -q '^scope=\|scope game' $work/out || true
grep 'ctest .*asan56' $work/fake.log | grep -v show-only | grep -q 'tests-from-file' && fail "ASan was cut for a game change: $(grep asan56 $work/fake.log)"
grep 'ctest .*release' $work/fake.log | grep -v show-only | grep -q 'tests-from-file' || fail "release ran every test for a game change"

# 2b. ui_kit_check runs in the gate for a game change, in a nix shell with imagemagick, after the release build: its ok lines are counted, a FAIL line or a bad status fails the gate, a skip is not a pass
fake_uikit() { mkdir -p $work/repo/tools; printf '#!/bin/sh\necho "ok   /x/a.png: arrow_w 9 (CD 9)"\necho "ok   /x/a.png: gap_left 14 (CD 14)"\n[ -n "$FAKE_UIKIT_FAIL" ] && echo "FAIL /x/a.png: gap_right 3 (CD 15)"\n[ -n "$FAKE_UIKIT_SKIP" ] && { echo "skip: no CD"; exit 77; }\n[ -n "$FAKE_UIKIT_FAIL" ] && exit 1\nexit 0\n' > $work/repo/tools/ui_kit_check.sh; chmod +x $work/repo/tools/ui_kit_check.sh; }
make_repo game/a.c; fake_uikit
gate GATE_SCOPE=auto
grep -q '^ui_kit_check: status 0, 2 ok, 0 FAIL' $work/out || fail "the gate did not print the ui_kit_check count: $(grep ui_kit $work/out)"
grep -q 'nix shell .*imagemagick' $work/fake.log || fail "ui_kit_check ran without an imagemagick shell: $(grep '^nix' $work/fake.log)"
grep -q '^verdict FAIL:.*ui_kit_check' $work/out && fail "a passing ui_kit_check failed the gate: $(grep verdict $work/out)"
make_repo game/a.c; fake_uikit
gate GATE_SCOPE=auto FAKE_UIKIT_FAIL=1
grep -q '^verdict FAIL:.*ui_kit_check' $work/out || fail "a FAIL line did not fail the gate: $(grep -E 'ui_kit|verdict' $work/out)"
grep -q '^FAIL /x/a.png: gap_right' $work/out || fail "the FAIL line was not printed: $(grep -E 'ui_kit|FAIL' $work/out)"
make_repo game/a.c; fake_uikit
gate GATE_SCOPE=auto FAKE_UIKIT_SKIP=1
grep -q '^verdict FAIL:.*ui_kit_check' $work/out || fail "a skipped ui_kit_check passed the gate: $(grep -E 'ui_kit|verdict' $work/out)"
make_repo docs/a.md; fake_uikit
gate
grep -q 'ui_kit_check: status' $work/out && fail "ui_kit_check ran for a docs diff"

# 3. a build that fails: exit 1, no verdict (nightly says so), the lock is dropped (the trap on exit inside eval)
make_repo docs/a.md
gate FAKE_BUILD_FAIL=1
[[ $rc != 0 ]] || fail "a failed build exited 0"
[[ ! -e $work/gate.lock ]] || fail "the lock was left behind after a failed build: $(cat $work/gate.lock)"
grep -q 'the build is broken' $work/out || fail "the build's last lines were not printed"
grep -q '^verdict' $work/out && fail "a failed build printed a verdict"

# 4. a failing ASan run: verdict FAIL ctest_asan, exit 1, lock dropped
gate FAKE_ASAN_FAIL=1
[[ $rc == 1 ]] || fail "a failed ASan ctest exits $rc, not 1"
grep -q '^verdict FAIL: ctest_asan' $work/out || fail "the verdict does not name ctest_asan: $(tail -4 $work/out)"
[[ ! -e $work/gate.lock ]] || fail "the lock was left behind after a failed ASan run"

# 5. a gate killed in the middle (TERM): the lock goes
rm -f $work/gate.lock; : > $work/fake.log
env PATH=$bin:$PATH GATE_NIX_BIN=$bin FAKE_LOG=$work/fake.log FAKE_JSON=$work/tests.json FAKE_CTEST_SLEEP=30 GATE_REPO=$work/repo GATE_WORK=$work/s GATE_LOCK=$work/gate.lock zsh $V2/gate_main_v2.sh > $work/out 2>&1 &
gpid=$!
for _ in $(seq 1 100); do [[ -e $work/gate.lock ]] && grep -q 'ctest' $work/fake.log && break; sleep 0.2; done
[[ -e $work/gate.lock ]] || fail "the lock was not taken while the gate ran"
grep -q "^pid=$gpid\$" $work/gate.lock || fail "the lock does not name the gate's pid: $(cat $work/gate.lock)"
pkill -TERM -P $gpid 2>/dev/null; kill -TERM $gpid 2>/dev/null; wait $gpid 2>/dev/null
[[ ! -e $work/gate.lock ]] || fail "the lock was left behind after a TERM: $(cat $work/gate.lock)"

# 6. a held lock (a live pid) refuses at once with exit 1 and does not touch the lock; a stale lock is taken over
printf 'kind=nightly\nbranch=main\ncommit=abc\nstart=now\npid=%s\nrepo=x\n' $$ > $work/gate.lock
env PATH=$bin:$PATH GATE_NIX_BIN=$bin FAKE_LOG=$work/fake.log FAKE_JSON=$work/tests.json GATE_REPO=$work/repo GATE_WORK=$work/s GATE_LOCK=$work/gate.lock GATE_LOCK_WAIT=0 zsh $V2/gate_main_v2.sh > $work/out 2>&1; rc=$?
[[ $rc == 1 ]] && grep -q '^lock: held by nightly' $work/out || fail "a held lock was not refused (rc $rc): $(head -5 $work/out)"
grep -q "^pid=$$\$" $work/gate.lock || fail "the refused gate changed the held lock"
printf 'kind=gate\nbranch=x\ncommit=abc\nstart=old\npid=999999\nrepo=x\n' > $work/gate.lock
env PATH=$bin:$PATH GATE_NIX_BIN=$bin FAKE_LOG=$work/fake.log FAKE_JSON=$work/tests.json GATE_REPO=$work/repo GATE_WORK=$work/s GATE_LOCK=$work/gate.lock zsh $V2/gate_main_v2.sh > $work/out 2>&1; rc=$?
grep -q '^lock: stale' $work/out && [[ $rc == 0 ]] || fail "a stale lock was not taken over (rc $rc): $(head -5 $work/out)"

# 7. ccache installed: the two trees are reconfigured once (not again once their cache has the launcher), the counters are printed
make_repo docs/a.md
mkdir -p $work/withcc; cp $bin/* $work/withcc/; cp $work/ccache.src $work/withcc/ccache
rm -f $work/repo/build/*/CMakeCache.txt(N)
rm -f $work/gate.lock; : > $work/fake.log
env PATH=$work/withcc:$PATH GATE_NIX_BIN=$work/withcc FAKE_LOG=$work/fake.log FAKE_JSON=$work/tests.json GATE_REPO=$work/repo GATE_WORK=$work/s GATE_LOCK=$work/gate.lock zsh $V2/gate_main_v2.sh > $work/out 2>&1; rc=$?
grep -q '^ccache on' $work/out || fail "ccache on PATH was not seen: $(sed -n 1,8p $work/out)"
[[ $(grep -c 'DCMAKE_C_COMPILER_LAUNCHER=ccache' $work/fake.log) == 2 ]] || fail "the two trees were not each configured once with the launcher: $(grep cmake $work/fake.log)"
grep -q 'ccache Hits' $work/out || fail "no ccache counters at the end: $(tail -4 $work/out)"
rm -f $work/gate.lock; : > $work/fake.log
env PATH=$work/withcc:$PATH GATE_NIX_BIN=$work/withcc FAKE_LOG=$work/fake.log FAKE_JSON=$work/tests.json GATE_REPO=$work/repo GATE_WORK=$work/s GATE_LOCK=$work/gate.lock zsh $V2/gate_main_v2.sh > $work/out 2>&1
grep -q 'DCMAKE_C_COMPILER_LAUNCHER=ccache' $work/fake.log && fail "the trees were configured again on the second gate"

# 8. the dry run: scope and counts printed, heavy commands as '+', no lock, no build
make_repo docs/a.md
rm -f $work/gate.lock; : > $work/fake.log
env PATH=$bin:$PATH GATE_NIX_BIN=$bin FAKE_LOG=$work/fake.log FAKE_JSON=$work/tests.json GATE_REPO=$work/repo GATE_WORK=$work/s GATE_LOCK=$work/gate.lock zsh $V2/gate_main_v2.sh -n > $work/out 2>&1; rc=$?
[[ $rc == 0 ]] && grep -q 'DRY RUN' $work/out && grep -q '^+ ' $work/out || fail "dry run (rc $rc): $(head -6 $work/out)"
grep -q 'cmake --build' $work/fake.log && fail "the dry run built"
[[ ! -e $work/gate.lock ]] || fail "the dry run took the lock"

(( fails == 0 )) && echo ok
exit $(( fails > 0 ))
