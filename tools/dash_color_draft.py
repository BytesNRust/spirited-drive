#!/usr/bin/env python3
"""A first dash_color.png for each dash, drafted from the dash's own art (dep98 item 2, the owner's dashboards).

The colour layer of a dash (game/race/dash_color.h) is a few flat colours painted over the lit parts: the speedo can glow another colour than the tach. This writes a draft
per car so the owner corrects it instead of painting it from nothing:

  1. the lit parts are the pixels of the car's data/dash_masks/<CAR>/dash_mask.png that are green (a dial) or white / yellow (always lit); the lit pixels that are near each
     other (10 pixels) are one gauge;
  2. each gauge takes the hue of the bright pixels the dash's art has there (the art as dash_mask_psd.py builds it from the game's dump: the dash without the wheel); an
     art that is grey or white there has no hue: the gauge takes --default-colour (orange, the colour a dash with no colour file has);
  3. the colour is that hue at full strength (the game takes only the strongest channel; the art gives the brightness), and the colours the game would fold together
     (distance 48, at most twelve) are one;
  4. the draft is painted on the gauge's lit pixels and two pixels round them.

usage: dash_color_draft.py [--game GAME --data DIR] [--stems A,B | --all] [--default-colour RRGGBB] <dump dir> <dash masks dir> <output dir>
<dump dir>        what the game wrote with TNFS_DASH_DUMP (dash_mask_psd.py's input: <STEM>.ppm, and the steering frames when it has them). With --game the stems it lacks are
                  dumped by running the game headless in each car's cockpit at night (about 10 s a car); the dump dir is made if it is not there.
<dash masks dir>  data/dash_masks (read only): <CAR>/dash_mask.png for each dash. By default the dashes with no dash_color.png; --all also the ones that have, --stems names some.
<output dir>      gets <CAR>/dash_mask.png (a copy, so the folder works as TNFS_DASH_MASKS), <CAR>/dash_color.png (the draft) and <CAR>/sheet.png: the dash | the draft | with --game,
                  the cockpit photo at night (TR1) with the draft | that photo enlarged on the lit part. Never inside art/ or data/, and a file that is there is never written over: the run stops before it writes anything.
Needs Pillow. The game's cockpit photo is the same one Studio's Race photo takes (Cockpit, Night, Gauge light 5).
"""
import argparse
import colorsys
import math
import importlib.util
import os
import subprocess
import sys
import tempfile

from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
WIDTH, HEIGHT = 640, 480
GAUGE_JOIN = 2         # lit pixels this close are one element (a numeral, a tick, a needle)
NEIGHBOUR = 60         # an element with no hue of its own takes the colour of the nearest one that has, when it is this close (same dial)
PAINT_MARGIN = 2       # the draft reaches this far round the lit pixels
FOLD_DISTANCE = 48     # game/race/dash_color.h: colours this near (sum of the channel differences) share a slot
MAX_COLOURS = 12
ORANGE = (255, 128, 0)  # a lit part with no colour glows this (gauge_light 3), and ANSX's own file paints it
LIT = {(0, 255, 0), (255, 255, 255), (255, 255, 0)}
# the game's car number (--car) of each dash's stem in a headless photo (editor/car_export.c: the model)
CAR_NUMBER = {"TSUPRA": 0, "LDIABL": 1, "P911": 2, "CZR1": 3, "F512TR": 4, "DVIPER": 5, "ANSX": 6, "MRX7": 7}
GAME_ARGS = ["--drive", "TR1", "--racers", "1", "--node", "200"]


def load_tool(name):
    spec = importlib.util.spec_from_file_location(name, os.path.join(HERE, name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def refuse_place(path):
    """None when `path` may be written, else why not: art/ and data/ are the owner's and the game's."""
    real = os.path.realpath(path)
    parts = real.split(os.sep)
    if "art" in parts or "data" in parts:
        return "%s is inside art/ or data/: the draft goes to a folder of its own" % path
    return None


def dash_image(psd_tool, dump, stem):
    """The dash as dash_mask_psd.py draws it: rebuilt without the wheel when the dump has the steering frames, else the dump's own picture."""
    if psd_tool.has_frames(dump, stem):
        return psd_tool.layers_from_frames(dump, stem)[0]
    return Image.open(os.path.join(dump, stem + ".ppm")).convert("RGB")


def lit_pixels(mask_path):
    """The set of (x, y) the mask marks lit."""
    mask = Image.open(mask_path).convert("RGB")
    if mask.size != (WIDTH, HEIGHT):
        mask = mask.resize((WIDTH, HEIGHT), Image.NEAREST)
    data = mask.tobytes()
    lit = set()
    for i in range(WIDTH * HEIGHT):
        if (data[3 * i], data[3 * i + 1], data[3 * i + 2]) in LIT:
            lit.add((i % WIDTH, i // WIDTH))
    return lit


def gauges(lit):
    """The lit pixels grouped by gauge: the groups of the lit pixels dilated by GAUGE_JOIN, each a list of (x, y)."""
    plane = Image.new("L", (WIDTH, HEIGHT), 0)
    plane.putdata([255 if (i % WIDTH, i // WIDTH) in lit else 0 for i in range(WIDTH * HEIGHT)])
    wide = plane.filter(ImageFilter.MaxFilter(2 * GAUGE_JOIN + 1)).tobytes()
    seen = bytearray(WIDTH * HEIGHT)
    groups = []
    for start in range(WIDTH * HEIGHT):
        if not wide[start] or seen[start]:
            continue
        stack, region = [start], []
        seen[start] = 1
        while stack:
            i = stack.pop()
            region.append(i)
            x, y = i % WIDTH, i // WIDTH
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if 0 <= nx < WIDTH and 0 <= ny < HEIGHT:
                    j = ny * WIDTH + nx
                    if wide[j] and not seen[j]:
                        seen[j] = 1
                        stack.append(j)
        pixels = [(i % WIDTH, i // WIDTH) for i in region if (i % WIDTH, i // WIDTH) in lit]
        if pixels:
            groups.append(pixels)
    return groups


def hue_of(art, pixels, fallback=ORANGE):
    """The full-strength colour of the hue the bright pixels of the art have under `pixels`; `fallback` (orange) when the art there is grey or white."""
    values = sorted(max(art.getpixel(p)) for p in pixels)
    floor = max(80, values[int(len(values) * 0.6)])
    bright = [art.getpixel(p) for p in pixels if max(art.getpixel(p)) >= floor]
    if not bright:
        return fallback
    mean = [sum(c[k] for c in bright) / len(bright) for k in range(3)]
    h, s, v = colorsys.rgb_to_hsv(mean[0] / 255, mean[1] / 255, mean[2] / 255)
    if s < 0.30:
        return fallback
    r, g, b = colorsys.hsv_to_rgb(h, 1.0, 1.0)
    return (round(r * 255), round(g * 255), round(b * 255))


def fold(colours):
    """The colours as the game folds them (dash_color.h): one near an earlier one (distance FOLD_DISTANCE) takes it; at most MAX_COLOURS, a later takes the nearest."""
    slots = []
    out = []
    for c in colours:
        near = [s for s in slots if sum(abs(c[k] - s[k]) for k in range(3)) <= FOLD_DISTANCE]
        if near:
            out.append(near[0])
        elif len(slots) < MAX_COLOURS:
            slots.append(c)
            out.append(c)
        else:
            out.append(min(slots, key=lambda s: sum(abs(c[k] - s[k]) for k in range(3))))
    return out


def centre(group):
    return (sum(p[0] for p in group) / len(group), sum(p[1] for p in group) / len(group))


def element_colours(art, groups, fallback):
    """The hue of each element from the art under it; one with no hue takes the nearest hued element's within NEIGHBOUR (its own dial), else `fallback`."""
    own = [hue_of(art, g, None) for g in groups]
    centres = [centre(g) for g in groups]
    out = []
    for k, colour in enumerate(own):
        if colour is None:
            near = [(math.hypot(centres[k][0] - centres[j][0], centres[k][1] - centres[j][1]), own[j]) for j in range(len(groups)) if own[j] is not None]
            near = [n for n in near if n[0] <= NEIGHBOUR]
            colour = min(near)[1] if near else fallback
        out.append(colour)
    return out


def draft(art, lit, fallback=ORANGE):
    """The RGBA draft: each element's lit pixels and PAINT_MARGIN round them in the element's colour; (image, [colour of each element])."""
    groups = gauges(lit)
    colours = fold(element_colours(art, groups, fallback))
    image = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
    pixels = image.load()
    for group, colour in zip(groups, colours):
        for x, y in group:
            for dy in range(-PAINT_MARGIN, PAINT_MARGIN + 1):
                for dx in range(-PAINT_MARGIN, PAINT_MARGIN + 1):
                    if 0 <= x + dx < WIDTH and 0 <= y + dy < HEIGHT:
                        pixels[x + dx, y + dy] = colour + (255,)
    return image, colours


def make_user_dir(folder, scale=2):
    """A scratch player folder: the Restomod profile at night, render scale 2 (the wide path the lit dash needs)."""
    with open(os.path.join(folder, "port.cfg"), "w") as f:
        f.write("profile=restomod\n")
    with open(os.path.join(folder, "restomod.cfg"), "w") as f:
        f.write("light_time=night\nrender_scale=%d\n" % scale)


def run_game(game, data, stem, extra_env, shot):
    """One headless cockpit run of the car's dash at night: the picture in `shot`; returns the game's output on failure, else None."""
    env = dict(os.environ)
    env.update({"SDL_VIDEO_DRIVER": "dummy", "SDL_RENDER_DRIVER": "software", "SDL_AUDIO_DRIVER": "dummy", "SDL_AUDIODRIVER": "dummy",
                "TNFS_VIEW": "0", "TNFS_GAUGE_LIGHT": "5"})
    env.update(extra_env)
    with tempfile.TemporaryDirectory(prefix="dash-color-") as user:
        make_user_dir(user)
        cmd = [game, "--data", data, "--user", user] + GAME_ARGS + ["--car", str(CAR_NUMBER[stem]), "--screenshot", shot]
        try:
            run = subprocess.run(cmd, env=env, capture_output=True, text=True, timeout=180)
        except subprocess.TimeoutExpired:
            return "the game did not finish in 180 s"
        if run.returncode != 0 or not os.path.isfile(shot):
            return (run.stdout + run.stderr)[-400:]
    return None


def photo_with(game, data, stem, masks_root):
    """The cockpit photo at night with the dash masks of `masks_root`, as an RGB image at 640 x 480, or None with the game's words in the second value."""
    with tempfile.TemporaryDirectory(prefix="dash-photo-") as tmp:
        shot = os.path.join(tmp, "photo.ppm")
        why = run_game(game, data, stem, {"TNFS_DASH_MASKS": masks_root}, shot)
        if why is not None:
            return None, why
        return Image.open(shot).convert("RGB").resize((WIDTH, HEIGHT), Image.LANCZOS), None


def dump_missing(game, data, dump, stems):
    """The dumps the dump dir lacks, by running the game in each car's cockpit at night."""
    os.makedirs(dump, exist_ok=True)
    for stem in stems:
        if os.path.isfile(os.path.join(dump, stem + ".ppm")):
            continue
        with tempfile.TemporaryDirectory(prefix="dash-shot-") as tmp:
            why = run_game(game, data, stem, {"TNFS_DASH_DUMP": dump}, os.path.join(tmp, "shot.ppm"))
        if why is not None or not os.path.isfile(os.path.join(dump, stem + ".ppm")):
            raise SystemExit("the game wrote no dump of %s: %s" % (stem, why or "no file"))


def sheet(panels):
    image = Image.new("RGB", (WIDTH * len(panels), HEIGHT), (0, 0, 0))
    for k, panel in enumerate(panels):
        image.paste(panel, (k * WIDTH, 0))
    return image


def zoom(photo, lit):
    """The part of the photo the lit pixels cover, enlarged to fill a panel."""
    xs = [p[0] for p in lit]
    ys = [p[1] for p in lit]
    if not xs:
        return Image.new("RGB", (WIDTH, HEIGHT), (0, 0, 0))
    x0, x1, y0, y1 = max(0, min(xs) - 16), min(WIDTH, max(xs) + 17), max(0, min(ys) - 16), min(HEIGHT, max(ys) + 17)
    scale = min(WIDTH / (x1 - x0), HEIGHT / (y1 - y0))
    crop = photo.crop((x0, y0, x1, y1)).resize((max(1, int((x1 - x0) * scale)), max(1, int((y1 - y0) * scale))), Image.LANCZOS)
    panel = Image.new("RGB", (WIDTH, HEIGHT), (0, 0, 0))
    panel.paste(crop, (0, 0))
    return panel


def on_art(art, draft_image):
    """The draft over the dash dimmed: what is coloured and where."""
    base = Image.eval(art, lambda v: v // 3).convert("RGBA")
    base.alpha_composite(draft_image)
    return base.convert("RGB")


def folder_of(masks_dir, stem):
    for name in sorted(os.listdir(masks_dir)):
        if name.lower() == stem.lower() and os.path.isdir(os.path.join(masks_dir, name)):
            return name
    return None


def choose_stems(args, masks_dir):
    if args.stems:
        return [s.upper() for s in args.stems.split(",") if s]
    stems = []
    for name in sorted(os.listdir(masks_dir)):
        folder = os.path.join(masks_dir, name)
        if os.path.isfile(os.path.join(folder, "dash_mask.png")) and (args.all or not os.path.isfile(os.path.join(folder, "dash_color.png"))):
            stems.append(name.upper())
    return stems


def main(argv):
    ap = argparse.ArgumentParser(description="a first dash_color.png per dash, from the dash's art")
    ap.add_argument("dump")
    ap.add_argument("masks")
    ap.add_argument("out")
    ap.add_argument("--game")
    ap.add_argument("--data")
    ap.add_argument("--stems")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--default-colour", default="ff8000", help="RRGGBB a gauge takes when the art under it has no hue (default ff8000, orange)")
    args = ap.parse_args(argv)
    if bool(args.game) != bool(args.data):
        return "--game and --data go together"
    if not os.path.isdir(args.masks):
        return "%s is not a folder" % args.masks
    for place in (args.out, args.dump):
        why = refuse_place(place)
        if why:
            return why
    try:
        fallback = tuple(int(args.default_colour[k:k + 2], 16) for k in (0, 2, 4)) if len(args.default_colour) == 6 else None
    except ValueError:
        fallback = None
    if fallback is None:
        return "--default-colour is six hex digits, such as ff8000"
    stems = choose_stems(args, args.masks)
    if not stems:
        return "no dash to draft in %s (every dash has a dash_color.png: --all or --stems)" % args.masks
    folders = {s: folder_of(args.masks, s) for s in stems}
    missing = [s for s, f in folders.items() if f is None]
    if missing:
        return "no dash_masks folder for %s in %s" % (", ".join(missing), args.masks)
    for stem in stems:   # nothing is written until every file it would write is known to be free
        for name in ("dash_mask.png", "dash_color.png", "sheet.png"):
            path = os.path.join(args.out, folders[stem], name)
            if os.path.exists(path):
                return "%s is there: a draft never goes over a file (choose an empty output folder)" % path
    if args.game:
        dump_missing(args.game, args.data, args.dump, stems)
    psd_tool = load_tool("dash_mask_psd")
    for stem in stems:
        if not os.path.isfile(os.path.join(args.dump, stem + ".ppm")):
            return "%s holds no %s.ppm: dump it with the game (--game and --data do)" % (args.dump, stem)
        art = dash_image(psd_tool, args.dump, stem)
        mask_path = os.path.join(args.masks, folders[stem], "dash_mask.png")
        lit = lit_pixels(mask_path)
        image, colours = draft(art, lit, fallback)
        target = os.path.join(args.out, folders[stem])
        os.makedirs(target, exist_ok=True)
        with open(mask_path, "rb") as src, open(os.path.join(target, "dash_mask.png"), "xb") as dst:
            dst.write(src.read())
        with open(os.path.join(target, "dash_color.png"), "xb") as dst:
            image.save(dst, "PNG")
        panels = [art, on_art(art, image)]
        if args.game:
            photo, why = photo_with(args.game, args.data, stem, os.path.abspath(args.out))
            if photo is None:
                print("%s: no photo (%s)" % (stem, why), file=sys.stderr)
            else:
                panels.extend([photo, zoom(photo, lit)])
        with open(os.path.join(target, "sheet.png"), "xb") as dst:
            sheet(panels).save(dst, "PNG")
        print("%s: %d gauges, colours %s -> %s" % (stem, len(colours), sorted(set("#%02x%02x%02x" % c for c in colours)), target))
    return None


if __name__ == "__main__":
    error = main(sys.argv[1:])
    if error:
        print("dash_color_draft.py: %s" % error, file=sys.stderr)
        sys.exit(1)
