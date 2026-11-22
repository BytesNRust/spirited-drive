#!/usr/bin/env python3
"""Converts the old light files of the tracks (data/track_light/<TRACK><A|B|C|N>.txt and data/track/<TRACK>.txt) into the new one file a track (data/track_light/<TRACK>.txt, game/race/track_light_track.h).

usage: track_light_convert.py <data dir> <out dir> [TRACK ...]

The stored `rows` are not written: the game makes the same rows when the track loads (checked: TNFS_LIGHT_ROWS_DIFF finds 0 differing nodes of the 64 files, and tests/track_light_fingerprint.sh
must give the same hashes with the new files). A file that has other lines than the ones below stops the conversion: a human decides what they become.
  places: track, present, tunnel A B -> fact `tunnel A B`; snow, snow_map -> the same facts; canyon / car_dim -> dropped (the car reads the rows by lane)
  light:  tunnel A B lit -> fact `tunnel A B lit` (the same at every time); tunnel A B [LIGHT] [COLOUR] -> `tunnel_look A B LIGHT [COLOUR]` (and the fact `tunnel A B` when the places have none there);
          tunnel_tone LIGHT [COLOUR] -> `tunnel_look LIGHT [COLOUR]`; lamp ... -> as it is
"""
import os
import re
import sys

TIMES = "ABCN"
TRACKS = "AL1 AL2 AL3 CL1 CL2 CL3 CY1 CY2 CY3 TR1 TR2 TR3 TR4 TR5 TR6 TR7".split()


def lines(path):
    with open(path) as f:
        for n, raw in enumerate(f, 1):
            text = raw.split("#", 1)[0].strip()
            if text:
                yield n, text.split()


# TR3's forest (the code's terrain_light_forest, nodes 852..934): two dark tones in bands of 8 nodes, the first the darker, eased in over the first and last 3 nodes. A track file says it as
# `brightness` lines; the percent is whole, so a point can come out one level apart from the code's rounding (the fingerprint of TR3 changes on purpose).
FOREST = {"TR3": (852, 934, 8, 3, (0.40, 0.62))}


def forest_lines(track):
    if track not in FOREST:
        return []
    a, b, period, ramp, tones = FOREST[track]
    runs = []
    for n in range(a, b + 1):
        tone = tones[((n - a) // period) % 2]
        edge = min(n - a, b - n)
        ease = 1.0 if edge >= ramp else (edge + 1) / (ramp + 1)
        percent = int((1.0 - ease * (1.0 - tone)) * 100 + 0.5)
        if runs and runs[-1][2] == percent and runs[-1][1] == n - 1:
            runs[-1][1] = n
        else:
            runs.append([n, n, percent])
    return ["  brightness %d %d %d" % tuple(r) for r in runs]


def convert(data, track):
    facts_tunnel, facts_hall, facts_other = [], None, []
    covered = set()
    ppath = os.path.join(data, "track", track + ".txt")
    snow = None
    snow_maps = []
    for n, w in lines(ppath):
        if w[0] == "track":
            assert w[1] == track, (ppath, n)
        elif w[0] == "present" or w[0] in ("canyon", "car_dim"):
            continue
        elif w[0] == "tunnel":
            a, b = int(w[1]), int(w[2])
            facts_tunnel.append((a, b))
            covered.update(range(a, b + 1))
        elif w[0] == "snow":
            snow = w[1]
        elif w[0] == "snow_map":
            snow_maps.append((w[1], w[2]))
        else:
            sys.exit("%s:%d: a line of the places this converter does not know: %s" % (ppath, n, " ".join(w)))
    blocks = {}
    halls = {}
    for t in TIMES:
        path = os.path.join(data, "track_light", track + t + ".txt")
        look, zones, lamps = None, [], []
        hall = []
        for n, w in lines(path):
            word = w[0]
            if word in ("tnfs", "track", "rows"):
                continue
            if word == "tunnel_tone":
                look = " ".join(w[1:])
            elif word == "tunnel":
                a, b = int(w[1]), int(w[2])
                if w[3:] == ["lit"]:
                    hall.append((a, b))
                else:
                    zones.append((a, b, " ".join(w[3:])))
            elif word == "lamp":
                lamps.append(" ".join(w))
            else:
                sys.exit("%s:%d: a line of the light this converter does not know: %s" % (path, n, " ".join(w)))
        halls[t] = hall
        blocks[t] = (look, zones, lamps)
    for t in TIMES[1:]:
        assert halls[t] == halls["A"], "%s: the halls differ at %s" % (track, t)
    extra = []
    for t in TIMES:
        for a, b, _ in blocks[t][1]:
            if not all(i in covered for i in range(a, b + 1)):
                extra.append((a, b))
                covered.update(range(a, b + 1))
    out = ["# tnfs track light 2", "track " + track]
    for a, b in sorted(set(facts_tunnel + extra)):
        out.append("tunnel %d %d" % (a, b))
    for a, b in halls["A"]:
        out.append("tunnel %d %d lit" % (a, b))
    if snow is not None:
        out.append("snow " + snow)
    for a, b in snow_maps:
        out.append("snow_map %s %s" % (a, b))
    for t in TIMES:
        look, zones, lamps = blocks[t]
        forest = forest_lines(track)
        if look is None and not zones and not lamps and not forest:
            continue
        out.append("")
        out.append("time " + t)
        if look is not None:
            out.append("  tunnel_look " + look)
        for a, b, rest in zones:
            out.append(("  tunnel_look %d %d %s" % (a, b, rest)).rstrip())
        for lamp in lamps:
            out.append("  " + lamp)
        out.extend(forest)
    return "\n".join(out) + "\n"


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    data, dest = sys.argv[1], sys.argv[2]
    tracks = sys.argv[3:] or TRACKS
    os.makedirs(dest, exist_ok=True)
    for track in tracks:
        with open(os.path.join(dest, track + ".txt"), "w") as f:
            f.write(convert(data, track))
        print("wrote %s/%s.txt" % (dest, track))


main()
