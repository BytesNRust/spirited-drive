#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""The Spirited Drive icon: a pair of driving gloves with a car key and its leather fob lying on them, seen as on a
table in front of you, over the red of the 1996 title screen. The small sizes show the key and fob alone.

    tools/spirited_icon.py [--out data/icon] [--only SIZE]

Writes spirited_<size>.png for every size, spirited.ico (Windows) and tnfs.icns (macOS, the name the packaging looks for). Needs Pillow and rsvg-convert. The same input always gives the same files.
"""
import argparse
import io
import random
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, str(Path(__file__).resolve().parent))
from spirited_logo import LETTERS, grain  # noqa: E402  the fob's initials are the banner's own letters

N = 768                                   # the working canvas
FILL = 0.9                                # how much of the frame the objects take
SMALL = (16, 32)                          # key and fob alone
LARGE = (48, 64, 128, 256, 512, 1024)     # gloves and key
GRID = 128                                # sizes above this are this size, enlarged by whole factors
# colours and grain per drawn size
FINISH = {16: (16, 3), 32: (16, 4), 48: (32, 6), 64: (32, 7), 128: (48, 7)}

# 16 px is too small to reduce a drawing to: the key and fob, pixel by pixel.
PIXELS_16 = (
    "................",
    "......ooo.......",
    ".....o...o......",
    ".....o...o......",
    "....ffooo.kk....",
    "...fFFFf.kkkk...",
    "..fFccFfx.kkss..",
    "..fFFcFfx..xsst.",
    "..fFccFfx...sst.",
    "..fFFFFfx....sst",
    "..fFccFfx....xst",
    "..fFcFcfx......x",
    "..fFccFfx.......",
    "...fFFfxx.......",
    "....ffxx........",
    "................",
)
INKS_16 = {".": (224, 0, 0), "x": (130, 0, 0), "o": (216, 212, 204), "k": (18, 16, 16), "s": (240, 236, 226), "t": (150, 145, 138),
           "f": (26, 10, 5), "F": (84, 40, 22), "c": (246, 226, 184)}

DEFS = '''<defs>
<linearGradient id="finger" x1="0" x2="1"><stop offset="0" stop-color="#8a5220"/><stop offset=".3" stop-color="#e2ad68"/><stop offset=".62" stop-color="#cf9350"/><stop offset="1" stop-color="#7c4618"/></linearGradient>
<linearGradient id="palm" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#dfa862"/><stop offset=".55" stop-color="#c58845"/><stop offset="1" stop-color="#8f5a24"/></linearGradient>
<linearGradient id="cuff" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#9a6228"/><stop offset="1" stop-color="#6e4016"/></linearGradient>
<linearGradient id="fob" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#5a2a18"/><stop offset="1" stop-color="#1e0a05"/></linearGradient>
<linearGradient id="steel" x1="0" x2="1"><stop offset="0" stop-color="#f4f0e8"/><stop offset="1" stop-color="#9a958c"/></linearGradient>
<filter id="under"><feColorMatrix type="matrix" values=".62 0 0 0 0 0 .55 0 0 0 0 0 .5 0 0 0 0 0 1 0"/></filter>
</defs>'''

FINGERS = ((16.5, 11, 26), (24.1, 6, 30), (31.7, 8, 28), (39.3, 14, 24))
THUMB = '<g transform="translate(15 40) rotate(-38)"><rect x="-3.8" y="-15" width="7.6" height="20" rx="3.8"{paint}/></g>'


def glove_shapes(finger, palm):
    fingers = "".join(f'<rect x="{x}" y="{y}" width="6.8" height="{h}" rx="3.4"{finger}/>' for x, y, h in FINGERS)
    return fingers + f'<rect x="16" y="29" width="30.6" height="27" rx="7"{palm}/>' + THUMB.replace("{paint}", finger)


def glove():
    """One glove, back of the hand up, fingers away from the wrist, in a box about 64 across."""
    line = ' fill="#4a2810" stroke="#4a2810" stroke-width="2.4" stroke-linejoin="round"'
    holes = "".join(f'<ellipse cx="{x}" cy="32.5" rx="2" ry="1.4" fill="#2a0a05"/>' for x in (19.9, 27.5, 35.1, 42.7))
    dots = "".join(f'<circle cx="{x}" cy="{y}" r=".7" fill="#4a2810"/>'
                   for x, y in ((27.5, 13), (27.5, 18), (27.5, 23), (35.1, 15), (35.1, 20), (35.1, 25), (19.9, 18), (19.9, 23), (42.7, 21), (42.7, 26)))
    creases = "".join(f'<path d="M{x + 1},{y + h * .45} h4.8" stroke="#7c4618" stroke-width=".7" opacity=".7"/>' for x, y, h in FINGERS)
    return (glove_shapes(line, line) + glove_shapes(' fill="url(#finger)"', ' fill="url(#palm)"')
            + '<g stroke="#4a2810" stroke-width="1"><line x1="23.7" y1="30" x2="23.7" y2="36"/><line x1="31.3" y1="30" x2="31.3" y2="36"/><line x1="38.9" y1="30" x2="38.9" y2="36"/></g>'
            + holes + creases + '<ellipse cx="31.3" cy="43" rx="8.5" ry="5" fill="#2a0a05"/>'
            '<path d="M22.8,43 a8.5,5 0 0 1 17,0" fill="none" stroke="#e8b874" stroke-width=".8" opacity=".8"/>'
            '<rect x="16" y="50" width="30.6" height="6" rx="2.5" fill="url(#cuff)"/><circle cx="40.5" cy="53" r="2" fill="#f4f0e8"/>' + dots)


def initial(letter, bottom):
    """One of the banner's letters, cream, leaning, centred on the fob's axis."""
    width, shape = LETTERS[letter]
    kx, ky = 0.08, 0.068
    left = -width * kx / 2 - 1.2
    return (f'<g transform="translate({left:.2f} {bottom}) skewX(-18) scale({kx} {ky}) translate(0 -100)" fill="#f6e2b8" fill-rule="evenodd">'
            f'{shape.replace("{ink}", "#f6e2b8")}</g>')


def keys():
    """The ring, the dark fob with its initials, and the key, in a box about 64 across."""
    return ('<circle cx="32" cy="15" r="8.5" fill="none" stroke="url(#steel)" stroke-width="2.4"/>'
            '<g transform="translate(27 22) rotate(24)">'
            '<rect x="-9" y="2" width="18" height="33" rx="8" fill="url(#fob)" stroke="#0c0402" stroke-width="1.6"/>'
            '<rect x="-6.3" y="4.7" width="12.6" height="27.6" rx="5.6" fill="none" stroke="#f6e2b8" stroke-width=".9" stroke-dasharray="1.6 1.4"/>'
            '<circle cx="0" cy="8.5" r="2.3" fill="url(#steel)" stroke="#0c0402" stroke-width=".6"/>'
            + initial("S", 19.4) + initial("D", 27.6) + '</g>'
            '<g transform="translate(37 23) rotate(-32)">'
            '<polygon points="-2.6,13 2.6,13 2.6,17 4.4,18.5 2.6,20 2.6,23 4.4,24.5 2.6,26 2.6,31 0,36 -2.6,33" fill="url(#steel)" stroke="#55524e" stroke-width=".6"/>'
            '<rect x="-7.5" y="0" width="15" height="14" rx="4.5" fill="#151515" stroke="#000" stroke-width=".8"/>'
            '<rect x="-3.4" y="2.6" width="6.8" height="3" rx="1.5" fill="#7a0a04"/></g>')


def at(body, x, y, turn, k, cx=30, cy=32):
    return f'<g transform="translate({x} {y}) rotate({turn}) scale({k}) translate({-cx} {-cy})">{body}</g>'


def flat(body, box):
    """A drawing seen from straight above, on the working canvas."""
    text = f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{box}">{DEFS}{body}</svg>'
    done = subprocess.run(["rsvg-convert", "-w", str(N), "-h", str(N)], input=text.encode(), capture_output=True, check=True)
    return Image.open(io.BytesIO(done.stdout)).convert("RGBA")


def solve(rows):
    """Gauss-Jordan on an 8 x 9 system."""
    for i in range(8):
        pivot = max(range(i, 8), key=lambda r: abs(rows[r][i]))
        rows[i], rows[pivot] = rows[pivot], rows[i]
        for r in range(8):
            if r != i:
                f = rows[r][i] / rows[i][i]
                rows[r] = [a - f * b for a, b in zip(rows[r], rows[i])]
    return [rows[i][8] / rows[i][i] for i in range(8)]


def on_table(im, top, bottom, narrow):
    """The flat drawing lying on a table in front of the viewer: the far edge is higher and narrower."""
    far, near = [(N * narrow, N * top), (N * (1 - narrow), N * top)], [(N * 1.06, N * bottom), (-N * .06, N * bottom)]
    rows = []
    for (x, y), (u, v) in zip(far + near, [(0, 0), (N, 0), (N, N), (0, N)]):
        rows += [[x, y, 1, 0, 0, 0, -u * x, -u * y, u], [0, 0, 0, x, y, 1, -v * x, -v * y, v]]
    return im.transform((N, N), Image.Transform.PERSPECTIVE, solve([list(map(float, r)) for r in rows]), Image.Resampling.BICUBIC)


def thick(im, depth, side):
    """Gives a lying object its edge: the outline repeated downwards in a darker tone, the object on top."""
    out = Image.new("RGBA", im.size, (0, 0, 0, 0))
    edge = Image.new("RGBA", im.size, side + (255,))
    alpha = im.split()[3]
    for dy in range(depth, 0, -1):
        out.paste(edge, (0, dy), alpha)
    out.alpha_composite(im)
    return out


def title_red():
    """The mottled red of the 1996 title screen, from its palette's values."""
    rnd = random.Random(7)
    reds = [(255, 0, 0), (255, 24, 24), (230, 0, 0), (206, 0, 0), (182, 0, 0), (149, 0, 0), (121, 0, 0)]
    small = Image.new("RGB", (24, 24))
    small.putdata([rnd.choice(reds) for _ in range(24 * 24)])
    bg = small.resize((N, N), Image.Resampling.BICUBIC).filter(ImageFilter.GaussianBlur(14))
    return Image.blend(bg, Image.new("RGB", (N, N), (230, 0, 0)), .45)


def framed(objs):
    """The objects centred, filling the frame, with their shadow on the red."""
    crop = objs.crop(objs.getbbox())
    k = N * FILL / max(crop.size)
    crop = crop.resize((round(crop.width * k), round(crop.height * k)), Image.Resampling.LANCZOS)
    objs = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    objs.paste(crop, ((N - crop.width) // 2, (N - crop.height) // 2 - int(N * .012)))
    bg = title_red().convert("RGBA")
    shade = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    shade.paste((40, 0, 0, 180), (int(N * .016), int(N * .028)), objs.split()[3])
    bg.alpha_composite(shade.filter(ImageFilter.GaussianBlur(N * .016)))
    bg.alpha_composite(objs)
    return bg.convert("RGB")


def scene_large():
    under = '<g filter="url(#under)">' + at(glove(), 22, 30, -46, .8) + '</g>'
    box = "-7 -6 76 76"
    gloves = thick(on_table(flat(under + at(glove(), 37, 26, 16, .8), box), .17, 1.0, .11), 13, (58, 30, 12))
    key = thick(on_table(flat(at(keys(), 40, 50, -14, .7, 35, 34), box), .17, 1.0, .11), 4, (20, 12, 8))
    gloves.alpha_composite(key)
    return framed(gloves)


def scene_small():
    return framed(thick(on_table(flat(at(keys(), 32, 30, -14, 1.28, 35, 34), "-10 -10 84 84"), .16, .98, .12), 5, (20, 12, 8)))


def finish(scene, size):
    """1996: the game's resolution, grain, a short palette, hard pixels; the corners rounded off."""
    if size == 16:
        im = Image.new("RGBA", (16, 16))
        im.putdata([INKS_16[c] + (255,) for row in PIXELS_16 for c in row])
        return im
    drawn = min(size, GRID)
    colours, speck = FINISH[drawn]
    im = scene.resize((drawn, drawn), Image.Resampling.LANCZOS)
    px = im.load()
    for y in range(drawn):
        for x in range(drawn):
            n = grain(x, y) * speck
            px[x, y] = tuple(max(0, min(255, round(v + n))) for v in px[x, y])
    im = im.quantize(colours, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.FLOYDSTEINBERG).convert("RGBA")
    if drawn >= 48:
        mask = Image.new("L", (drawn * 4, drawn * 4), 0)
        ImageDraw.Draw(mask).rounded_rectangle((0, 0, drawn * 4 - 1, drawn * 4 - 1), radius=drawn * 4 * .14, fill=255)
        im.putalpha(mask.resize((drawn, drawn), Image.Resampling.LANCZOS).point(lambda v: 255 if v >= 128 else 0))
    return im.resize((size, size), Image.Resampling.NEAREST)


def pack(made, out):
    """The same pictures as Windows and macOS want them: one .ico, one .icns."""
    ico = [made[n] for n in (16, 32, 48, 256)]
    ico[-1].save(out / "spirited.ico", format="ICO", sizes=[im.size for im in ico], append_images=ico[:-1])
    icns = [made[n] for n in (16, 32, 64, 128, 256, 512, 1024)]
    icns[-1].save(out / "tnfs.icns", format="ICNS", append_images=icns[:-1])
    print(out / "spirited.ico")
    print(out / "tnfs.icns")


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--out", type=Path, default=Path("data/icon"))
    ap.add_argument("--only", type=int, choices=SMALL + LARGE)
    args = ap.parse_args(argv)
    args.out.mkdir(parents=True, exist_ok=True)
    scenes, made = {}, {}
    for size in (args.only,) if args.only else SMALL + LARGE:
        kind = "small" if size in SMALL else "large"
        if kind not in scenes:
            scenes[kind] = scene_small() if kind == "small" else scene_large()
        path = args.out / f"spirited_{size}.png"
        made[size] = finish(scenes[kind], size)
        made[size].save(path, optimize=True)
        print(path)
    if not args.only:
        pack(made, args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
