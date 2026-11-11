#!/usr/bin/env python3
"""psx_light_synthetic.py OUT NODES full|dark|all|ramp|sun: a made-up Z<TRACK>B.LGT for checking the PS1 light without the CD's files.
all: the terrain of every node at 0x40. sun: the road at 0xD7 and the terrain at 0xC8 (a low sun's A / C). ramp: terrain of nodes 96..159 fading linearly from 255 to 40 (a tunnel mouth). full: every point 255 (the frames must equal the light off). dark: terrain points of the middle tenth of the nodes at 0x40.
Layout: u32 nodes, u32 scale, 11 bytes a node (0 middle, 1-5 right, 6-10 left), 12 byte trailer."""
import struct, sys

out, nodes, kind = sys.argv[1], int(sys.argv[2]), sys.argv[3]
roads = {0, 1, 6}
rows = bytearray()
for n in range(nodes):
    dark = kind == "all" or (kind == "dark" and nodes * 9 // 20 <= n < nodes * 11 // 20)
    ramp = 255 - round(215 * (n - 96) / 63) if kind == "ramp" and 96 <= n < 160 else (40 if kind == "ramp" and n >= 160 else 255)
    if kind == "sun":
        rows += bytes(0xD7 if p in roads else 0xC8 for p in range(11))
        continue
    rows += bytes(0x40 if dark and p not in roads else (ramp if p not in roads else 255) for p in range(11))
open(out, "wb").write(struct.pack("<II", nodes, 1) + rows + b"CRCF" + bytes(8))
