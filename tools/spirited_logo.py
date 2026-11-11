#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""The Spirited Drive banner: draws the lettering (our own outlines), writes it as SVG, and makes the boot screen's
PNG from it: 580 x 140 at the 640 x 480 front end's own resolution, RGBA with a soft shadow. The game lays it over
the screen's background and cuts the result to the screen's palette, as the original title screen was one image.

    tools/spirited_logo.py [--svg art/logo/spirited_logo.svg] [--png data/logo/spirited_logo.png]

Needs Pillow and rsvg-convert. The same input always gives the same files.
"""
import argparse
import io
import subprocess
import sys
from pathlib import Path

from PIL import Image

W, H = 580, 140                     # the boot screen's box, shown 1:1
CAP = 100                           # the letters are drawn 100 units tall
GAP = 5                             # units between letters
SX, SY = 0.74, 0.54                 # units to pixels: wide, low letters
LEAN = 18                           # degrees
ROW_TOP = (13, 70)                  # the two rows' tops, in pixels before the lean
RIGHT = 577                         # where both rows end, before the lean
SHADOW = 4                          # the hard shadow's offset, pixels
GRAIN = 6

RED = ("#a00018", "#f60030", "#ff1818")
AMBER = ("#ff4a10", "#ff7118", "#ffae10")

# Each letter: its width and its drawing in a box CAP tall. Fills use the even-odd rule, so counters are subpaths.
S_SPINE = "M79,31 C79,18 65,14 46,14 C26,14 14,21 14,33 C14,45 24,49 46,52 C68,55 79,59 79,69 C79,80 66,86 46,86 C26,86 13,82 13,68"
LETTERS = {
    "S": (92, f'<path d="{S_SPINE}" fill="none" stroke="{{ink}}" stroke-width="28"/>'),
    "P": (96, '<path d="M0,0 H56 C84,0 96,12 96,32 C96,52 84,64 56,64 H33 V100 H0 Z M33,23 H52 C60,23 64,26 64,32 C64,38 60,41 52,41 H33 Z"/>'),
    "I": (33, '<path d="M0,0 H33 V100 H0 Z"/>'),
    "R": (101, '<path d="M0,0 H58 C84,0 96,11 96,30 C96,45 88,55 73,59 L101,100 H65 L42,62 H33 V100 H0 Z M33,23 H54 C61,23 64,26 64,31 C64,36 61,39 54,39 H33 Z"/>'),
    "T": (90, '<path d="M0,0 H90 V25 H61.5 V100 H28.5 V25 H0 Z"/>'),
    "E": (84, '<path d="M0,0 H84 V24 H33 V38 H78 V61 H33 V76 H84 V100 H0 Z"/>'),
    "D": (100, '<path d="M0,0 H52 C86,0 100,20 100,50 C100,80 86,100 52,100 H0 Z M33,24 H50 C62,24 67,34 67,50 C67,66 62,76 50,76 H33 Z"/>'),
    "V": (104, '<path d="M0,0 H35 L52,60 L69,0 H104 L70,100 H34 Z"/>'),
}
ROWS = (("SPIRITED", RED, "red"), ("DRIVE", AMBER, "amber"))


def row_width(word):
    return sum(LETTERS[c][0] for c in word) + GAP * (len(word) - 1)


def row_svg(word, ink):
    """One row in letter units, its origin at the row's top left."""
    out, x = [], 0
    for c in word:
        width, shape = LETTERS[c]
        out.append(f'<g transform="translate({x} 0)">{shape.replace("{ink}", ink)}</g>')
        x += width + GAP
    return "".join(out)


def rows_svg(inks):
    """Both rows in pixels (before the lean), right-aligned at RIGHT. An ink that is a gradient runs along the whole
    row, so it is painted as one bar seen through the letters."""
    out = []
    for (word, _, name), top, ink in zip(ROWS, ROW_TOP, inks):
        left = RIGHT - row_width(word) * SX
        place = f'transform="translate({left:.2f} {top}) scale({SX} {SY})"'
        if ink.startswith("url("):
            out.append(f'<mask id="row-{name}" maskUnits="userSpaceOnUse" x="-20" y="-20" width="{row_width(word) + 40}" height="{CAP + 40}">'
                       f'<g fill="#fff" fill-rule="evenodd">{row_svg(word, "#fff")}</g></mask>'
                       f'<g {place}><rect x="-20" y="-20" width="{row_width(word) + 40}" height="{CAP + 40}" fill="{ink}" mask="url(#row-{name})"/></g>')
        else:
            out.append(f'<g {place} fill="{ink}" fill-rule="evenodd">{row_svg(word, ink)}</g>')
    return "".join(out)


def lean_x(x, y):
    """Where a point before the lean lands."""
    import math
    return x - math.tan(math.radians(LEAN)) * y


def streaks_svg():
    """Each row's colour trailing off to the left, two bars a row."""
    out = []
    for (word, colours, name), top in zip(ROWS, ROW_TOP):
        cap = CAP * SY
        left = RIGHT - row_width(word) * SX
        for frac, thick, length in ((0.20, 0.22, 150), (0.62, 0.16, 210)):
            y = top + cap * frac
            end = lean_x(left, y + cap * thick / 2) + 6
            out.append(f'<rect x="{max(0.0, end - length):.1f}" y="{y:.1f}" width="{min(length, end):.1f}" height="{cap * thick:.1f}" fill="url(#streak-{name})"/>')
    return "".join(out)


def svg():
    lean = f'transform="skewX(-{LEAN})"'
    grads = []
    for word, colours, name in ROWS:
        stops = "".join(f'<stop offset="{o}" stop-color="{c}"/>' for o, c in zip((0, 0.42, 1), colours))
        grads.append(f'<linearGradient id="ink-{name}" gradientUnits="userSpaceOnUse" x1="0" x2="{row_width(word)}" y1="0" y2="0">{stops}</linearGradient>')
        grads.append(f'<linearGradient id="streak-{name}" x1="0" x2="1"><stop offset="0" stop-color="{colours[1]}" stop-opacity="0"/>'
                     f'<stop offset="1" stop-color="{colours[1]}" stop-opacity="1"/></linearGradient>')
    smear = "".join(f'<g transform="translate({dx} {SHADOW})" opacity="{o}">{rows_svg(("#000", "#000"))}</g>' for dx, o in ((-6, 0.85), (-34, 0.6), (-72, 0.4), (-116, 0.24), (-160, 0.12)))
    return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}">
<defs>{"".join(grads)}
<filter id="smear" x="-40%" width="180%" y="-30%" height="160%"><feGaussianBlur stdDeviation="17 2.6"/></filter>
<filter id="soft" x="-10%" width="120%" y="-30%" height="160%"><feGaussianBlur stdDeviation="2.4"/></filter></defs>
<g {lean} filter="url(#smear)">{smear}</g>
{streaks_svg()}
<g {lean}><g transform="translate({SHADOW} {SHADOW})" filter="url(#soft)">{rows_svg(("#000", "#000"))}{rows_svg(("#000", "#000"))}</g>{rows_svg(("url(#ink-red)", "url(#ink-amber)"))}</g>
</svg>
'''


def grain(x, y):
    """A fixed speckle, so the output does not change from run to run."""
    v = (x * 374761393 + y * 668265263) & 0xFFFFFFFF
    v = ((v ^ (v >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((v ^ (v >> 16)) & 0xFF) / 255.0 * 2 - 1


def finish(rgba):
    """Film grain on what is drawn; the cover stays soft, so the shadow melts into whatever is behind."""
    rgba = rgba.convert("RGBA")
    px = rgba.load()
    for y in range(rgba.height):
        for x in range(rgba.width):
            r, g, b, a = px[x, y]
            if a:
                n = grain(x, y) * GRAIN
                px[x, y] = (*(max(0, min(255, round(v + n))) for v in (r, g, b)), a)
    return rgba


def render(text):
    done = subprocess.run(["rsvg-convert", "-w", str(W), "-h", str(H)], input=text.encode(), capture_output=True, check=True)
    return Image.open(io.BytesIO(done.stdout))


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--svg", type=Path, default=Path("art/logo/spirited_logo.svg"))
    ap.add_argument("--png", type=Path, default=Path("data/logo/spirited_logo.png"))
    args = ap.parse_args(argv)
    text = svg()
    for path in (args.svg, args.png):
        path.parent.mkdir(parents=True, exist_ok=True)
    args.svg.write_text(text)
    banner = finish(render(text))
    banner.save(args.png, optimize=True)
    print(f"{args.svg}  {args.png}  {banner.width}x{banner.height}  RGBA")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
