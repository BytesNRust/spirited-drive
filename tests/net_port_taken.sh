#!/usr/bin/env bash
# The network screen when its UDP port is already taken: the host's game starts, opens its games box, the network service does not
# start ("net_udp_open: port N: Address already in use", "BUG: init network service FAILED") and the screen is left with a line in
# the log. It read a NULL there and crashed (SIGSEGV, status 139). The port taken is a free one found by tools/free_udp_ports.py
# and claimed, as the link tests' own are, so other ctests of the machine do not meet it.
# usage: tests/net_port_taken.sh BUILD_DIR   (the folder holding tnfs; run from anywhere)
set -u
build=${1:?usage: net_port_taken.sh BUILD_DIR}
repo=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
locks=${TNFS_LINK_LOCKS:-${TMPDIR:-/tmp}/tnfs_link_ports}
holder="" base=""
cleanup() {
    [ -n "$holder" ] && kill "$holder" 2>/dev/null
    [ -n "$base" ] && python3 -I -c 'import sys; sys.path.insert(0, sys.argv[1]); import free_udp_ports as F; F.release(sys.argv[2], int(sys.argv[3]))' "$repo/tools" "$locks" "$base"
    find "$work" -delete 2>/dev/null
}
trap cleanup EXIT
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }

[ -f "$repo/original/SIMDATA/MISC/AL1.TRI" ] || { echo "skip: no original/"; exit 0; }
base=$(python3 -I "$repo/tools/free_udp_ports.py" 16100 1 --lock-dir "$locks") || { echo "FAIL: no free port" >&2; exit 1; }
python3 -I -c 'import socket, sys, time
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.bind(("", int(sys.argv[1]))); print("held", flush=True); time.sleep(600)' "$base" >"$work/holder" &
holder=$!
disown "$holder" 2>/dev/null   # its kill at the end is no news
for _ in $(seq 1 50); do grep -q held "$work/holder" 2>/dev/null && break; sleep 0.1; done
grep -q held "$work/holder" || { echo "FAIL: the port $base was not taken" >&2; exit 1; }

# a host on the taken port: its game cannot be hosted, so link_loopback would sit out STEP_TIMEOUT waiting for it. The run is stopped as soon as the host has said there is no
# network service and then opened a screen (it left, and is alive); a host that never does ends the run by STEP_TIMEOUT as before
log=$work/out/host/log
BASE_PORT_FIXED=1 BASE_PORT=$base STEP_TIMEOUT=60 bash "$repo/tools/link_loopback.sh" "$build" refused "$work/out" >"$work/run.log" 2>&1 &
run=$!
left=0 rc=""
while kill -0 "$run" 2>/dev/null; do
    if awk '/net_host: no network service/ { seen = 1; next } seen && /^mark:screen=/ { found = 1 } END { exit !found }' "$log" 2>/dev/null; then left=1; break; fi
    sleep 0.2
done
rc=$(cat "$work/out/host/rc" 2>/dev/null)   # read before the run is stopped: a status here is the host's own end
kill -TERM "$run" 2>/dev/null
wait "$run" 2>/dev/null
[ -f "$log" ] || { fail "no host log: $(tail -3 "$work/run.log")"; exit 1; }
grep -q "Address already in use" "$log" || fail "the host did not meet the taken port: $(tail -3 "$log")"
grep -q "net_host: no network service" "$log" || fail "the games box did not say there is no network service: $(tail -3 "$log")"
[ "$left" = 1 ] || fail "no screen opened after the games box was refused: $(tail -3 "$log")"
grep -qaE "AddressSanitizer|runtime error:|SEGV" "$log" && fail "the host crashed on the taken port"
case $rc in 139 | 134 | 138) fail "the host died of a signal (status $rc) on the taken port" ;; esac
[ "$failures" = 0 ] && echo "ok: the host on taken port $base left the screen (status ${rc:-still running})"
exit $((failures != 0))
