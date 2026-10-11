#!/usr/bin/env python3
"""Shifts every pixel index of an 8-bit indexed PNG by N (mod 256) so the picture changes colour group.
Reads the PNGs tnfs-studio writes (unfiltered rows); anything else is refused.
usage: png_shift.py FILE.png [N]"""
import struct
import sys
import zlib


def chunk(tag, body):
    return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF)


def main(path, shift):
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        sys.exit(f"{path}: not a PNG")
    pos, chunks, idat = 8, [], b""
    while pos < len(data):
        length, tag = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if tag == b"IDAT":
            idat += body
        else:
            chunks.append((tag, body))
    ihdr = next(b for t, b in chunks if t == b"IHDR")
    width, height, depth, color = struct.unpack(">IIBB", ihdr[:10])
    if depth != 8 or color != 3:
        sys.exit(f"{path}: not an 8-bit indexed PNG")
    raw = bytearray(zlib.decompress(idat))
    row = width + 1
    if len(raw) != row * height:
        sys.exit(f"{path}: unexpected image data size")
    for y in range(height):
        if raw[y * row] != 0:
            sys.exit(f"{path}: filtered rows are not supported")
        for x in range(1, row):
            raw[y * row + x] = (raw[y * row + x] + shift) & 0xFF

    out = b"\x89PNG\r\n\x1a\n"
    for tag, body in chunks:
        if tag == b"IEND":
            out += chunk(b"IDAT", zlib.compress(bytes(raw)))
        out += chunk(tag, body)
    open(path, "wb").write(out)


if __name__ == "__main__":
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__)
    main(sys.argv[1], int(sys.argv[2]) if len(sys.argv) == 3 else 32)
