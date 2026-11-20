#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""The icon of Spirited Drive Studio: the game's icon in the editor's blue, with STUDIO on a dark band across the top.

    tools/studio_icon.py [--out editor/icon]

From 64 px up the pictures are the band version of the 1024 px artwork, scaled down. At 48 px and below the label cannot
be read, so there the blue icon alone is made from the game's own small pictures. Writes studio_<size>.png for 16 to
1024 px and studio.icns (macOS, through iconutil; on Apple's icon grid, as the game's tnfs.icns is). Needs Pillow,
Arial Black and iconutil (macOS). The same input always gives the same files.
"""
import argparse
import subprocess
import tempfile
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parent.parent
GAME_ICON_DIR = ROOT / "data" / "icon"
FONT = "/System/Library/Fonts/Supplemental/Arial Black.ttf"

SIZES = (16, 32, 48, 64, 128, 256, 512, 1024)
PLAIN_MAX = 48                 # up to this size the blue icon has no label
BLUE = (66, 150, 250)          # editor ImGui button blue, ImVec4(0.26, 0.59, 0.98)
BAND = (24, 64, 120)           # darker shade of the same blue, behind the label
NAVY = (10, 30, 62)            # outline round the label letters
WHITE = (255, 255, 255)
STROKE = 6
BAND_H = 260                   # on the 1024 px artwork
RED_REF = 250.0                # the value that maps the editor blue to full brightness
REDNESS_LO, REDNESS_HI = 50, 140
MAC_BODY = 824 / 1024          # Apple's icon grid: a centred body of 824 on 1024, with a soft shadow
MAC_SHADOW_BLUR, MAC_SHADOW_DROP = 28 / 1024, 10 / 1024
ICONSET = (("icon_16x16.png", 16), ("icon_16x16@2x.png", 32), ("icon_32x32.png", 32), ("icon_32x32@2x.png", 64),
           ("icon_128x128.png", 128), ("icon_128x128@2x.png", 256), ("icon_256x256.png", 256),
           ("icon_256x256@2x.png", 512), ("icon_512x512.png", 512), ("icon_512x512@2x.png", 1024))


def pixels(img):
    data = img.tobytes()
    return list(zip(data[0::4], data[1::4], data[2::4], data[3::4]))


def redness(r, g, b):
    return r - max(g, b)


def mix(a, b, w):
    return int(round(a * (1.0 - w) + b * w))


def load_game_icon(size):
    return Image.open(GAME_ICON_DIR / ("spirited_%d.png" % size)).convert("RGBA")


def recolour_background(icon):
    """The red background, joined to the outside, turned editor blue; each pixel keeps its brightness.

    Red details enclosed by other artwork (the key-fob mark) are left alone.
    """
    last = icon.width - 3
    px = pixels(icon)
    cand = Image.new("L", icon.size, 0)
    cand.putdata([255 if redness(r, g, b) > REDNESS_LO else 0 for r, g, b, _ in px])
    seeds = [(2, 2), (last, 2), (2, last), (last, last),
             (icon.width // 2, 2), (2, icon.height // 2), (last, icon.height // 2), (icon.width // 2, last)]
    for seed in seeds:
        if cand.getpixel(seed) == 255:
            ImageDraw.floodfill(cand, seed, 128, thresh=0)
    out = []
    for (r, g, b, a), m in zip(px, cand.tobytes()):
        if m != 128:
            out.append((r, g, b, a))
            continue
        w = min(1.0, max(0.0, (redness(r, g, b) - REDNESS_LO) / float(REDNESS_HI - REDNESS_LO)))
        v = max(r, g, b) / RED_REF
        br, bg, bb = (min(255, round(c * v)) for c in BLUE)
        out.append((mix(r, br, w), mix(g, bg, w), mix(b, bb, w), a))
    result = Image.new("RGBA", icon.size)
    result.putdata(out)
    return result


def fit_font(text, max_w, max_h, start=400):
    size = start
    while size > 8:
        font = ImageFont.truetype(FONT, size)
        l, t, r, b = font.getbbox(text, stroke_width=STROKE)
        if r - l <= max_w and b - t <= max_h:
            return font
        size -= 2
    raise RuntimeError("label does not fit")


def draw_centred(draw, cx, cy, text, font):
    l, t, r, b = draw.textbbox((0, 0), text, font=font, stroke_width=STROKE)
    x = cx - (r - l) / 2.0 - l
    y = cy - (b - t) / 2.0 - t
    draw.text((x, y), text, font=font, fill=WHITE, stroke_width=STROKE, stroke_fill=NAVY)


def label_band(base):
    """STUDIO on a band across the top of the 1024 px artwork, clipped to the icon's own shape."""
    size = base.width
    layer = Image.new("RGBA", base.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)
    draw.rectangle((0, 0, size - 1, BAND_H - 1), fill=BAND + (255,))
    draw.rectangle((0, BAND_H - 8, size - 1, BAND_H - 1), fill=BLUE + (255,))
    draw_centred(draw, size / 2.0, BAND_H / 2.0, "STUDIO", fit_font("STUDIO", 840, BAND_H - 90))
    layer.putalpha(ImageChops.multiply(layer.getchannel("A"), base.getchannel("A")))
    return Image.alpha_composite(base, layer)


def pictures():
    """Studio's picture at each size: the band version from 64 px up, the blue icon alone below."""
    band = label_band(recolour_background(load_game_icon(1024)))
    out = {}
    for size in SIZES:
        if size <= PLAIN_MAX:
            out[size] = recolour_background(load_game_icon(size))
        else:
            out[size] = band.resize((size, size), Image.Resampling.LANCZOS)
    return out


def on_mac_grid(art):
    """The picture on Apple's icon grid at its own size, with the shadow the game's tnfs.icns has."""
    size = art.width
    body = round(size * MAC_BODY)
    margin = (size - body) // 2
    im = art.resize((body, body), Image.Resampling.LANCZOS)
    shade = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    shade.paste((0, 0, 0, 128), (margin, margin + round(size * MAC_SHADOW_DROP)), im.getchannel("A"))
    out = shade.filter(ImageFilter.GaussianBlur(size * MAC_SHADOW_BLUR / 2))
    out.alpha_composite(im, (margin, margin))
    return out


def write_icns(pics, path):
    with tempfile.TemporaryDirectory() as tmp:
        iconset = Path(tmp) / "studio.iconset"
        iconset.mkdir()
        for name, size in ICONSET:
            on_mac_grid(pics[size]).save(iconset / name)
        subprocess.run(["iconutil", "-c", "icns", str(iconset), "-o", str(path)], check=True)


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--out", type=Path, default=ROOT / "editor" / "icon")
    args = ap.parse_args(argv)
    args.out.mkdir(parents=True, exist_ok=True)
    pics = pictures()
    for size, im in pics.items():
        im.save(args.out / ("studio_%d.png" % size))
    write_icns(pics, args.out / "studio.icns")
    print(args.out)


if __name__ == "__main__":
    main(__import__("sys").argv[1:])
