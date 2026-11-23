"""A small stand-in for the part of Pillow the UI kit checks use, with the python standard library only (zlib, struct): the repo's CI and gate
have no Pillow. Image.open(path) reads a PNG (8-bit grey, grey+alpha, RGB, RGBA, or indexed) or a P6 PPM; .convert("RGB") (the same picture), .size,
.load() (px[x, y] is an (r, g, b) tuple), .getpixel((x, y)), and save_png(path) writes an RGB PNG."""
import struct
import zlib


class _Picture:
    def __init__(self, w, h, rows):
        self.size = (w, h)
        self._rows = rows          # h rows of w*3 bytes

    def convert(self, mode):
        if mode != "RGB":
            raise ValueError("only RGB")
        return self

    def getpixel(self, xy):
        x, y = xy
        i = x * 3
        r = self._rows[y]
        return (r[i], r[i + 1], r[i + 2])

    def load(self):
        return self

    __getitem__ = lambda self, xy: self.getpixel(xy)

    def save_png(self, path):
        w, h = self.size
        raw = b"".join(b"\x00" + bytes(r) for r in self._rows)
        def chunk(t, d):
            c = t + d
            return struct.pack(">I", len(d)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)
        with open(path, "wb") as f:
            f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def _paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    return a if pa <= pb and pa <= pc else (b if pb <= pc else c)


def _read_png(d):
    pos, idat, plte, w = 8, b"", b"", 0
    while pos < len(d):
        n, t = struct.unpack(">I4s", d[pos:pos + 8])
        body = d[pos + 8:pos + 8 + n]
        if t == b"IHDR":
            w, h, depth, ctype, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if depth != 8 or interlace or ctype not in (0, 2, 3, 4, 6):
                raise ValueError("PNG kind not read (8-bit, not interlaced)")
        elif t == b"PLTE":
            plte = body
        elif t == b"IDAT":
            idat += body
        pos += 12 + n
    chans = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ctype]
    stride = w * chans
    raw = zlib.decompress(idat)
    prev = bytearray(stride)
    rows = []
    for y in range(h):
        f = raw[y * (stride + 1)]
        cur = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = cur[i - chans] if i >= chans else 0
            b = prev[i]
            c = prev[i - chans] if i >= chans else 0
            if f == 1: cur[i] = (cur[i] + a) & 255
            elif f == 2: cur[i] = (cur[i] + b) & 255
            elif f == 3: cur[i] = (cur[i] + (a + b) // 2) & 255
            elif f == 4: cur[i] = (cur[i] + _paeth(a, b, c)) & 255
        prev = cur
        out = bytearray()
        for x in range(w):
            p = cur[x * chans:(x + 1) * chans]
            if ctype in (0, 4): out += bytes((p[0], p[0], p[0]))
            elif ctype == 3: out += plte[p[0] * 3:p[0] * 3 + 3]
            else: out += bytes(p[:3])
        rows.append(bytes(out))
    return _Picture(w, h, rows)


def _read_ppm(d):
    parts = d.split(b"\n", 3)
    if len(parts) < 4 or parts[0] != b"P6":
        raise ValueError("not a P6 PPM")
    w, h = (int(v) for v in parts[1].split())
    px = parts[3]
    return _Picture(w, h, [px[y * w * 3:(y + 1) * w * 3] for y in range(h)])


class Image:
    @staticmethod
    def open(path):
        with open(path, "rb") as f:
            d = f.read()
        return _read_png(d) if d[:8] == b"\x89PNG\r\n\x1a\n" else _read_ppm(d)
