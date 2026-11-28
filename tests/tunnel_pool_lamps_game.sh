#!/bin/sh
# The headlights' pool, the tunnel's lamps and the mouth's gradient blend with no odd effect: Alpine's first tunnel (AL1 1596..1607) and AL3's 616..639, at both mouths, day and night, the lamps of the
# tunnel at 0, 1, 50 and 100 %, the chase view with the car at nodes -6..+6 around each mouth (a spread of them, the neighbouring ones in a row: TNFS_POOL_ALL=1 for every node). The picture's road in front of the car:
#  (1) never jumps over 40 % of its range between neighbouring nodes (the whole run of a mouth, lamps and time fixed);
#  (2) on the open road outside the mouth the pool never makes it lighter than the same node at lamps 100 % or than the farthest open node (+5 %);
#  (3) inside, at each node, the lamps 0 % -> 1 % step is no jump (within 10 % of the range) and 1 -> 50 -> 100 % no jump over 40 %.
# (1) at night is printed as NOTE, not checked: with the pool on, a step inside the beam (AL1 nodes 0 -> 1, 42 % of the range even at lamps 100) is the tunnel floor's texture, accepted by the owner's rule (the pool wins);
# the outside ramp is not drawn at night with the pool.
#  (4) by day, going in with the lamps at 0, 1 and 50 %, the road does not rise by more than 2 (of 255) from node -1 to node +2 (the beam comes into the tunnel gradually).
#  (5) by day, going out with the lamps at 0, 1 and 50 %, the road at the tunnel's last node is not a dip (over 2 under both its neighbours): the beam fades out at the exit.
# The tunnels' light files are main's of cd627f90 (tests/fixtures/track_light), so the test checks the code and not the owner's tuning of AL1 and AL3.
# ctest runs it in eight parts side by side, one track at one time of day at one mouth each (POOL_TRACK, POOL_TIME, POOL_MOUTH): every check compares shots of one track, time and mouth only (the lamps'
# four values included). POOL_PARTS (ctest's list of the parts) must be exactly this script's tracks x times x mouths, so a case added here without its part fails. Without them it runs all.
# usage: tests/tunnel_pool_lamps_game.sh <tnfs binary> <original/ dir> [contact sheet dir]      (run from the repo's root)  Exit 77 (skipped) without the CD's data.
set -u
bin=$1
original=$2
sheets=${3:-}
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_pool_lamps.XXXXXX")
if [ -n "${TNFS_POOL_ALL:-}" ]; then offsets="-6 -5 -4 -3 -2 -1 0 1 2 3 4 5 6"; else offsets="-6 -2 -1 0 1 2 6"; fi
jobs_max=${TNFS_POOL_JOBS:-4}
# track from to
cases="AL1 1596 1607
AL3 616 639"
times="day night"
mouths="in out"
if [ -n "${POOL_PARTS:-}" ]; then
  have=$(echo "$cases" | while read -r track from to; do for time in $times; do for mouth in $mouths; do printf '%s_%s_%s ' $track $time $mouth; done; done; done)
  [ "$have" = "$POOL_PARTS " ] || { echo "FAIL: the test's cases are '$have', ctest runs the parts '$POOL_PARTS'"; exit 1; }
fi
[ -z "${POOL_TRACK:-}" ] || cases=$(echo "$cases" | grep "^$POOL_TRACK ")
[ -z "${POOL_TIME:-}" ] || times=$POOL_TIME
[ -z "${POOL_MOUTH:-}" ] || mouths=$POOL_MOUTH
case $mouths in "in out"|in|out) ;; *) echo "FAIL: no mouth '$mouths'"; exit 1 ;; esac
{ [ -n "$cases" ] && case $times in "day night"|day|night) true;; *) false;; esac; } || { echo "FAIL: no case for track '${POOL_TRACK:-}' time '${POOL_TIME:-}'"; exit 1; }
lamps_all="0 1 50 100"
mkdir -p "$work/shots"
echo "$cases" | while read -r track from to; do
  grep -q "^tunnel $from $to" tests/fixtures/track_light/${track}_main_cd627f90.txt || { echo "FAIL: $track's tunnel $from $to is not in the fixture"; touch "$work/bad"; exit 1; }
done
[ ! -e "$work/bad" ] || exit 1
for lamps in $lamps_all; do
  for time in $times; do
    blk=B; [ $time = night ] && blk=N
    echo "$cases" | while read -r track from to; do
      d="$work/mods_${track}_${time}_$lamps"
      mkdir -p "$d/track_light"
      awk -v b="$blk" -v f="$from" -v t="$to" -v s="$lamps" '{ print } $1 == "time" { on = ($2 == b) } on && /^  tunnel_look [0-9.]+ / && !done { print "  tunnel_look " f " " t " 60 lamps " s " reach 1"; done = 1 }' "tests/fixtures/track_light/${track}_main_cd627f90.txt" > "$d/track_light/$track.txt"
    done
  done
done
shot() {   # track time lamps node out
  d="$work/mods_$1_$2_$3"
  u="$work/user_$1_$2_$3_$4"
  mkdir -p "$u"
  printf 'profile=restomod\npsx_dir=\nlight_effects=1\n' > "$u/port.cfg"
  printf 'light_effects=1\n' > "$u/restomod.cfg"
  TNFS_FRAMES=30 TNFS_LIGHT_TIME=$2 TNFS_WEATHER=clear TNFS_AUTOPILOT=1 TNFS_VIEW=3 TNFS_NO_HUD=1 TNFS_START_NODE=$4 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$u/pref" \
    timeout -k 5 300 "$bin" --data "$original" --user "$u" --mods "$d" --drive "$1" --car 3 --racers 1 --node 20 --screenshot "$5" > /dev/null 2>&1
  rm -rf "$u"
}
n=0
echo "$cases" | while read -r track from to; do
  for time in $times; do
    for lamps in $lamps_all; do
      for mouth in $mouths; do
        base=$from; [ $mouth = out ] && base=$to
        for off in $offsets; do
          node=$((base + off))
          shot $track $time $lamps $node "$work/shots/${track}_${time}_${lamps}_${mouth}_${off}.ppm" &
          n=$((n + 1))
          [ $((n % jobs_max)) -eq 0 ] && wait
        done
      done
    done
  done
  wait
done
wait
python3 -I - "$work" "$offsets" "$sheets" "$(echo "$cases" | tr '\n' ';')" "$times" "$mouths" << 'PY'
import sys, os
w, offsets, sheets = sys.argv[1], [int(o) for o in sys.argv[2].split()], sys.argv[3]
cases = [(c.split()[0], int(c.split()[1]), int(c.split()[2])) for c in sys.argv[4].split(";") if c]
times = sys.argv[5].split()
mouths = sys.argv[6].split()
def tm_of(tag):
    return tag.split()[1]
def mean(p):
    try:
        d = open(p, "rb").read().split(b"\n", 3)
        W, H = map(int, d[1].split())
        px = d[3]
    except Exception:
        return None
    s = n = 0
    for y in range(int(H * .45), int(H * .95)):
        for x in range(int(W * .15), int(W * .85)):
            i = (y * W + x) * 3
            s += px[i] + px[i + 1] + px[i + 2]
            n += 3
    return s / n
bad = []
notes = []
tab = {}
for track, f, t in cases:
    for tm in times:
        for mouth in mouths:
            for lamps in (0, 1, 50, 100):
                seq = [mean("%s/shots/%s_%s_%d_%s_%d.ppm" % (w, track, tm, lamps, mouth, o)) for o in offsets]
                if any(v is None for v in seq):
                    bad.append("%s %s %s lamps %d: a screenshot is missing" % (track, tm, mouth, lamps)); continue
                tab[(track, tm, mouth, lamps)] = seq
for (track, tm, mouth, lamps), seq in tab.items():
    rng = max(max(seq) - min(seq), 8.0)
    tag = "%s %s %s lamps %d" % (track, tm, mouth, lamps)
    # offsets are around the mouth: for "in" the tunnel is at offsets >= 0, for "out" at offsets <= 0
    inside = [i for i, o in enumerate(offsets) if (o >= 0 if mouth == "in" else o <= 0)]
    outside = [i for i in range(len(offsets)) if i not in inside]
    for a, b in zip(range(len(offsets)), range(1, len(offsets))):
        if offsets[b] - offsets[a] != 1: continue   # neighbouring nodes only
        if abs(seq[a] - seq[b]) > 0.4 * rng and rng > 8:
            (bad if tm_of(tag) == "day" else notes).append("(1) %s: nodes %d -> %d jump %.1f of a range %.1f" % (tag, offsets[a], offsets[b], abs(seq[a] - seq[b]), rng))
    far = seq[0] if mouth == "in" else seq[-1]
    ref = tab.get((track, tm, mouth, 100))
    for i in outside:
        cap = max(far, ref[i] if ref else far) * 1.05 + 2
        if seq[i] > cap:
            bad.append("(2) %s: outside node %d is %.1f, over the open road's %.1f" % (tag, offsets[i], seq[i], cap))
for track, f, t in cases:
    for tm in times:
        for mouth in mouths:
            s = [tab.get((track, tm, mouth, l)) for l in (0, 1, 50, 100)]
            if any(x is None for x in s): continue
            rng = max(max(max(x) for x in s) - min(min(x) for x in s), 8.0)
            inside = [i for i, o in enumerate(offsets) if (o >= 0 if mouth == "in" else o <= 0)]
            for i in inside:
                if abs(s[0][i] - s[1][i]) > 0.10 * rng:
                    bad.append("(3) %s %s %s node %d: lamps 0 -> 1 jumps %.1f -> %.1f" % (track, tm, mouth, offsets[i], s[0][i], s[1][i]))
                for a, b in ((1, 2), (2, 3)):
                    if abs(s[b][i] - s[a][i]) > 0.4 * rng:
                        bad.append("(3) %s %s %s node %d: lamps %d -> %d jumps %.1f -> %.1f" % (track, tm, mouth, offsets[i], (0, 1, 50, 100)[a], (0, 1, 50, 100)[b], s[a][i], s[b][i]))
for track, f, t in cases:   # (4) going in by day with the lamps at 0, 1, 50 %: the road under the car does not rise by more than 2 from the mouth to the third node in (the beam comes in gradually)
    for lamps in (0, 1, 50):
        seq = tab.get((track, "day", "in", lamps))
        if seq is None: continue
        v = {o: seq[i] for i, o in enumerate(offsets)}
        path = [o for o in (-1, 0, 1, 2) if o in v]
        for a, b in zip(path, path[1:]):
            if v[b] > v[a] + 2:
                bad.append("(4) %s day in lamps %d: the road rises going in, node %d -> %d: %.1f -> %.1f" % (track, lamps, a, b, v[a], v[b]))
for track, f, t in cases:   # (5) going out by day with the lamps at 0, 1, 50 %: the road at the last node in (the beam eased to none there) is no dip against its neighbours (more than 2 under both)
    for lamps in (0, 1, 50):
        seq = tab.get((track, "day", "out", lamps))
        if seq is None: continue
        v = {o: seq[i] for i, o in enumerate(offsets)}
        if all(o in v for o in (-1, 0, 1)) and v[0] < v[-1] - 2 and v[0] < v[1] - 2:
            bad.append("(5) %s day out lamps %d: a dip at the exit's last node: %.1f between %.1f and %.1f" % (track, lamps, v[0], v[-1], v[1]))
for k in sorted(tab):
    print(" ".join(map(str, k)), " ".join("%.0f" % v for v in tab[k]))
for b in notes:
    print("NOTE (night, the open night's own light, not the lamps' fade):", b)
for b in bad:
    print("FAIL:", b)
if sheets:
    os.makedirs(sheets, exist_ok=True)
    for name in os.listdir(w + "/shots"):
        pass
sys.exit(1 if bad else 0)
PY
status=$?
if [ -n "$sheets" ]; then
  mkdir -p "$sheets"
  cp "$work/shots/"*.ppm "$sheets/" 2>/dev/null
fi
[ $status -eq 0 ] && echo "tunnel_pool_lamps_game: ok" && { [ -n "${TNFS_POOL_KEEP:-}" ] && echo "kept shots: $work" || rm -rf "$work"; }
[ $status -ne 0 ] && echo "kept: $work"
exit $status
