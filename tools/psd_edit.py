#!/usr/bin/env python3
"""Rewrite some layers of an existing 8-bit RGB PSD and leave every other layer as it is (byte for byte), for tools/dash_mask_psd.py --update.

The owner paints the "mask" (and "color") layers of a dash PSD; when the dash layer under them changes, those layers must survive. read_psd() splits a PSD
(version 1, any layer compression the game's own writer and Photoshop use: raw or PackBits RLE) into its layers; Layer.replace() puts new pixels into one of them;
write_psd() writes the file again, the untouched layers' records and channel data copied verbatim, and the merged image recomposed (normal blending, opacity,
hidden layers left out). Needs Pillow only for the merged image's final assembly. No other dependency.
"""
import struct


class Layer:
    """One layer: its record (bounds, channels, blending, extra data) and its channel data, as they are in the file."""

    def __init__(self, bounds, channels, blend, opacity, clip, flags, extra, data):
        self.bounds = bounds      # (top, left, bottom, right)
        self.channels = channels  # [(channel id, length)] the length counts the compression word
        self.blend, self.opacity, self.clip, self.flags = blend, opacity, clip, flags
        self.extra = extra        # the record's extra data, verbatim
        self.data = data          # one blob per channel: compression word + data, verbatim

    @property
    def name(self):
        return layer_name(self.extra)

    @property
    def hidden(self):
        return bool(self.flags & 2)

    def plane(self, channel_id, width, height):
        """The channel's pixels on the canvas (width x height bytes, 0 where the layer has none), or None if the layer has no such channel."""
        for (cid, _), blob in zip(self.channels, self.data):
            if cid == channel_id:
                top, left, bottom, right = self.bounds
                w, h = right - left, bottom - top
                rows = decode_channel(blob, w, h)
                canvas = bytearray(width * height)
                for y in range(h):
                    cy = top + y
                    if 0 <= cy < height:
                        x0, x1 = max(0, left), min(width, left + w)
                        if x1 > x0:
                            canvas[cy * width + x0: cy * width + x1] = rows[y * w + (x0 - left): y * w + (x1 - left)]
                return bytes(canvas)
        return None

    def replace(self, planes, width, height, hidden=None):
        """A copy of this layer holding `planes` {channel id: width*height bytes} over the whole canvas, raw. Blending, opacity and the rest of the record stay."""
        ids = [cid for cid, _ in self.channels]
        for cid in ids:
            if cid not in planes:
                raise ValueError("no pixels for channel %d of layer %r" % (cid, self.name))
        flags = self.flags if hidden is None else (self.flags | 2 if hidden else self.flags & ~2)
        return Layer((0, 0, height, width), [(cid, 2 + width * height) for cid in ids], self.blend, self.opacity, self.clip, flags, self.extra,
                     [struct.pack(">H", 0) + bytes(planes[cid]) for cid in ids])

    def record(self):
        top, left, bottom, right = self.bounds
        rec = struct.pack(">iiiiH", top, left, bottom, right, len(self.channels))
        for cid, length in self.channels:
            rec += struct.pack(">hI", cid, length)
        rec += b"8BIM" + self.blend + bytes([self.opacity, self.clip, self.flags, 0])
        return rec + struct.pack(">I", len(self.extra)) + self.extra


def layer_name(extra):
    """The layer's name: the Unicode 'luni' block when there is one, else the Pascal string."""
    mask_len = struct.unpack(">I", extra[:4])[0]
    p = 4 + mask_len
    ranges_len = struct.unpack(">I", extra[p:p + 4])[0]
    p += 4 + ranges_len
    n = extra[p]
    ascii_name = extra[p + 1: p + 1 + n].decode("latin1")
    p += 1 + n
    p += (-(1 + n)) % 4
    while p + 12 <= len(extra):
        key, length = extra[p + 4: p + 8], struct.unpack(">I", extra[p + 8: p + 12])[0]
        if key == b"luni" and length >= 4:
            count = struct.unpack(">I", extra[p + 12: p + 16])[0]
            return extra[p + 16: p + 16 + 2 * count].decode("utf-16-be")
        p += 12 + length + (length % 2)
    return ascii_name


def decode_channel(blob, width, height):
    """A channel's rows (width*height bytes) from its blob: the compression word, then raw rows or PackBits RLE."""
    comp = struct.unpack(">H", blob[:2])[0]
    if comp == 0:
        return blob[2: 2 + width * height]
    if comp != 1:
        raise ValueError("layer channel compression %d is not supported (raw and RLE are)" % comp)
    counts = struct.unpack(">%dH" % height, blob[2: 2 + 2 * height])
    p = 2 + 2 * height
    out = bytearray()
    for n in counts:
        row, end = bytearray(), p + n
        while p < end:
            c = blob[p]
            p += 1
            if c < 128:
                row += blob[p: p + c + 1]
                p += c + 1
            elif c > 128:
                row += blob[p: p + 1] * (257 - c)
                p += 1
        out += row[:width].ljust(width, b"\0")
    return bytes(out)


class Psd:
    def __init__(self, header, before_layers, layers, tail, width, height):
        self.header, self.before_layers, self.layers, self.tail = header, before_layers, layers, tail
        self.width, self.height = width, height


def read_psd(path):
    d = open(path, "rb").read()
    sig, ver, _, chans, height, width, depth, mode = struct.unpack(">4sH6sHIIHH", d[:26])
    if sig != b"8BPS" or ver != 1 or depth != 8 or mode != 3:
        raise ValueError("%s is not an 8-bit RGB version 1 PSD" % path)
    p = 26
    for _ in range(2):   # the colour mode data, the image resources
        p += 4 + struct.unpack(">I", d[p: p + 4])[0]
    before = d[26:p]
    section_len = struct.unpack(">I", d[p: p + 4])[0]
    sec = d[p + 4: p + 4 + section_len]
    info_len = struct.unpack(">I", sec[:4])[0]
    info = sec[4: 4 + info_len]
    tail = sec[4 + info_len:]
    count = abs(struct.unpack(">h", info[:2])[0])
    q = 2
    heads = []
    for _ in range(count):
        top, left, bottom, right, nc = struct.unpack(">iiiiH", info[q: q + 18])
        q += 18
        chs = []
        for _ in range(nc):
            chs.append(struct.unpack(">hI", info[q: q + 6]))
            q += 6
        blend, opacity, clip, flags = struct.unpack(">4sBBB", info[q + 4: q + 11])
        q += 12
        elen = struct.unpack(">I", info[q: q + 4])[0]
        extra = info[q + 4: q + 4 + elen]
        q += 4 + elen
        heads.append(((top, left, bottom, right), chs, blend, opacity, clip, flags, extra))
    layers = []
    for bounds, chs, blend, opacity, clip, flags, extra in heads:
        blobs = []
        for _, length in chs:
            blobs.append(info[q: q + length])
            q += length
        layers.append(Layer(bounds, chs, blend, opacity, clip, flags, extra, blobs))
    return Psd(d[:26], before, layers, tail, width, height)


def merged_planes(psd):
    """The merged image's three planes: the visible layers composed bottom to top (normal blending, layer opacity, alpha), over black."""
    from PIL import Image
    w, h = psd.width, psd.height
    img = Image.new("RGB", (w, h), (0, 0, 0))
    for layer in psd.layers:
        if layer.hidden:
            continue
        planes = [layer.plane(c, w, h) for c in (0, 1, 2)]
        if None in planes:
            continue
        alpha = layer.plane(-1, w, h)
        a = Image.frombytes("L", (w, h), alpha if alpha is not None else b"\xff" * (w * h))
        a = a.point(lambda v, o=layer.opacity: v * o // 255)
        src = Image.merge("RGB", [Image.frombytes("L", (w, h), p) for p in planes])
        img = Image.composite(src, img, a)
    return b"".join(band.tobytes() for band in img.split())


def write_psd(path, psd):
    """The file for `psd`: its layers' records and channel data as they are, the merged image recomposed (raw)."""
    w, h = psd.width, psd.height
    records = b"".join(layer.record() for layer in psd.layers)
    data = b"".join(b"".join(layer.data) for layer in psd.layers)
    info = struct.pack(">h", len(psd.layers)) + records + data
    if len(info) % 2:
        info += b"\0"
    section = struct.pack(">I", len(info)) + info + psd.tail
    head = bytearray(psd.header)
    head[12:14] = struct.pack(">H", 3)   # three channels: the merged image has no alpha
    out = bytes(head) + psd.before_layers + struct.pack(">I", len(section)) + section + struct.pack(">H", 0) + merged_planes(psd)
    with open(path, "wb") as f:
        f.write(out)
