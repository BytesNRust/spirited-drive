#!/usr/bin/env python3
"""The generic road's lines with a darker rim: the outer rows of the yellow band take the colour a car's shadow turns yellow into (0.6 of it, as the race
darkens it, game/sys/palette.c). The race draws a shadow by mapping each pixel to the palette's nearest entry to that colour, and only the entries the road's
pictures use survive the race's own palette work: with this rim the stage's palette keeps a dark yellow, and the centre line stays yellow under a car instead of
going green (editor/art/README.md, tests/test_generic_art_shades.c).
Usage: road_rim.py ROAD_LINES.png OUT.png   (ROAD_LINES.png is make_road.py's: the yellow band is the bottom rows, 4.5 % of the picture)"""
import sys
from PIL import Image

LINE = 0.045                      # make_road.py: the yellow band's share of the picture, from the bottom
RIM = 0.002                       # the share of the picture, from the band's outer edge inwards, that takes the dark yellow: one row at 512 (in the smaller sizes it averages away)
SHADE = 0x9999                    # the race's HUD_SHADE_SCALE: 0.6

src = Image.open(sys.argv[1]).convert("RGB")
w, h = src.size
px = src.load()
yellow = px[w // 2, h - 1]
six = [c >> 2 for c in yellow]
dark = tuple((((c * SHADE) >> 16) + (((c * SHADE) >> 15) & 1)) * 4 for c in six)   # the race's shade_channel, back to 8 bits
bands = max(2, round(h * LINE))
rim = max(1, round(h * RIM))
for y in range(h - bands, h - bands + rim):
    for x in range(w):
        px[x, y] = dark
src.save(sys.argv[2])
