#!/usr/bin/env python3
# tools/ppm2png.py IN OUT: a captured frame (PPM) saved as a PNG, python standard library only (tools/rgb_image.py), for tools/menu_walk.sh when ImageMagick is not on the PATH.
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rgb_image import Image

if len(sys.argv) != 3:
    sys.exit("usage: ppm2png.py IN OUT.png")
Image.open(sys.argv[1]).save_png(sys.argv[2])
