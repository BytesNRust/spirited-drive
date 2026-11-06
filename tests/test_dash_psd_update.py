#!/usr/bin/env python3
"""tools/dash_mask_psd.py --update and its parts: tools/dash_rebuild.py (the dash behind the steering wheel from the steering frames), tools/psd_edit.py (a PSD's
layers rewritten, the others byte for byte). Skipped (exit 0) when Pillow is not installed."""
import importlib.util
import os
import struct
import sys
import tempfile

here = os.path.dirname(os.path.abspath(__file__))
tools = os.path.join(here, "..", "tools")
try:
    from PIL import Image

    def load(name):
        spec = importlib.util.spec_from_file_location(name, os.path.join(tools, name + ".py"))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    dash_psd, rebuild, psd_edit, car_psd = load("dash_mask_psd"), load("dash_rebuild"), load("psd_edit"), load("car_mask_psd")
except ImportError as error:
    print("test_dash_psd_update: skipped (%s)" % error)
    sys.exit(0)

failures = 0


def check(name, got, expected):
    global failures
    if got != expected:
        failures += 1
        print("FAIL %s: got %r, want %r" % (name, got, expected))


# ---- the rebuild: a 12 x 3 strip, art 9 = wheel baked in, frames 255 = see-through
W, H = 12, 3
dash_value, wheel_value = 7, 3
art = [dash_value] * (W * H)
art[0] = wheel_value                                  # pixel 0: wheel in the art; frames show the dash behind it in every pose but one
art[1] = wheel_value                                  # pixel 1: wheel in every pose and different each time (the rim's leather)
art[2] = 5                                            # pixel 2: the dash, no frame covers it
art[3] = wheel_value                                  # pixel 3: wheel in the art and in every frame, the same (a hub's cap)
frames = []
for k in range(6):
    f = [255] * (W * H)
    for i in (0, 1, 3):
        f[i] = dash_value
    f[1] = 100 + k                                    # a value of its own in each pose
    if k == 0:
        f[0] = wheel_value                            # a spoke over pixel 0 in one pose
    f[3] = wheel_value
    f[4] = dash_value                                 # pixel 4: the dash, every frame agrees with the art
    frames.append(f)
rebuild.RADIUS = 0   # no neighbourhood in a strip this small
dash, never = rebuild.rebuild(art, frames, W, H, 0.45, geometry=False)
check("a spoke over the pixel in one pose: the dash is what the others show", dash[0], dash_value)
check("a pixel no frame covers is the art", dash[2], 5)
check("a pixel every frame agrees with the art on is the art", dash[4], dash_value)
check("the leather's pixel, different in every pose, is never visible", never[1], 1)
check("pixels nothing covers are not never visible", (never[2], never[4]), (0, 0))

# ---- the rim: a ring of pixels that change, dash inside it (agreeing with the art), found as a circle
RW, RH = 41, 30
ring_differs, ring_blue = [0] * (RW * RH), [0] * (RW * RH)
for y in range(RH):
    for x in range(RW):
        d = ((x - 20) ** 2 + (y - 22) ** 2) ** 0.5
        if 11 <= d <= 15:
            ring_differs[y * RW + x] = 1
        elif d < 11:
            ring_blue[y * RW + x] = 1
cx, cy, outer, inner = rebuild.fit_wheel(RW, RH, ring_differs, ring_blue)
check("the rim's circle is found (the inner radius to a pixel)", (round(cx), round(cy), round(outer), abs(inner - 11) <= 1), (20, 22, 15, True))

# ---- the PSD: a four layer file as the owner has it (dash, volante, mask, color), the mask and color painted
size = (640, 480)
dash_rgb = Image.new("RGB", size, (10, 20, 30))
guide_rgb = Image.new("RGB", size, (90, 90, 90))
guide_a = Image.new("L", size, 255)
mask_alpha = Image.new("L", size, 0)
mask_alpha.putpixel((100, 100), 255)
car_psd.write_psd.__globals__["mask_planes"] = dash_psd.mask_planes
keep = Image.new("L", size, 0)
keep.putpixel((100, 100), dash_psd.LIT)
with tempfile.TemporaryDirectory() as tmp:
    owner = os.path.join(tmp, "owner", "CAR")
    os.makedirs(owner)
    source = os.path.join(owner, "dash.psd")
    car_psd.write_psd(source, dash_rgb, keep, "dash", ("volante (guía)", guide_rgb, guide_a))
    psd = psd_edit.read_psd(source)
    # a fourth layer, "color", painted by the owner: put it on top of the mask
    colour = psd_edit.Layer((0, 0, 480, 640), [(-1, 2 + 640 * 480), (0, 2 + 640 * 480), (1, 2 + 640 * 480), (2, 2 + 640 * 480)], b"norm", 200, 0, 0,
                            struct.pack(">I", 0) + struct.pack(">I", 0) + car_psd.pascal("color", 4),
                            [struct.pack(">H", 0) + (b"\xff" if c == -1 else bytes([c * 40])) * (640 * 480) for c in (-1, 0, 1, 2)])
    psd.layers = psd.layers + [colour]
    psd_edit.write_psd(source, psd)
    before = psd_edit.read_psd(source)
    check("the owner's file has dash, volante, mask and color", [l.name for l in before.layers], ["dash", "volante (guía)", "mask", "color"])
    # a round trip changes nothing
    again = os.path.join(tmp, "again.psd")
    psd_edit.write_psd(again, before)
    check("a PSD written back as read is the same file", open(again, "rb").read(), open(source, "rb").read())
    # --update
    new_dash = Image.new("RGB", size, (200, 100, 50))
    never = Image.new("L", size, 0)
    never.putpixel((5, 5), 255)
    out = os.path.join(tmp, "out")
    target = os.path.join(out, "CAR", "dash.psd")
    os.makedirs(os.path.dirname(target))
    dash_psd.update_psd(source, target, new_dash, (Image.new("RGB", size, (1, 2, 3)), Image.new("L", size, 255)), never)
    after = psd_edit.read_psd(target)
    names = [l.name for l in after.layers]
    check("update keeps the layers and adds nunca visible", names, ["dash", "volante (guía)", "nunca visible", "mask", "color"])
    by = {l.name: l for l in after.layers}
    old = {l.name: l for l in before.layers}
    check("the mask layer is byte for byte the owner's", (by["mask"].record(), by["mask"].data), (old["mask"].record(), old["mask"].data))
    check("the color layer is byte for byte the owner's", (by["color"].record(), by["color"].data), (old["color"].record(), old["color"].data))
    check("the dash layer has the new pixels", by["dash"].plane(0, 640, 480)[:3], bytes([200, 200, 200]))
    check("the guide stays hidden", by["volante (guía)"].hidden, True)
    check("nunca visible shows on the never pixels only", (by["nunca visible"].plane(-1, 640, 480)[5 * 640 + 5], by["nunca visible"].plane(-1, 640, 480)[0]), (255, 0))
    # the output dir may not be the owner's
    check("the owner's dir is refused as the output", dash_psd.update(tmp, os.path.join(tmp, "owner"), os.path.join(tmp, "owner")), 2)

# ---- PackBits (what Photoshop writes): a row of 5 equal bytes and 3 literals
rle = struct.pack(">H", 1) + struct.pack(">H", 5) + bytes([256 - 4, 7, 2, 1, 2, 3])
check("an RLE channel decodes", psd_edit.decode_channel(rle, 8, 1), bytes([7] * 5 + [1, 2, 3]))

print("test_dash_psd_update: %d failure(s)" % failures)
sys.exit(1 if failures else 0)
