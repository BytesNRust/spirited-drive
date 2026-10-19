#!/usr/bin/env bash
# A link race on one machine: two tnfs instances (the host on UDP 15000, the
# client on 15001), each with its own --mods scratch directory and the SDL
# dummy drivers, driven by input scripts through the network menus into a race
# on the network game's default track (Coastal, segmented), the humans driven
# by TNFS_AUTOPILOT=1 (a test harness only). Then, by MODE:
#   finish    both machines reach the race's end ("race over" in both logs)
#   continue  Done on the results and Continue at the Checkpoint: both race
#             segment 2 to its end
#   quit      Done, Quit game and Yes (the Y key) at the Checkpoint, then
#             Exit at Game Mode: both exit by themselves before their timeout
#   quitclick the same with Yes clicked (the mouse)
# usage: tools/link_loopback.sh BUILD_DIR [finish|continue|quit|quitclick] [OUT_DIR]
# Every machine's "race over" standings must also match the host's. CLIENTS
# (1) machines join: "client" on UDP 15001, then "client2" on 15002 and so on,
# each STAGGER s (10) after the one before; the host's Start and RESULTS_AT
# move on by as much (a client gives up waiting for the Start 40 s after its
# join).
# The script's times are wall-clock ms from each instance's start; the clients
# start DELAY s (25) after the host. RESULTS_AT (362000, the host's) is when
# the results screen is answered; raise it for a slower build. The logs stay in
# OUT_DIR (default: a new temporary directory); CLIENT_CONFIG=FILE is the first
# client's config.dat (its name, car and gearbox); CAPTURE_EVERY=N also keeps
# every Nth frame there (OUT_DIR/NAME/cap, PPMs: delete them after).
set -euo pipefail

build=${1:?usage: tools/link_loopback.sh BUILD_DIR [finish|continue|quit|quitclick] [OUT_DIR]}
mode=${2:-finish}
exe=$(cd "$build" && pwd)/tnfs
out=${3:-$(mktemp -d -t tnfs_link)}
delay=${DELAY:-25}
clients=${CLIENTS:-1}
viewers=${VIEWERS:-0}
stagger=${STAGGER:-10}
late=$(( (clients + viewers - 1) * stagger * 1000 ))  # the last joiner's start past the first's
at=$(( ${RESULTS_AT:-362000} + late ))
cd "$(dirname "$0")/.."
[ -x "$exe" ] || { echo "no $exe" >&2; exit 2; }

# Game Mode: Network; the service; host or join; the host starts the race (the menus' Start at 40, 450)
nav="1000:ESC,2500:ESC,6000:DOWN,7000:DOWN,8000:ENTER,10000:ENTER,14000:DOWN,16000:ENTER"
host="$nav,20000:ENTER,$((55000 + late)):CLICK:40:450"
client="$nav,22000:ENTER"
# a viewer: View only (below Select game) turned on, then Select game and the game
viewer="1000:ESC,2500:ESC,6000:DOWN,7000:DOWN,8000:ENTER,10000:ENTER,12000:DOWN,13000:DOWN,14000:ENTER,15000:UP,16000:ENTER,22000:ENTER"

# The steps after the race, at the host's times (the client's DELAY s earlier)
case $mode in
finish)   after=""; secs=$((at / 1000 - 20)) ;;
continue) after="0:ENTER,20000:ENTER"; secs=$((at / 1000 + 300)) ;;
quit | quitclick)
          yes=Y
          [ "$mode" = quitclick ] && yes=CLICK:230:262  # the box's Yes
          after="0:ENTER,18000:CLICK:320:403,28000:$yes,40000:DOWN,41000:DOWN,42000:DOWN,44000:ENTER,47000:Y"
          secs=$((at / 1000 + 70)) ;;
*) echo "mode: finish, continue, quit or quitclick" >&2; exit 2 ;;
esac
steps() {  # $1: the shift (ms) from the host's times
    local s list=""
    for s in ${after//,/ }; do list="$list,$((at + ${s%%:*} - $1)):${s#*:}"; done
    printf '%s' "$list"
}

run() {  # name script secs port [args...]
    local name=$1 script=$2 secs=$3 port=$4
    shift 4
    mkdir -p "$out/$name/mods"
    if [ "$name" = client ] && [ -n "${CLIENT_CONFIG:-}" ]; then
        mkdir -p "$out/$name/mods/user" && cp "$CLIENT_CONFIG" "$out/$name/mods/user/config.dat"
    fi
    [ -n "${CAPTURE_EVERY:-}" ] && mkdir -p "$out/$name/cap"
    set +e
    TNFS_CAPTURE_DIR=${CAPTURE_EVERY:+$out/$name/cap} TNFS_CAPTURE_EVERY=${CAPTURE_EVERY:-} SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_AUTOPILOT=1 TNFS_INPUT_SCRIPT=$script \
        timeout -k 5 "$secs" "$exe" --mods "$out/$name/mods" --net-port "$port" "$@" >"$out/$name/log" 2>&1
    echo $? >"$out/$name/rc"
}

machines=host
run host "$host$(steps 0)" "$secs" 15000 &
sleep "$delay"
for i in $(seq 1 $((clients + viewers))); do
    kind=client script=$client k=$i
    [ "$i" -gt "$clients" ] && kind=viewer script=$viewer k=$((i - clients))
    name=$kind
    [ "$k" -gt 1 ] && name=$kind$k
    machines="$machines $name"
    start=$(( delay + (i - 1) * stagger ))
    run "$name" "$script$(steps $((start * 1000)))" $((secs - start)) $((15000 + i)) --net-host 127.0.0.1:15000 &
    sleep "$stagger"
done
wait

fail=0
for name in $machines; do
    rc=$(cat "$out/$name/rc")
    ends=$(grep -ac "race over" "$out/$name/log" || true)
    case $mode in
    finish)   [ "$ends" -ge 1 ] ;;
    continue) [ "$ends" -ge 2 ] ;;
    quit*)    [ "$ends" -ge 1 ] && [ "$rc" = 0 ] ;;
    esac && [ "$(grep -a "race over" "$out/$name/log")" = "$(grep -a "race over" "$out/host/log")" ] \
        && verdict=PASS || { verdict=FAIL; fail=1; }
    echo "$name: $verdict (races ended $ends, rc $rc)"
done
grep -a "race over" "$out/host/log" || true
echo "logs: $out"
exit $fail
