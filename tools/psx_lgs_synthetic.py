#!/usr/bin/env python3
"""psx_lgs_synthetic.py OUT R G B: a made-up Z<TRACK><A|B|C>.LGS in the real files' shape (CR line ends, comments, a CRCF trailer)
whose ambient colour is R,G,B (128 neutral). Nothing of the CD's files."""
import sys

out, r, g, b = sys.argv[1], *map(int, sys.argv[2:5])
text = ("/* Depth ceuing definition X0,Y0, X1,Y1, X2,Y2, X3,Y3 */\r0,95  10,99  75,37  100,28\r"
        "/* Dynamic channel system (start rgb) */\r%d,%d,%d\r/* Dynamic channel system (end rgb) */\r%d,%d,%d\r"
        "/* Number of light context */\r1\r/* context 1 */\r14,14  64  111,111,111  0,0,0\r" % (r, g, b, r, g, b))
open(out, "wb").write(text.encode() + b"CRCF" + bytes(8))
