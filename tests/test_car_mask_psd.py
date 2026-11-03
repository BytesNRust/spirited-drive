#!/usr/bin/env python3
"""The PSD mask tool's lamp colours (tools/car_mask_from_psd.py lamp_class): red brake 240, orange position 244, yellow headlight 248 and cyan
reversing light 252, the cyan decided before the blue (glass) and green (body) rules it ties with (b == g); the values are car_mask.h's.
Skipped (exit 0) when Pillow is not installed: the tool imports it."""
import importlib.util
import os
import re
import sys

here = os.path.dirname(os.path.abspath(__file__))
try:
    spec = importlib.util.spec_from_file_location("car_mask_from_psd", os.path.join(here, "..", "tools", "car_mask_from_psd.py"))
    tool = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(tool)
    spec = importlib.util.spec_from_file_location("car_mask_psd", os.path.join(here, "..", "tools", "car_mask_psd.py"))
    writer = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(writer)
except ImportError as error:
    print("test_car_mask_psd: skipped (%s)" % error)
    sys.exit(0)

header = open(os.path.join(here, "..", "game", "race", "car_mask.h")).read()
classes = dict((name, int(value)) for name, value in re.findall(r"CAR_MASK_(BRAKE|POSITION|HEADLIGHT|REVERSE) = (\d+)", header))
failures = 0


def check(name, got, want):
    global failures
    if got != want:
        failures += 1
        print("FAIL %s: got %r, want %r" % (name, got, want))


check("tool values are the header's", (tool.LAMP_BRAKE, tool.LAMP_POSITION, tool.LAMP_HEAD, tool.LAMP_REVERSE),
      (classes["BRAKE"], classes["POSITION"], classes["HEADLIGHT"], classes["REVERSE"]))
check("red", tool.lamp_class(255, 0, 0), 240)
check("orange", tool.lamp_class(255, 128, 0), 244)
check("yellow", tool.lamp_class(255, 255, 0), 248)
check("cyan 00ffff", tool.lamp_class(0, 255, 255), 252)
check("cyan, off a little", tool.lamp_class(20, 230, 240), 252)
check("pure blue stays glass", tool.lamp_class(0, 0, 255), 0)
check("pure green stays body", tool.lamp_class(0, 255, 0), 0)
check("a pale cyan is not the light", tool.lamp_class(150, 255, 255), 0)
check("a dull cyan is not the light", tool.lamp_class(0, 150, 150), 0)
check("white is not a lamp", tool.lamp_class(255, 255, 255), 0)

# the writer (tools/car_mask_psd.py): a class's colour is the one the reader decodes to the same class, and the lamps' greys keep their class
classes = dict((name, int(value)) for name, value in re.findall(r"CAR_MASK_(BRAKE|POSITION|HEADLIGHT|REVERSE) = (\d+)", header))
for value in (classes["BRAKE"], classes["POSITION"], classes["HEADLIGHT"], classes["REVERSE"]):
    check("writer colour of lamp %d is read back" % value, tool.lamp_class(*writer.CLASS_COLOUR[value]), value)
check("class_of: paint, shine, body, black", [writer.class_of(v) for v in (0, 100, 128, 192, 223, 224, 239)], [0, 128, 128, 192, 192, 224, 224])
check("class_of: the lamps' fours", [writer.class_of(v) for v in (240, 243, 244, 247, 248, 251, 252, 254, 255)], [240, 240, 244, 244, 248, 248, 252, 252, 255])
alpha, red, green, blue = writer.mask_planes(bytes([0, 128, 240, 248, 255]))
check("mask_planes alpha", alpha, bytes([0, 255, 255, 255, 255]))
check("mask_planes colours", list(zip(red, green, blue)), [(255, 255, 255), (0, 0, 255), (255, 0, 0), (255, 255, 0), (255, 255, 255)])
from PIL import Image
import tempfile
with tempfile.TemporaryDirectory() as tmp:
    rgb = Image.new("RGB", (4, 1), (10, 20, 30))
    keep = Image.frombytes("L", (4, 1), bytes([0, 128, 248, 255]))
    out = os.path.join(tmp, "t.psd")
    writer.write_psd(out, rgb, keep)
    psd = Image.open(out)
    check("psd: size and two layers", (psd.size, getattr(psd, "n_frames", 1)), ((4, 1), 2))
    shown = psd.convert("RGB").getpixel((2, 0))   # the texture under yellow at 50 %: about (132, 137, 15)
    check("psd: a headlight texel shows yellow over the texture", all(abs(a - b) <= 2 for a, b in zip(shown, (132, 137, 15))), True)
print("test_car_mask_psd: %d failures" % failures)
sys.exit(1 if failures else 0)
