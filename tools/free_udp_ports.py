#!/usr/bin/env python3
"""A block of consecutive free UDP ports for a link test: tools/free_udp_ports.py START COUNT [--step N] [--tries N] [--lock-dir DIR]

Prints the first port of the first block (START, START+STEP, ...) in which every port can be bound, and exits 0; prints nothing and
exits 1 when none of the TRIES blocks is free. The probe binds on all addresses with no reuse flag, as the game's net_udp_open does,
so a port the game could not take is not picked.
With --lock-dir the block is also claimed by making DIR/<first port> (mkdir is atomic): two runs asking at the same moment, from
different worktrees, cannot both be told the same block before either has bound it. The caller removes the claim when its run
ends (rmdir); one older than LOCK_AGE_LIMIT seconds was left by a run that was killed and is taken over.
Python standard library only."""
import argparse, os, socket, sys, time

LOCK_AGE_LIMIT = 3700  # a link run is limited to OVERALL (3600 s) in tools/link_loopback.sh


def port_free(port):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.bind(("", port))
        return True
    except OSError:
        return False
    finally:
        s.close()


def block_free(base, count):
    return base + count <= 65536 and all(port_free(base + i) for i in range(count))


def claim(lock_dir, base):
    path = os.path.join(lock_dir, str(base))
    try:
        os.makedirs(lock_dir, exist_ok=True)
        os.mkdir(path)
        return True
    except FileExistsError:
        try:
            if time.time() - os.stat(path).st_mtime > LOCK_AGE_LIMIT:
                os.rmdir(path)
                os.mkdir(path)
                return True
        except OSError:
            pass
        return False
    except OSError:
        return True  # no place for locks: the bind probe alone


def release(lock_dir, base):
    try:
        os.rmdir(os.path.join(lock_dir, str(base)))
    except OSError:
        pass


def pick(start, count, step=20, tries=40, lock_dir=None):
    for k in range(tries):
        base = start + k * step
        if base + count > 65536:
            break
        if lock_dir is not None and not claim(lock_dir, base):
            continue
        if block_free(base, count):
            return base
        if lock_dir is not None:
            release(lock_dir, base)
    return None


def main(argv):
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("start", type=int)
    p.add_argument("count", type=int)
    p.add_argument("--step", type=int, default=20)
    p.add_argument("--tries", type=int, default=40)
    p.add_argument("--lock-dir")
    a = p.parse_args(argv)
    base = pick(a.start, a.count, a.step, a.tries, a.lock_dir)
    if base is None:
        print("free_udp_ports: no block of %d free ports from %d (%d tries of %d)" % (a.count, a.start, a.tries, a.step), file=sys.stderr)
        return 1
    print(base)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
