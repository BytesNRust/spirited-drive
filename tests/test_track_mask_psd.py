#!/usr/bin/env python3
"""The track mask tools (tools/track_mask_psd.py, tools/track_mask_from_psd.py): a painted texel is reflective when it is opaque and not black, the PSD the writer makes is a
texture under an empty mask layer, and the files it reads carry the names track_mask.h looks up. Skipped (exit 0) when Pillow is not installed."""
import importlib.util
import os
import sys
import tempfile

here = os.path.dirname(os.path.abspath(__file__))
try:
    def load(name):
        spec = importlib.util.spec_from_file_location(name, os.path.join(here, "..", "tools", name + ".py"))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module
    reader = load("track_mask_from_psd")
    writer = load("track_mask_psd")
    from PIL import Image
except ImportError as error:
    print("test_track_mask_psd: skipped (%s)" % error)
    sys.exit(0)

failures = 0


def check(name, got, want):
    global failures
    if got != want:
        failures += 1
        print("FAIL %s: got %r, want %r" % (name, got, want))


check("white paint", reader.texel_reflective(255, 255, 255, 255), True)
check("red paint", reader.texel_reflective(255, 0, 0, 255), True)
check("a dim colour of 64", reader.texel_reflective(0, 64, 0, 255), True)
check("black is not paint", reader.texel_reflective(0, 0, 0, 255), False)
check("nearly black", reader.texel_reflective(63, 20, 10, 255), False)
check("transparent", reader.texel_reflective(255, 255, 255, 0), False)
check("faint", reader.texel_reflective(255, 255, 255, 127), False)
check("half opaque", reader.texel_reflective(255, 255, 255, 128), True)

with tempfile.TemporaryDirectory() as tmp:
    texture = Image.new("RGBA", (6, 4), (10, 20, 30, 255))
    texture.putpixel((0, 0), (200, 100, 50, 0))   # a transparent texel of the texture
    path = os.path.join(tmp, "slot_03.psd")
    writer.write_track_psd(path, texture)
    psd = Image.open(path)
    check("psd: size and two layers", (psd.size, getattr(psd, "n_frames", 1)), ((6, 4), 2))
    check("psd: the empty mask leaves the texture as it is", psd.convert("RGB").getpixel((3, 2)), (10, 20, 30))
    # the drafts' names become the PSDs' and the masks': <name>.rgb.png -> <name>.psd -> <name>.png
    drafts, out = os.path.join(tmp, "drafts"), os.path.join(tmp, "psds")
    os.makedirs(os.path.join(drafts, "CL1"))
    texture.save(os.path.join(drafts, "CL1", "scenery_02.rgb.png"))
    check("main writes one PSD", writer.main(["x", drafts, out]), 0)
    check("the PSD is named by the texture", sorted(os.listdir(os.path.join(out, "CL1"))), ["scenery_02.psd"])
print("test_track_mask_psd: %d failures" % failures)
sys.exit(1 if failures else 0)
