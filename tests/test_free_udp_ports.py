"""tools/free_udp_ports.py: a block of free UDP ports for a link test, each run its own (two ctests on one machine must not share a
host's port). Plain stdlib; run with python3 -I."""
import os, socket, subprocess, sys, tempfile, time

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = os.path.join(HERE, "..", "tools", "free_udp_ports.py")
sys.path.insert(0, os.path.join(HERE, "..", "tools"))
import free_udp_ports as F

failures = 0


def check(ok, what):
    global failures
    if not ok:
        failures += 1
        print("FAIL: " + what, file=sys.stderr)


def hold(port):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("", port))
    return s


def free_base(count):  # a base where `count` ports are free right now, for the tests to build on
    for base in range(41000, 41800, 20):
        if F.block_free(base, count):
            return base
    raise SystemExit("no free block to test on")


base = free_base(2)
check(F.block_free(base, 2), "a free block is seen as free")
with tempfile.TemporaryDirectory() as locks:
    # a port taken by another socket: the host's port, then the client's (inside the block)
    for taken in (base, base + 1):
        s = hold(taken)
        got = F.pick(base, 2, step=20, tries=5, lock_dir=None)
        check(got is not None and got != base, "a block with port %d taken is skipped (got %s)" % (taken, got))
        check(got is None or F.block_free(got, 2), "the block picked is free")
        s.close()
    check(F.pick(base, 2, step=20, tries=5, lock_dir=None) == base, "the first free block is the wanted one")

    # two runs asking at once: the second one is told another block while the first holds its lock
    first = F.pick(base, 2, step=20, tries=5, lock_dir=locks)
    second = F.pick(base, 2, step=20, tries=5, lock_dir=locks)
    check(first == base, "the first run gets the wanted block")
    check(second is not None and second != first, "the second run gets another block while the first one's lock stands (got %s)" % second)
    F.release(locks, first)
    check(F.pick(base, 2, step=20, tries=5, lock_dir=locks) == first, "a released block is free for the next run")
    F.release(locks, first)

    # a lock the run that made it never released (killed): taken over once it is older than the limit
    os.mkdir(os.path.join(locks, str(base)))
    old = time.time() - 7200
    os.utime(os.path.join(locks, str(base)), (old, old))
    check(F.pick(base, 2, step=20, tries=5, lock_dir=locks) == base, "an old lock is taken over")
    F.release(locks, base)

    # nothing free in the tries asked for: None, and the CLI says so with status 1
    s = hold(base)
    check(F.pick(base, 2, step=20, tries=1, lock_dir=None) is None, "no block in the tries: None")
    r = subprocess.run([sys.executable, "-I", TOOL, str(base), "2", "--step", "20", "--tries", "1"], capture_output=True, text=True)
    check(r.returncode == 1 and r.stdout == "", "the CLI exits 1 and prints nothing when nothing is free (rc %d)" % r.returncode)
    s.close()
    r = subprocess.run([sys.executable, "-I", TOOL, str(base), "2", "--lock-dir", locks], capture_output=True, text=True)
    check(r.returncode == 0 and r.stdout.strip() == str(base), "the CLI prints the block's first port (%r)" % r.stdout)
    check(os.path.isdir(os.path.join(locks, str(base))), "the CLI leaves its lock for the caller to release")

sys.exit(1 if failures else 0)
