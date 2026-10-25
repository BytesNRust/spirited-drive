#!/usr/bin/env python3
"""psx_light_synthetic.py OUT NODES full|dark|all: a made-up Z<TRACK>B.LGT for checking the PS1 light without the CD's files.
all: the terrain of every node at 0x40. full: every point 255 (the frames must equal the light off). dark: terrain points of the middle tenth of the nodes at 0x40.
Layout (re/notes/psx-lgt-format.md): u32 nodes, u32 scale, 11 bytes a node (0 middle, 1-5 right, 6-10 left), 12 byte trailer."""
import struct, sys

out, nodes, kind = sys.argv[1], int(sys.argv[2]), sys.argv[3]
roads = {0, 1, 6}
rows = bytearray()
for n in range(nodes):
    dark = kind == "all" or (kind == "dark" and nodes * 9 // 20 <= n < nodes * 11 // 20)
    rows += bytes(0x40 if dark and p not in roads else 255 for p in range(11))
open(out, "wb").write(struct.pack("<II", nodes, 1) + rows + b"CRCF" + bytes(8))
