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
    auto = load("track_mask_auto")
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

# reflective_mask on stand-in layers (no psd-tools needed): only the layer named "mask" counts, a `draft` over it is ignored
from PIL import Image as _Image


class FakeLayer:
    def __init__(self, name, image, left=0, top=0):
        self.name, self._image, self.left, self.top = name, image, left, top

    def is_group(self):
        return False

    def topil(self):
        return self._image


class FakePsd(list):
    width, height = 4, 3


painted = _Image.new("RGBA", (2, 1), (255, 255, 255, 255))
guess = _Image.new("RGBA", (4, 3), (255, 255, 255, 255))
psd = FakePsd([FakeLayer("texture", _Image.new("RGBA", (4, 3), (9, 9, 9, 255))), FakeLayer("draft", guess), FakeLayer("mask", painted, left=1, top=2)])
got = reader.reflective_mask(psd)
check("mask layer only: size", got.size, (4, 3))
check("mask layer only: the painted texels land where the layer is", [got.getpixel((x, y)) for y in range(3) for x in range(4)], [0] * 8 + [0, 255, 255, 0])
psd_no_mask = FakePsd([FakeLayer("draft", guess)])
try:
    reader.reflective_mask(psd_no_mask)
    check("a PSD with a draft and no mask is refused", False, True)
except ValueError:
    pass
empty = FakePsd([FakeLayer("mask", None)])
check("an empty mask layer reads as nothing reflective", reader.reflective_mask(empty).getbbox(), None)

# the automatic guess (synthetic textures: no pixel of the game)
road = _Image.new("RGBA", (128, 256), (70, 70, 70, 255))
road.putpixel((10, 20), (220, 60, 40, 255))      # a red eye
road.putpixel((11, 20), (230, 140, 50, 255))     # an orange one
road.putpixel((12, 20), (220, 200, 20, 255))     # yellow paint: not an eye
road_mask = auto.auto_mask("slot_00", road)
check("road: only the red and orange dots", road_mask.getbbox(), (10, 20, 12, 21))
check("road: the yellow paint is not one", road_mask.getpixel((12, 20)), 0)
yellow = _Image.new("RGBA", (53, 127), (0, 0, 0, 0))
for y in range(50):
    for x in range(53):
        yellow.putpixel((x, y), (230, 170, 30, 255))
for y in range(50, 127):
    for x in range(23, 30):
        yellow.putpixel((x, y), (180, 180, 180, 255))   # the pole
yellow.putpixel((26, 25), (10, 10, 10, 255))     # the symbol
yellow.putpixel((26, 100), (10, 10, 10, 255))    # dark on the pole: not the head
m = auto.auto_mask("scenery_00", yellow)
check("yellow sign: the dark texel of the head", (m.getpixel((26, 25)), m.getpixel((26, 100)), m.getpixel((5, 5))), (255, 0, 0))
green = _Image.new("RGBA", (128, 64), (24, 64, 40, 255))
for y in range(64):
    for x in range(128):
        if x < 2 or x >= 126 or y < 2 or y >= 62:
            green.putpixel((x, y), (216, 216, 220, 255))   # the white frame
green.putpixel((60, 30), (216, 216, 220, 255))   # a letter
m = auto.auto_mask("scenery_08", green)
check("green sign: the letter yes, the frame no", (m.getpixel((60, 30)), m.getpixel((0, 30)), m.getpixel((1, 1)), m.getpixel((30, 30))), (255, 0, 0, 0))
post = _Image.new("RGBA", (4, 16), (0, 0, 0, 0))
for y in range(16):
    for x in range(4):
        post.putpixel((x, y), (60, 60, 60, 255))
post.putpixel((1, 3), (230, 140, 40, 255))
m = auto.auto_mask("scenery_18", post)
check("post: the stripe", (m.getpixel((1, 3)), m.getpixel((2, 8))), (255, 0))
# a palm's crown (31 x 127: green leaves with dark gaps) is no sign: nothing marked, whatever is dark in it
palm = _Image.new("RGBA", (31, 127), (0, 0, 0, 0))
for y in range(60):
    for x in range(31):
        palm.putpixel((x, y), (20, 90, 30, 255) if (x + y) % 3 else (8, 12, 8, 255))   # leaves and dark gaps
check("a palm's crown is not a sign", auto.auto_mask("scenery_15", palm).getbbox(), None)
check("the candidates of CL1: a listed road slot and board", (auto.is_candidate("CL1", "slot_06"), auto.is_candidate("CL1", "scenery_18")), (True, True))
check("the candidates of CL1: a palm, a tree and an unlisted slot are not", (auto.is_candidate("CL1", "scenery_15"), auto.is_candidate("CL1", "scenery_14"), auto.is_candidate("CL1", "slot_12")), (False, False, False))
check("a track with no list: everything is", (auto.is_candidate("TR9", "scenery_15"), auto.is_candidate("TR9", "slot_12")), (True, True))
check("junk names are not candidates", (auto.is_candidate("CL1", "scenery_x"), auto.is_candidate("CL1", "thing_03")), (False, False))
check("transparent is never reflective", auto.auto_mask("scenery_09", _Image.new("RGBA", (53, 127), (0, 0, 0, 0))).getbbox(), None)

with tempfile.TemporaryDirectory() as tmp:
    texture = Image.new("RGBA", (6, 4), (10, 20, 30, 255))
    texture.putpixel((0, 0), (200, 100, 50, 0))   # a transparent texel of the texture
    path = os.path.join(tmp, "slot_03.psd")
    writer.write_track_psd(path, texture)
    psd = Image.open(path)
    check("psd: size and two layers", (psd.size, getattr(psd, "n_frames", 1)), ((6, 4), 2))
    check("psd: the empty mask leaves the texture as it is", psd.convert("RGB").getpixel((3, 2)), (10, 20, 30))
    writer.write_track_psd(path, texture, _Image.new("L", (6, 4), 255))
    psd = Image.open(path)
    check("psd with a draft: three layers, the draft over the mask", ([layer[0] for layer in psd.layers] if hasattr(psd, "layers") else None), ["texture", "mask", "draft"])
    check("psd with a draft: hidden, the picture is as it was", psd.convert("RGB").getpixel((3, 2)), (10, 20, 30))
    # the drafts' names become the PSDs' and the masks': <name>.rgb.png -> <name>.psd -> <name>.png
    drafts, out = os.path.join(tmp, "drafts"), os.path.join(tmp, "psds")
    os.makedirs(os.path.join(drafts, "CL1"))
    texture.save(os.path.join(drafts, "CL1", "scenery_02.rgb.png"))
    pngs = os.path.join(tmp, "pngs")
    check("main writes one PSD", writer.main(["x", drafts, out, "--png", pngs]), 0)
    check("the PSD is named by the texture", sorted(os.listdir(os.path.join(out, "CL1"))), ["scenery_02.psd"])
    check("--png writes the guess under the same name", sorted(os.listdir(os.path.join(pngs, "CL1"))), ["scenery_02.png"])
    # the default writes the track's candidates only; --all writes every texture
    texture.save(os.path.join(drafts, "CL1", "scenery_15.rgb.png"))
    os.makedirs(os.path.join(drafts, "TR9"))
    texture.save(os.path.join(drafts, "TR9", "scenery_15.rgb.png"))
    out2 = os.path.join(tmp, "psds2")
    check("main by default", writer.main(["x", drafts, out2]), 0)
    check("default: CL1 only its candidates", sorted(os.listdir(os.path.join(out2, "CL1"))), ["scenery_02.psd"])
    check("default: a track with no list gets all", sorted(os.listdir(os.path.join(out2, "TR9"))), ["scenery_15.psd"])
    out3 = os.path.join(tmp, "psds3")
    check("main --all", writer.main(["x", drafts, out3, "--all"]), 0)
    check("--all: CL1 every texture", sorted(os.listdir(os.path.join(out3, "CL1"))), ["scenery_02.psd", "scenery_15.psd"])
    check("the guess PNG is the texture's size, grey", (Image.open(os.path.join(pngs, "CL1", "scenery_02.png")).size, Image.open(os.path.join(pngs, "CL1", "scenery_02.png")).mode), ((6, 4), "L"))
print("test_track_mask_psd: %d failures" % failures)
sys.exit(1 if failures else 0)
