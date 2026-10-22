#!/usr/bin/env python3
"""Whether a replay plays back the race it recorded, from TNFS_TRACE_PLAYER=N logs (slot 0's car every N ticks).

    replay_sync.py race RACE.log REPLAY.log
        a --drive race (its "trace 0" lines) against its Replay.rpl played back (--replay, the intro's "trace 1"
        lines): every tick both logged must hold the same position
    replay_sync.py marks REPLAY.log
        a replay recorded by the original (the CD's REPLAY*.RPL; N = 2): each minute mark (tick 1802, 3602, ...)
        puts the cars back to the original's snapshot, so the jump there (the position against the two ticks
        before it, carried on) is how far the playback strayed from the original's race in that minute; 0.0 when
        it drove the same race
"""
import re
import sys

LINE = re.compile(r'trace (\d+) (\d+): node (-?\d+) x (-?\d+) y (-?\d+) z (-?\d+)')
UNIT = 65536.0
MARK_FIRST, MARK_EVERY, JUMP_LIMIT = 1802, 1800, 1.0


def positions(path, replay):
    """tick -> (x, z), the first time the log has the tick, for the race (0) or the replay (1)"""
    out = {}
    with open(path, errors='replace') as f:
        for m in filter(None, map(LINE.search, f)):
            if int(m.group(1)) == replay:
                out.setdefault(int(m.group(2)), (int(m.group(4)), int(m.group(6))))
    return out


def race(race_log, replay_log):
    a, b = positions(race_log, 0), positions(replay_log, 1)
    both = sorted(set(a) & set(b))
    if not both:
        print("no tick in both logs")
        return 2
    off = [(t, (abs(a[t][0] - b[t][0]) + abs(a[t][1] - b[t][1])) / UNIT) for t in both]
    bad = [o for o in off if o[1] > 0]
    print("%d ticks (%d..%d), worst %.2f m" % (len(both), both[0], both[-1], max(d for _, d in off)))
    print("MATCH" if not bad else "DESYNC from tick %d (%.2f m)" % bad[0])
    return 1 if bad else 0


def marks(replay_log):
    p = positions(replay_log, 1)
    step = 2
    jumps = []
    for t in range(MARK_FIRST, max(p) + 1 if p else 0, MARK_EVERY):
        a, b, c = p.get(t - step), p.get(t - 2 * step), p.get(t)
        if a and b and c:
            jumps.append((t, (abs(c[0] - 2 * a[0] + b[0]) + abs(c[1] - 2 * a[1] + b[1])) / UNIT))
    print(" ".join("%d:%.1f" % j for j in jumps) or "no mark logged")
    bad = [j for j in jumps if j[1] > JUMP_LIMIT]
    print("MATCH" if jumps and not bad else "DESYNC" if bad else "UNKNOWN")
    return 1 if bad else 0


if __name__ == '__main__':
    if len(sys.argv) == 4 and sys.argv[1] == 'race':
        sys.exit(race(sys.argv[2], sys.argv[3]))
    if len(sys.argv) == 3 and sys.argv[1] == 'marks':
        sys.exit(marks(sys.argv[2]))
    print(__doc__)
    sys.exit(2)
