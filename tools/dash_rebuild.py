#!/usr/bin/env python3
"""The dash's art without its steering wheel, for tools/dash_mask_psd.py: the pixels the wheel covers in the straight pose (baked into the art) rebuilt from the
steering frames, and the pixels no frame ever shows.

The game draws the wheel over the dash with the 'w' frames (game/race/wheel_front.h): each is an opaque rectangle (the wheel in one pose and the dash around and
behind it, see-through 255 outside) laid over the art, which holds the straight pose. So, pixel by pixel over the frames that cover it:
  * every frame agrees with the art: nothing ever covers it, it is the dash (or a bit of the wheel that is the same in every pose, see below);
  * most frames agree on one value (MIN_SHARE of them): the poses where no wheel is on the pixel show the dash, that value; a spoke that passes over the pixel
    in a few poses is the others;
  * no value is shared by most frames: the wheel is on the pixel in most poses, and the leather's texture turns with it: nothing shows what is behind.
The rim is a circle and the hub a disc: they do not change with the steering, so no frame shows what is behind them, and some of their pixels are the same
in every frame (the rim's highlight, the hub's cap), which the rules above take for dash. They are found as geometry: the circle of the wheel's rim fitted to the
pixels that change with the steering, as wide as the rim is along the wheel's vertical axis, and the hub, a disc around the wheel's centre of a fifth of the rim's radius. What is inside them, and the pixels no frame shows, is "never visible"; a majority vote cleans the mask of lone pixels.
"""
from collections import Counter

MIN_SHARE = 0.45   # of the frames covering a pixel, the part that must agree on a value for it to be the dash's
HUB_SHARE = 0.22   # the hub's radius over the rim's outer radius
RADIUS = 4         # the neighbourhood of the majority vote: (2 * RADIUS + 1) squared
SEE_THROUGH = 255


def vote(never, width, height, radius=None):
    """`never` (0/1 per pixel) with each pixel set to the majority of its square neighbourhood (ties keep the pixel's own)."""
    radius = RADIUS if radius is None else radius
    out = list(never)
    for y in range(height):
        for x in range(width):
            y0, y1, x0, x1 = max(0, y - radius), min(height, y + radius + 1), max(0, x - radius), min(width, x + radius + 1)
            total = sum(sum(never[row * width + x0: row * width + x1]) for row in range(y0, y1))
            cells = (y1 - y0) * (x1 - x0)
            if total * 2 > cells:
                out[y * width + x] = 1
            elif total * 2 < cells:
                out[y * width + x] = 0
    return out


def fit_wheel(width, height, differs, blue):
    """(cx, cy, outer, inner) of the wheel's rim (the circle through the pixels that change with the steering, and the radius where the dash inside it starts on
    the vertical axis), or None when nothing changes."""
    pts = [i for i in range(width * height) if differs[i]]
    if not pts:
        return None
    xs, ys = [i % width for i in pts], [i // width for i in pts]
    xmin, xmax, ymin = min(xs), max(xs), min(ys)
    cx, outer = (xmin + xmax) / 2.0, (xmax - xmin) / 2.0
    col, y = int(round(cx)), ymin
    while y < height and not blue[y * width + col]:
        y += 1
    return cx, ymin + outer, outer, max(0.0, outer - (y - ymin))


def rebuild(art, frames, width, height, min_share=MIN_SHARE, geometry=True):
    """(dash, never): dash[i] is the palette index of the dash at pixel i (the art where the wheel never covers it), never[i] 1 where no pose shows the dash.
    art: width*height palette indices, frames: lists of width*height indices (255 see-through). geometry False leaves out the rim and the hub (the pixel rules alone)."""
    n = width * height
    dash, never = list(art), [0] * n
    blue, differs, shown = [0] * n, [0] * n, [0] * n
    for i in range(n):
        vs = [f[i] for f in frames if f[i] != SEE_THROUGH]
        if not vs:
            continue
        if all(v == art[i] for v in vs):
            blue[i] = 1
            continue
        differs[i] = 1
        value, hits = Counter(vs).most_common(1)[0]
        if hits >= min_share * len(vs):
            dash[i], shown[i] = value, 1
        else:
            never[i] = 1
    wheel = fit_wheel(width, height, differs, blue) if geometry else None
    if wheel is not None:
        cx, cy, outer, inner = wheel
        hub = HUB_SHARE * outer   # the hub, the disc around the wheel's centre (the same in every pose)
        for i in range(n):
            if not (differs[i] or blue[i]):
                continue
            d = (((i % width) - cx) ** 2 + ((i // width) - cy) ** 2) ** 0.5
            if d >= inner and d <= outer + 1 or d < hub:
                never[i] = 1
    never = vote(never, width, height)
    for i in range(n):
        if never[i]:
            dash[i] = art[i]
    return dash, never
