#!/usr/bin/env python3
"""The update notice end to end, on the real executable (an update test build: TNFS_RELEASE=ON, TNFS_UPDATE_TEST_VERSION=<old>; see CMakeLists.txt).
Runs `tnfs --update-check` (the real check: the system's curl, our API's answer, the version order) and reads its line.

usage: update_check_e2e.py live  EXE      against the live API (api.bytesnrust.com): the old stamped version must be told a newer release exists
       update_check_e2e.py local EXE      against servers of this script's own (TNFS_UPDATE_API_BASE, which only an update test build reads): the notice, and
                                          everything that must stay silent: up to date, {"latest":null}, 403 rate limit, 5xx, HTML, garbage, nothing
                                          listening (offline), a server that never answers (no hang), and a proxy (http_proxy)
Standard library only. Exit 0 when every case holds."""
import http.server
import os
import socket
import subprocess
import sys
import threading
import time

NEWER = "update check: newer version available: "
NONE = "update check: no newer version"
OLD = "0.0.1"
failures = []


def check(ok, what):
    if not ok:
        failures.append(what)
        print("FAIL: " + what, file=sys.stderr)


def run(exe, env_extra, timeout):
    env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_AUDIO_DRIVER="dummy")
    for k in ("http_proxy", "HTTP_PROXY", "https_proxy", "HTTPS_PROXY", "all_proxy", "ALL_PROXY", "TNFS_UPDATE_API_BASE"):
        env.pop(k, None)
    env.update(env_extra)
    start = time.time()
    with __import__("tempfile").TemporaryDirectory() as user:
        try:
            p = subprocess.run([exe, "--user", user, "--update-check"], env=env, capture_output=True, text=True, timeout=timeout)
        except subprocess.TimeoutExpired:
            return None, "", time.time() - start
    return p.returncode, p.stdout + p.stderr, time.time() - start


class Answer(http.server.BaseHTTPRequestHandler):
    status, body, ctype, stall = 200, b"", "application/json", 0
    seen = []

    def do_GET(self):
        Answer.seen.append(self.path)
        if Answer.stall:
            time.sleep(Answer.stall)
            return
        self.send_response(Answer.status)
        self.send_header("Content-Type", Answer.ctype)
        self.send_header("Content-Length", str(len(Answer.body)))
        self.end_headers()
        self.wfile.write(Answer.body)

    def do_POST(self):
        self.do_GET()

    def log_message(self, *args):
        pass


def serve():
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Answer)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server, server.server_address[1]


def answer(status, body, ctype="application/json", stall=0):
    Answer.status, Answer.body, Answer.ctype, Answer.stall, Answer.seen = status, body, ctype, stall, []


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def live(exe):
    line = ""
    for attempt in range(3):   # a network that hiccups once is not a broken notice
        rc, out, took = run(exe, {}, 40)
        line = out.strip().splitlines()[-1] if out.strip() else "(no output; status %s)" % rc
        if rc == 0 and line.startswith(NEWER):
            print("live: ok: " + line + " (%.1f s)" % took)
            return
        time.sleep(3)
    check(False, "live: the stamped %s was not told a newer release exists: %s" % (OLD, line))


def local(exe):
    server, port = serve()
    base = "http://127.0.0.1:%d/spirited-drive/v1" % port
    env = {"TNFS_UPDATE_API_BASE": base}

    answer(200, b'{"latest":"9.9.9","page":"https://example.invalid"}')
    rc, out, _ = run(exe, env, 30)
    check(rc == 0 and NEWER + "9.9.9" in out, "a newer latest gave no notice: %s" % out.strip())
    check(any("/latest?v=" + OLD + "&p=" in p for p in Answer.seen), "the request was not GET /latest?v=%s&p=...: %s" % (OLD, Answer.seen))

    for what, status, body, ctype in (
        ("an up-to-date latest", 200, b'{"latest":"0.0.1"}', "application/json"),
        ("an older latest", 200, b'{"latest":"0.0.0"}', "application/json"),
        ("{\"latest\":null} (503, before the first release)", 503, b'{"latest":null}', "application/json"),
        ("a 403 rate limit", 403, b'{"message":"API rate limit exceeded for 1.2.3.4","documentation_url":"https://docs.github.com"}', "application/json"),
        ("a 429", 429, b"Too Many Requests", "text/plain"),
        ("a 500", 500, b"<html>oops</html>", "text/html"),
        ("an HTML page with a 200", 200, b"<html><body>captive portal: log in</body></html>", "text/html"),
        ("garbage", 200, bytes(range(256)) * 4, "application/octet-stream"),
        ("a version that is not one", 200, b'{"latest":"nine"}', "application/json"),
        ("an empty answer", 200, b"", "application/json"),
    ):
        answer(status, body, ctype)
        rc, out, took = run(exe, env, 30)
        check(rc == 0 and NONE in out and NEWER not in out, "%s must stay silent, got: %s" % (what, out.strip()))

    # offline: nothing listens at the address; the check ends at once, silently
    rc, out, took = run(exe, {"TNFS_UPDATE_API_BASE": "http://127.0.0.1:%d/spirited-drive/v1" % free_port()}, 30)
    check(rc == 0 and NONE in out and took < 8, "nothing listening: no silent end in time (%.1f s): %s" % (took, out.strip()))

    # a server that takes the connection and never answers: curl's --max-time 5 ends it; the game never waits on it anyway
    answer(200, b"{}", stall=40)
    rc, out, took = run(exe, env, 30)
    check(rc == 0 and NONE in out and took < 12, "a server that never answers: not ended in time (%.1f s, status %s): %s" % (took, rc, out.strip()))
    server.shutdown()

    # a proxy: the address does not exist, only http_proxy reaches the answer
    proxy, pport = serve()
    answer(200, b'{"latest":"9.9.9"}')
    rc, out, _ = run(exe, {"TNFS_UPDATE_API_BASE": "http://updates.test.invalid/spirited-drive/v1", "http_proxy": "http://127.0.0.1:%d" % pport}, 30)
    check(rc == 0 and NEWER + "9.9.9" in out, "through a proxy (http_proxy) the notice did not come: %s" % out.strip())
    check(any("updates.test.invalid" in p for p in Answer.seen), "the proxy was not asked for the full address: %s" % Answer.seen)
    answer(200, b'{"latest":"9.9.9"}')
    rc, out, took = run(exe, {"TNFS_UPDATE_API_BASE": "http://updates.test.invalid/spirited-drive/v1", "http_proxy": "http://127.0.0.1:%d" % free_port()}, 30)
    check(rc == 0 and NONE in out and took < 8, "a dead proxy: no silent end in time (%.1f s): %s" % (took, out.strip()))
    proxy.shutdown()
    if not failures:
        print("local: ok")


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in ("live", "local") or not os.path.isfile(sys.argv[2]):
        print(__doc__, file=sys.stderr)
        return 2
    rc, out, _ = run(sys.argv[2], {}, 10)   # the build must be an update test one: it stamps +updatetest
    if "+updatetest" not in out:
        print("this is not an update test build (no +updatetest in its version): " + out.strip(), file=sys.stderr)
        return 2
    (live if sys.argv[1] == "live" else local)(sys.argv[2])
    return 1 if failures else 0


sys.exit(main())
