#!/usr/bin/env bash
# A link race on one machine: two or more tnfs instances (the host on UDP 15000, the clients on 15001..), each with
# its own --user scratch directory and the SDL dummy drivers, driven by input scripts through the network menus into
# a race on the network game's default track (Coastal, segmented), the humans driven by TNFS_AUTOPILOT=1 (a test
# harness only). Then, by MODE:
#   finish    both machines reach the race's end ("race over" in both logs)
#   continue  Done on the results and Continue at the Checkpoint: both race segment 2 to its end
#   quit      Done, Quit game and Yes (the Y key) at the Checkpoint, then Exit at Game Mode: every machine exits by itself
#   quitclick the same with Yes clicked (the mouse)
# The steps do not wait on wall-clock times: TNFS_INPUT_SETTLE sequences each script (an event comes a pause after the one before,
# a click once the screen has stilled) and where one machine depends on another the script holds on a line the game logs
# (platform_sdl3.c's WAIT; the "mark:..." lines): the guests start when the host logs mark:game_hosted (guest k+1 when mark:joined=k),
# a guest picks the game once it logs mark:game_listed, the host presses Start once every guest has joined (mark:joined=N), and the steps after the race
# come after mark:race_over. A step that does not come in time stops the run with the step named: "input script: step N: WAIT text
# timed out after S s" (the instance exits with status 3) and this script says so and which machine.
# usage: tools/link_loopback.sh BUILD_DIR [finish|continue|quit|quitclick] [OUT_DIR]
# Environment:
#   BASE_PORT (15000)      the host's UDP port, the clients' the ones after it
#   CLIENTS (1), VIEWERS (0)  machines that join: "client" on 15001, "client2" ..., then the View-only ones ("viewer", ...)
#   STEP_TIMEOUT (120)     seconds a step may wait for a line of another machine (the game listed, a guest seated)
#   RACE_TIMEOUT (900)     seconds the race may take, from the Start to mark:race_over (slower under ASan: raise it)
#   OVERALL (3600)         the whole run's limit, seconds (each instance also runs under timeout -k 5)
#   SETTLE (1500)          TNFS_INPUT_SETTLE, ms
#   HOST_ENV / CLIENT_ENV="VAR=value ..."  environment of the host / of every client (the host-only rules: HOST_ENV="TNFS_WEATHER=rain
#                          TNFS_REVERSE=1"; the clients' logs then say "session rules" on a build that has them)
#   CLIENT_CONFIG=FILE     the first client's config.dat (its name, car and gearbox)
#   CAPTURE_EVERY=N        also keeps every Nth frame in OUT_DIR/NAME/cap (PPMs: delete them after)
#   CLIENT_IDLE=1          the clients press nothing after the race (only the host answers the screens)
#   AFTER="ms:KEY,..."     the steps after the race (each ms after the one before) instead of the mode's
# The logs stay in OUT_DIR (default: a new temporary directory); every machine's "race over" standings must match the host's.
set -euo pipefail

build=${1:?usage: tools/link_loopback.sh BUILD_DIR [finish|continue|quit|quitclick] [OUT_DIR]}
mode=${2:-finish}
exe=$(cd "$build" && pwd)/tnfs
out=${3:-$(mktemp -d -t tnfs_link)}
clients=${CLIENTS:-1}
viewers=${VIEWERS:-0}
step_timeout=${STEP_TIMEOUT:-120}
race_timeout=${RACE_TIMEOUT:-900}
overall=${OVERALL:-3600}
settle=${SETTLE:-1500}
cd "$(dirname "$0")/.."
[ -x "$exe" ] || { echo "no $exe" >&2; exit 2; }
humans=$((1 + clients + viewers))

# Game Mode: Network; the service; the host/join box
nav="1000:ESC,1500:ESC,4500:DOWN,1000:ENTER,2000:ENTER,4000:DOWN,2000:ENTER"
wait_for() { printf '0:WAIT:%s:%s' "$1" "$2"; }   # the script holds for a line of the game's
host="$nav,4000:ENTER,$(wait_for "mark:joined=$((humans - 1))" "$((step_timeout + 60 * humans))"),1000:CLICK:40:450"
# LEFT then RIGHT ends on the first game whether the box opened with the game listed (that row selected) or without (New selected)
pick="500:LEFT,500:RIGHT,500:ENTER"
client="$nav,$(wait_for mark:game_listed "$step_timeout"),$pick"
# a viewer: View only (below Select game) turned on, then Select game and the game (no New for it: Cancel selected when the box opened
# with no game, the game when with one; LEFT goes from Cancel to the game and stays on it)
viewer="1000:ESC,1500:ESC,4500:DOWN,1000:ENTER,2000:ENTER,2000:DOWN,1000:DOWN,1000:ENTER,1000:UP,1000:ENTER,$(wait_for mark:game_listed "$step_timeout"),500:LEFT,500:ENTER"

# The steps after the race, each a pause after the one before
race_over=$(wait_for mark:race_over "$race_timeout")
case $mode in
finish)   after=""; races=1 ;;
continue) after="10000:ENTER,20000:ENTER,$(wait_for mark:race_over "$race_timeout")"; races=2 ;;
quit | quitclick)
          yes=Y
          [ "$mode" = quitclick ] && yes=CLICK:230:262  # the box's Yes
          after="10000:ENTER,18000:CLICK:320:403,10000:$yes,13000:DOWN,1000:DOWN,2000:ENTER,3000:Y"
          races=1 ;;
*) echo "mode: finish, continue, quit or quitclick" >&2; exit 2 ;;
esac
[ -n "${AFTER:-}" ] && after=$AFTER
steps="$race_over${after:+,$after}"

run() {  # name script port [args...]
    local name=$1 script=$2 port=$3
    shift 3
    mkdir -p "$out/$name/mods"
    if [ "$name" = client ] && [ -n "${CLIENT_CONFIG:-}" ]; then
        mkdir -p "$out/$name/mods/user" && cp "$CLIENT_CONFIG" "$out/$name/mods/user/config.dat"
    fi
    [ -n "${CAPTURE_EVERY:-}" ] && mkdir -p "$out/$name/cap"
    local extra=${HOST_ENV:-}
    [ "$name" != host ] && extra=${CLIENT_ENV:-}
    set +e
    env $extra TNFS_CAPTURE_DIR=${CAPTURE_EVERY:+$out/$name/cap} TNFS_CAPTURE_EVERY=${CAPTURE_EVERY:-} SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
        TNFS_VERBOSE=1 TNFS_AUTOPILOT=1 TNFS_INPUT_SETTLE="$settle" TNFS_INPUT_SCRIPT="$script" \
        timeout -k 5 "$overall" "$exe" --user "$out/$name/mods/user" --net-port "$port" "$@" >"$out/$name/log" 2>&1 &
    local pid=$!
    echo "$pid" >"$out/$name/pid"
    wait "$pid"
    echo $? >"$out/$name/rc"
}

logged() { local n; n=$(grep -ac "^$1" "$out/$2/log" 2>/dev/null || true); echo "${n:-0}"; }   # how many times a machine logged a line of its own (at the start of a line: the script's own "waiting for mark:..." lines do not count)

# waits (at most `limit` s) until `test` (a command) holds; false on a machine's step timing out or the limit
failed_step=""
await() {  # description limit test...
    local what=$1 limit=$2 t=0
    shift 2
    while ! "$@"; do
        for m in $machines; do
            if [ -f "$out/$m/rc" ] && [ "$(cat "$out/$m/rc")" = 3 ]; then
                failed_step="$m: $(grep -a 'input script: step' "$out/$m/log" | head -1)"
                return 1
            fi
        done
        [ "$t" -ge "$limit" ] && { failed_step="nothing came in ${limit} s: $what"; return 1; }
        sleep 2
        t=$((t + 2))
    done
}

host_hosted() { [ "$(logged mark:game_hosted host)" -ge 1 ]; }
joined() { [ "$(logged "mark:joined=$1" host)" -ge 1 ]; }
over_everywhere() { for m in $machines; do [ "$(logged mark:race_over "$m")" -ge "$races" ] || return 1; done; }
all_exited() { for m in $machines; do [ -f "$out/$m/rc" ] || return 1; done; }
done_all() { if [ "$mode" = finish ] || [ "$mode" = continue ]; then over_everywhere; else all_exited; fi; }

machines=host
port=${BASE_PORT:-15000}
run host "$host${steps:+,$steps}" "$port" &
ok=1
await "the host hosting a game (mark:game_hosted)" "$step_timeout" host_hosted || ok=0
for i in $(seq 1 $((clients + viewers))); do
    [ "$ok" = 1 ] || break
    kind=client script=$client k=$i
    [ "$i" -gt "$clients" ] && kind=viewer script=$viewer k=$((i - clients))
    name=$kind
    [ "$k" -gt 1 ] && name=$kind$k
    # a guest starts once the one before has joined (the host logs mark:joined=N for the N-th guest)
    [ "$i" -gt 1 ] && { await "guest $((i - 1)) joining (mark:joined=$((i - 1)))" "$((step_timeout + 60))" joined "$((i - 1))" || { ok=0; break; }; }
    machines="$machines $name"
    after_steps=$steps
    [ "$kind" = client ] && [ -n "${CLIENT_IDLE:-}" ] && after_steps=$race_over   # the client's user answers none of the screens
    run "$name" "$script${after_steps:+,$after_steps}" $((port + i)) --net-host 127.0.0.1:"$port" &
done
[ "$ok" = 1 ] && { await "the race to end on every machine" "$overall" done_all || ok=0; }

# stop what is still running (a finished race leaves the machines on the results screen)
for m in $machines; do
    [ -f "$out/$m/rc" ] || kill -TERM "$(cat "$out/$m/pid" 2>/dev/null || echo 0)" 2>/dev/null || true
done
wait

fail=0
[ "$ok" = 1 ] || { echo "FAIL: stopped at: $failed_step"; fail=1; }
for name in $machines; do
    rc=$(cat "$out/$name/rc")
    ends=$(logged mark:race_over "$name")
    verdict=PASS
    case $mode in
    finish | continue) [ "$ends" -ge "$races" ] || verdict=FAIL ;;
    quit*)    [ "$ends" -ge 1 ] && [ "$rc" = 0 ] || verdict=FAIL ;;
    esac
    [ "$rc" = 3 ] && verdict=FAIL
    [ "$(grep -a "race over" "$out/$name/log")" = "$(grep -a "race over" "$out/host/log")" ] || verdict=FAIL
    [ "$verdict" = PASS ] || fail=1
    echo "$name: $verdict (races ended $ends, rc $rc)"
done
grep -a "race over" "$out/host/log" || true
echo "logs: $out"
exit $fail
