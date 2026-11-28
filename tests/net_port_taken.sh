#!/usr/bin/env bash
# The network screen when its UDP port is already taken: the game opens its games box, the network service does not
# start ("net_udp_open: port N: Address already in use", "BUG: init network service FAILED") and the screen is left with a line in
# the log. It read a NULL there and crashed (SIGSEGV, status 139). The port taken is a free one found by tools/free_udp_ports.py
# and claimed, as the link tests' own are, so other ctests of the machine do not meet it.
# The game's own run, not tools/link_loopback.sh: that waits for a hosted game, which never comes (a step's 60 s sat out). Here the script waits for the refusal and then leaves
# the network screen and the game by Game Mode's Escape ("Exit to system ?", Yes): a game that left the screen and lives ends with status 0.
# usage: tests/net_port_taken.sh BUILD_DIR   (the folder holding tnfs; run from anywhere)
set -u
build=${1:?usage: net_port_taken.sh BUILD_DIR}
exe=$(cd "$build" && pwd)/tnfs
repo=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
locks=${TNFS_LINK_LOCKS:-${TMPDIR:-/tmp}/tnfs_link_ports}
holder="" base=""
cleanup() {
    [ -n "$holder" ] && kill "$holder" 2>/dev/null
    [ -n "$base" ] && python3 -I -c 'import sys; sys.path.insert(0, sys.argv[1]); import free_udp_ports as F; F.release(sys.argv[2], int(sys.argv[3]))' "$repo/tools" "$locks" "$base"
    find "${work:?}" -delete 2>/dev/null
}
trap cleanup EXIT
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }

[ -f "$repo/original/SIMDATA/MISC/AL1.TRI" ] || { echo "skip: no original/"; exit 77; }
[ -x "$exe" ] || { echo "FAIL: no $exe" >&2; exit 1; }
base=$(python3 -I "$repo/tools/free_udp_ports.py" 16100 1 --lock-dir "$locks") || { echo "FAIL: no free port" >&2; exit 1; }
python3 -I -c 'import socket, sys, time
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.bind(("", int(sys.argv[1]))); print("held", flush=True); time.sleep(600)' "$base" >"$work/holder" &
holder=$!
disown "$holder" 2>/dev/null   # its kill at the end is no news
for _ in $(seq 1 50); do grep -q held "$work/holder" 2>/dev/null && break; sleep 0.1; done
grep -q held "$work/holder" || { echo "FAIL: the port $base was not taken" >&2; exit 1; }

# the way in as tools/link_loopback.sh's host: the movie, the title, Game Mode: Network; the name box; the games box. Its refusal leaves the network screen up: Escape to Game Mode, Escape, Yes
script="1000:ESC,0:WAIT:mark:movie=:60,500:ESC,0:WAIT:mark:screen=title:60,1000:ESC,0:WAIT:mark:screen=game_mode:60"
script="$script,1500:DOWN,1000:ENTER,0:WAIT:mark:screen=name_box:60,500:ENTER,4000:DOWN,2000:ENTER"
script="$script,0:WAIT:net_host: no network service:60,3000:ESC,3000:ESC,0:WAIT:mark:yes_no:60,500:Y"   # Escape leaves the network screen, Escape again asks (a dropped one fails the wait); the box drains keys sent before it reads them
mkdir -p "$work/user"
log=$work/log
(cd "$repo" && env SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_VERBOSE=1 TNFS_INPUT_SETTLE=1500 TNFS_INPUT_SCRIPT="$script" \
    timeout -k 5 150 "$exe" --user "$work/user" --net-port "$base" >"$log" 2>&1)
rc=$?
grep -q "Address already in use" "$log" || fail "the game did not meet the taken port: $(tail -3 "$log")"
grep -q "net_host: no network service" "$log" || fail "the games box did not say there is no network service: $(tail -3 "$log")"
grep -qaE "AddressSanitizer|runtime error:|SEGV" "$log" && fail "the game crashed on the taken port"
[ "$rc" = 0 ] || fail "the game did not leave the screen and exit by itself (status $rc; 139, 134, 138: died of a signal; 3: a step timed out): $(tail -3 "$log")"
[ "$failures" = 0 ] && echo "ok: the game on taken port $base left the screen and exited"
[ "$failures" = 0 ] || grep -aE 'mark:|input script|net_' "$log" | tail -12 >&2   # where the game got to (the work folder goes)
exit $((failures != 0))
