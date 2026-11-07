#!/usr/bin/env bash
# Try the online mode on one Mac: two visible game windows, a host and a guest, talking over UDP on localhost.
# Each has its own player folder (./user and ./user-guest), the host listens on UDP 15000 and the guest on 15001 and looks for
# the host's game at 127.0.0.1:15000. The game's data is ./original and the mods folder ./mods. Both windows run under timeout -k
# (2 hours) and close together: closing one window, or Ctrl-C here, stops the other.
# usage: tools/try_online_local.sh [PATH_TO_TNFS]     (run it from the folder that holds original/ and mods/, or set TNFS_HOME)
# DRY_RUN=1 only prints the two commands. Nothing is written outside the folders named here.
set -euo pipefail

home=${TNFS_HOME:-$PWD}
cd "$home"
here=$(cd "$(dirname "$0")/.." && pwd)
exe=${1:-}
if [ -z "$exe" ]; then
    for c in ./tnfs ./build/release/tnfs "$here/build/release/tnfs"; do
        [ -x "$c" ] && { exe=$c; break; }
    done
fi
[ -n "$exe" ] && { [ -x "$exe" ] || [ -n "${DRY_RUN:-}" ]; } || { echo "no tnfs to run: give its path (tools/try_online_local.sh PATH_TO_TNFS)" >&2; exit 2; }

# the game's own windows: the SDL dummy drivers the test tools set are not wanted here
clean=(env -u SDL_VIDEO_DRIVER -u SDL_AUDIO_DRIVER)
common=(timeout -k 5 7200 "$exe" --data ./original --mods ./mods)
host=("${clean[@]}" "${common[@]}" --user ./user --net-port 15000)
guest=("${clean[@]}" "${common[@]}" --user ./user-guest --net-port 15001 --net-host 127.0.0.1:15000)

if [ -n "${DRY_RUN:-}" ]; then
    echo "host:  ${host[*]}"
    echo "guest: ${guest[*]}"
    exit 0
fi
[ -d ./original ] || { echo "no ./original (the game's data) in $PWD" >&2; exit 2; }
mkdir -p ./mods ./user ./user-guest

stop() {   # both windows go, whichever way this script ends
    [ -z "${host_pid:-}" ] || kill "$host_pid" 2>/dev/null || true
    [ -z "${guest_pid:-}" ] || kill "$guest_pid" 2>/dev/null || true
}
trap stop EXIT INT TERM
"${host[@]}" &
host_pid=$!
sleep 3   # the host's window first, so the guest finds its game when it looks
"${guest[@]}" &
guest_pid=$!
# when one window closes the other goes too (bash 3.2 on a Mac has no wait -n: look every second)
while kill -0 "$host_pid" 2>/dev/null && kill -0 "$guest_pid" 2>/dev/null; do sleep 1; done
exit 0
