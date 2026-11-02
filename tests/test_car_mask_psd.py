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
print("test_car_mask_psd: %d failures" % failures)
sys.exit(1 if failures else 0)
