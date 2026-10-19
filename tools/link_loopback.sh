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
# The script's times are wall-clock ms from each instance's start; the client
# starts DELAY s (25) after the host. RESULTS_AT (362000, the host's) is when
# the results screen is answered; raise it for a slower build. The logs stay in
# OUT_DIR (default: a new temporary directory).
set -euo pipefail

build=${1:?usage: tools/link_loopback.sh BUILD_DIR [finish|continue|quit|quitclick] [OUT_DIR]}
mode=${2:-finish}
exe=$(cd "$build" && pwd)/tnfs
out=${3:-$(mktemp -d -t tnfs_link)}
delay=${DELAY:-25}
at=${RESULTS_AT:-362000}
cd "$(dirname "$0")/.."
[ -x "$exe" ] || { echo "no $exe" >&2; exit 2; }

# Game Mode: Network; the service; host or join; the host starts the race (the menus' Start at 40, 450)
nav="1000:ESC,2500:ESC,6000:DOWN,7000:DOWN,8000:ENTER,10000:ENTER,14000:DOWN,16000:ENTER"
host="$nav,20000:ENTER,55000:CLICK:40:450,61000:CLICK:40:450"
client="$nav,22000:ENTER"

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
    set +e
    SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_AUTOPILOT=1 TNFS_INPUT_SCRIPT=$script \
        timeout -k 5 "$secs" "$exe" --mods "$out/$name/mods" --net-port "$port" "$@" >"$out/$name/log" 2>&1
    echo $? >"$out/$name/rc"
}

run host "$host$(steps 0)" "$secs" 15000 &
sleep "$delay"
run client "$client$(steps $((delay * 1000)))" $((secs - delay)) 15001 --net-host 127.0.0.1:15000 &
wait

fail=0
for name in host client; do
    rc=$(cat "$out/$name/rc")
    ends=$(grep -ac "race over" "$out/$name/log" || true)
    case $mode in
    finish)   [ "$ends" -ge 1 ] ;;
    continue) [ "$ends" -ge 2 ] ;;
    quit*)    [ "$ends" -ge 1 ] && [ "$rc" = 0 ] ;;
    esac && verdict=PASS || { verdict=FAIL; fail=1; }
    echo "$name: $verdict (races ended $ends, rc $rc)"
done
echo "logs: $out"
exit $fail
