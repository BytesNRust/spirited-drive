#!/usr/bin/env python3
"""Synthetic tracks for oracle runs that real tracks do not cover.

    synth_tracks.py SIMDATA_MISC OUT_DIR

TRPOL2.TRI (from TR1) and CYPOL.TRI (from CY1): about one node in six gets
the police spawn bit (node +0x1a bit 0) with random bits 1..4 (the far side,
the stay bit, the four odds types; real tracks only use types 1 and 2), and
the closing node (node_count; node 0 on a circuit) is a spawn node too.
TRPOL2 also gets random lane nibbles (node +4, 0..3 each) so node 0's lanes
differ from the spawn node's. Used by 0046b950 (oracle_car `copplace`)."""
import random
import struct
import sys
from pathlib import Path

NODES, NODE_SIZE = 0x98C, 36


def node_count(d):
    return struct.unpack_from("<h", d, 6)[0] * 4


def spawn_bits(d, seed):
    r = random.Random(seed)
    for i in range(node_count(d) + 1):
        if r.random() < 1 / 6:
            o = NODES + NODE_SIZE * i + 0x1A
            v = struct.unpack_from("<H", d, o)[0]
            struct.pack_into("<H", d, o, (v & ~0x1F) | r.getrandbits(5) | 1)


def lane_nibbles(d, seed):
    r = random.Random(seed)
    for i in range(node_count(d) + 1):
        d[NODES + NODE_SIZE * i + 4] = r.randint(0, 3) << 4 | r.randint(0, 3)


def main():
    src, out = Path(sys.argv[1]), Path(sys.argv[2])
    tr = bytearray((src / "TR1.TRI").read_bytes())
    spawn_bits(tr, 11)
    lane_nibbles(tr, 12)
    struct.pack_into("<H", tr, NODES + 0x1A, 0x09)  # node 0, where the circuit's closing node wraps
    (out / "TRPOL2.TRI").write_bytes(tr)
    cy = bytearray((src / "CY1.TRI").read_bytes())
    spawn_bits(cy, 11)
    struct.pack_into("<H", cy, NODES + NODE_SIZE * node_count(cy) + 0x1A, 0x01)  # the stage's closing node
    (out / "CYPOL.TRI").write_bytes(cy)


if __name__ == "__main__":
    main()
