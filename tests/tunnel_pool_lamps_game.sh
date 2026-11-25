#!/bin/sh
# The headlights' pool, the tunnel's lamps and the mouth's gradient blend with no odd effect: Alpine's first tunnel (AL1 1596..1607) and AL3's 616..639, at both mouths, day and night, the lamps of the
# tunnel at 0, 1, 50 and 100 %, the chase view with the car at nodes -6..+6 around each mouth (a spread of them, the neighbouring ones in a row: TNFS_POOL_ALL=1 for every node). The picture's road in front of the car:
#  (1) never jumps over 40 % of its range between neighbouring nodes (the whole run of a mouth, lamps and time fixed);
#  (2) on the open road outside the mouth the pool never makes it lighter than the same node at lamps 100 % or than the farthest open node (+5 %);
#  (3) inside, at each node, the lamps 0 % -> 1 % step is no jump (within 10 % of the range) and 1 -> 50 -> 100 % no jump over 40 %.
# (1) at night is printed as NOTE: the night's own step at a mouth (AL1 nodes 0 -> 1, 42 % of the range even at lamps 100) is the gradient's, not the lamps' fade.
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
lamps_all="0 1 50 100"
mkdir -p "$work/shots"
echo "$cases" | while read -r track from to; do
  grep -q "^tunnel $from $to" data/track_light/$track.txt || { echo "FAIL: $track's tunnel $from $to is not in data/track_light"; touch "$work/bad"; exit 1; }
done
[ ! -e "$work/bad" ] || exit 1
for lamps in $lamps_all; do
  for time in day night; do
    blk=B; [ $time = night ] && blk=N
    echo "$cases" | while read -r track from to; do
      d="$work/mods_${track}_${time}_$lamps"
      mkdir -p "$d/track_light"
      awk -v b="$blk" -v f="$from" -v t="$to" -v s="$lamps" '{ print } $1 == "time" { on = ($2 == b) } on && /^  tunnel_look [0-9.]+ / && !done { print "  tunnel_look " f " " t " 60 lamps " s " reach 1"; done = 1 }' "data/track_light/$track.txt" > "$d/track_light/$track.txt"
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
  for time in day night; do
    for lamps in $lamps_all; do
      for mouth in in out; do
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
python3 -I - "$work" "$offsets" "$sheets" << 'PY'
import sys, os
w, offsets, sheets = sys.argv[1], [int(o) for o in sys.argv[2].split()], sys.argv[3]
cases = [("AL1", 1596, 1607), ("AL3", 616, 639)]
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
    for tm in ("day", "night"):
        for mouth in ("in", "out"):
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
    for tm in ("day", "night"):
        for mouth in ("in", "out"):
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
[ $status -eq 0 ] && echo "tunnel_pool_lamps_game: ok" && rm -rf "$work"
[ $status -ne 0 ] && echo "kept: $work"
exit $status
