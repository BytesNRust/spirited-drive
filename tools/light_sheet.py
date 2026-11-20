#!/usr/bin/env python3
"""A 2x2 contact sheet of four PPM pictures (tools/light_photo.sh): dawn A and day B on the top row, dusk C and night N under them.

usage: light_sheet.py OUT.png A.ppm B.ppm C.ppm N.ppm [--full]
Each picture is halved (2x2 box average) unless --full is given; a white letter A, B, C or N in its top left corner says which time it is.
Standard library only (no Pillow): a P6 PPM in, an RGB PNG out (also written by `write_png`)."""
import struct
import sys
import zlib

GLYPHS = {   # 5x7, one string a row
    "A": ("01110", "10001", "10001", "11111", "10001", "10001", "10001"),
    "B": ("11110", "10001", "10001", "11110", "10001", "10001", "11110"),
    "C": ("01110", "10001", "10000", "10000", "10000", "10001", "01110"),
    "N": ("10001", "11001", "10101", "10101", "10011", "10001", "10001"),
}
LETTERS = "ABCN"


def read_ppm(path):
    """(width, height, bytearray of RGB rows) of a binary P6 PPM with maxval 255."""
    with open(path, "rb") as f:
        data = f.read()
    fields, at = [], 0
    while len(fields) < 4:
        while data[at:at + 1].isspace():
            at += 1
        if data[at:at + 1] == b"#":
            while data[at:at + 1] != b"\n":
                at += 1
            continue
        start = at
        while not data[at:at + 1].isspace():
            at += 1
        fields.append(data[start:at])
    at += 1   # the single whitespace after maxval
    if fields[0] != b"P6" or fields[3] != b"255":
        raise ValueError("%s: not a P6 PPM with maxval 255" % path)
    width, height = int(fields[1]), int(fields[2])
    pixels = data[at:at + width * height * 3]
    if len(pixels) != width * height * 3:
        raise ValueError("%s: %d bytes of pixels, %dx%d needs %d" % (path, len(pixels), width, height, width * height * 3))
    return width, height, bytearray(pixels)


def halve(width, height, pixels):
    """The picture at half size, each pixel the average of a 2x2 block (an odd last row or column is dropped)."""
    w2, h2 = width // 2, height // 2
    out = bytearray(w2 * h2 * 3)
    stride = width * 3
    for y in range(h2):
        top = pixels[2 * y * stride:(2 * y + 1) * stride]
        bottom = pixels[(2 * y + 1) * stride:(2 * y + 2) * stride]
        row = bytearray(w2 * 3)
        for ch in range(3):
            a, b = top[ch::3][0:2 * w2:2], top[ch::3][1:2 * w2:2]
            c, d = bottom[ch::3][0:2 * w2:2], bottom[ch::3][1:2 * w2:2]
            row[ch::3] = bytes((p + q + r + s + 2) >> 2 for p, q, r, s in zip(a, b, c, d))
        out[y * w2 * 3:(y + 1) * w2 * 3] = row
    return w2, h2, out


def stamp(width, height, pixels, letter, scale=4, margin=6):
    """The letter in white with a black shadow in the top left corner."""
    glyph = GLYPHS[letter]
    for shadow, colour in ((2, (0, 0, 0)), (0, (255, 255, 255))):
        for gy, row in enumerate(glyph):
            for gx, bit in enumerate(row):
                if bit != "1":
                    continue
                for dy in range(scale):
                    for dx in range(scale):
                        x, y = margin + shadow + gx * scale + dx, margin + shadow + gy * scale + dy
                        if x < width and y < height:
                            at = (y * width + x) * 3
                            pixels[at:at + 3] = bytes(colour)


def make_sheet(paths, full=False):
    """(width, height, pixels) of the 2x2 sheet of the four PPM paths, in the order dawn, day, dusk, night."""
    if len(paths) != 4:
        raise ValueError("a sheet takes four pictures, got %d" % len(paths))
    cells = []
    for letter, path in zip(LETTERS, paths):
        width, height, pixels = read_ppm(path)
        if not full:
            width, height, pixels = halve(width, height, pixels)
        stamp(width, height, pixels, letter, scale=8 if full else 4)
        cells.append((width, height, pixels))
    if len({(w, h) for w, h, _ in cells}) != 1:
        raise ValueError("the four pictures are not the same size: %s" % [(w, h) for w, h, _ in cells])
    width, height = cells[0][0], cells[0][1]
    sheet = bytearray(2 * width * 2 * height * 3)
    row_bytes = width * 3
    for i, (_, _, pixels) in enumerate(cells):
        x0, y0 = (i % 2) * width, (i // 2) * height
        for y in range(height):
            at = ((y0 + y) * 2 * width + x0) * 3
            sheet[at:at + row_bytes] = pixels[y * row_bytes:(y + 1) * row_bytes]
    return 2 * width, 2 * height, sheet


def write_png(path, width, height, pixels):
    """An RGB PNG, filter 0 on every row."""
    def chunk(tag, body):
        return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF)
    stride = width * 3
    raw = b"".join(b"\x00" + bytes(pixels[y * stride:(y + 1) * stride]) for y in range(height))
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
                chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def main(argv):
    args = [a for a in argv[1:] if a != "--full"]
    if len(args) != 5:
        sys.stderr.write(__doc__ + "\n")
        return 2
    try:
        width, height, pixels = make_sheet(args[1:], full="--full" in argv)
        write_png(args[0], width, height, pixels)
    except (OSError, ValueError) as err:
        sys.stderr.write("light_sheet: %s\n" % err)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
