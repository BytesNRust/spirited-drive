#!/usr/bin/env python3
"""A first guess at a track texture's reflective mask, for the `draft` layer of the PSDs (tools/track_mask_psd.py): the owner copies it into the `mask` layer or touches it up.
Never read by the game: only the `mask` layer counts (tools/track_mask_from_psd.py).

`auto_mask(name, rgba)` takes the texture's name (slot_NN for a road texture, scenery_NN for a board) and its RGBA Image and returns an 'L' Image the size of the texture: 255 where it
guesses reflective, 0 elsewhere. A transparent texel is never reflective. By kind:
  road (slot_NN)     only the cat's eyes: the red and orange dots
  green sign         only the white letters and numbers (not the white border), a board whose face is dark green
  sign               the symbol and the border: the dark texels of the head (rows wider than a pole) of a yellow or white sign
  post (<= 18 wide)  the reflective stripe: its orange or red, yellow and near-white texels
  anything else      nothing (a crown, a tree: no sign face)
CANDIDATES lists, per track, the textures worth a draft; track_mask_psd.py writes only those unless it is given --all.
"""
from PIL import Image

DARK = 70          # a texel whose brightest channel is at most this is the symbol or the border of a sign
WHITE = 170        # a texel whose darkest channel is at least this is white (a green sign's letters)
HEAD_WIDTH = 20    # a row with more opaque texels than this belongs to a sign's head, not its pole
POST_WIDTH = 18    # a board this narrow or less is a post
BORDER = 3         # the white frame of a green sign, in texels, is not reflective
FACE_SHARE = 0.3   # a road sign's face (yellow or white) is at least this share of its head: a palm's crown, a tree, a bush are not signs

# the textures worth a draft, by track: (road slots, scenery boards). The owner chose the tracks one at a time, and what the guess marks on the other boards (the vegetation) is no
# business of his; a track with no list gets a draft of every texture (the vegetation still left out by the sign rule's face test).
CANDIDATES = {
    "CL1": ({0, 1, 3, 4, 6, 7, 10}, {0, 1, 2, 3, 4, 5, 8, 11, 12, 13, 16, 18}),
}


def is_candidate(track, name):
    """Whether texture `name` (slot_NN / scenery_NN) of `track` is on the track's list of candidates; true for every texture of a track with no list."""
    if track not in CANDIDATES:
        return True
    slots, boards = CANDIDATES[track]
    kind, _, number = name.partition("_")
    return number.isdigit() and int(number) in (slots if kind == "slot" else boards if kind == "scenery" else ())


def is_eye(r, g, b):
    """The red or orange of a cat's eye (not yellow: the lane paint)."""
    return r > 150 and r - b > 90 and g < r * 0.72 and g > b


def is_stripe(r, g, b):
    """A post's reflective stripe: orange or red, yellow, or near-white."""
    return (r >= 120 and r - b >= 60) or min(r, g, b) >= 190


def head_rows(px, width, height):
    """The rows of a sign's head: the ones with more than HEAD_WIDTH opaque texels."""
    return [y for y in range(height) if sum(1 for x in range(width) if px[x, y][3] != 0) > HEAD_WIDTH]


def face_is_green(px, width, rows):
    """Whether the head's opaque texels are mostly dark green (a green sign)."""
    green = total = 0
    for y in rows:
        for x in range(width):
            r, g, b, a = px[x, y]
            if a == 0:
                continue
            total += 1
            green += g > r + 15 and g > b and max(r, g, b) < 120
    return total > 0 and green * 2 > total


def has_sign_face(px, width, rows):
    """Whether the head is mostly a sign's face: yellow or white (the dark symbol and border count for the rest) and not the green of a crown or a bush."""
    face = total = 0
    for y in rows:
        for x in range(width):
            r, g, b, a = px[x, y]
            if a == 0:
                continue
            total += 1
            face += (r >= 180 and g >= 110 and b <= 90) or min(r, g, b) >= 170
    return total > 0 and face >= total * FACE_SHARE


def auto_mask(name, rgba):
    rgba = rgba.convert("RGBA")
    width, height = rgba.size
    px = rgba.load()
    mask = Image.new("L", rgba.size, 0)
    out = mask.load()
    if name.startswith("slot_"):
        for y in range(height):
            for x in range(width):
                r, g, b, a = px[x, y]
                if a and is_eye(r, g, b):
                    out[x, y] = 255
        return mask
    if not name.startswith("scenery_"):
        return mask
    if width <= POST_WIDTH:
        for y in range(height):
            for x in range(width):
                r, g, b, a = px[x, y]
                if a and is_stripe(r, g, b):
                    out[x, y] = 255
        return mask
    rows = head_rows(px, width, height)
    if not rows:
        return mask
    if face_is_green(px, width, rows):
        left = min(x for y in rows for x in range(width) if px[x, y][3] != 0)
        right = max(x for y in rows for x in range(width) if px[x, y][3] != 0)
        top, bottom = rows[0], rows[-1]
        for y in rows:
            for x in range(width):
                r, g, b, a = px[x, y]
                inside = left + BORDER <= x <= right - BORDER and top + BORDER <= y <= bottom - BORDER
                if a and inside and min(r, g, b) >= WHITE:
                    out[x, y] = 255
        return mask
    if width <= 70 and height >= 85 and has_sign_face(px, width, rows):   # a road sign on its pole
        for y in rows:
            for x in range(width):
                r, g, b, a = px[x, y]
                if a and max(r, g, b) <= DARK:
                    out[x, y] = 255
    return mask
