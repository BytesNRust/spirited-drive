#!/usr/bin/env python3
"""Writes the small made-up seeds kept in tests/fuzz/<reader>/seed_*: every byte written here, none from the CD.
usage: python3 -I fuzz/seeds.py <repository>   (docs/fuzzing.md)"""
import os
import struct
import sys


def u16(v): return struct.pack('<H', v & 0xffff)
def u24(v): return struct.pack('<I', v)[:3]
def u32(v): return struct.pack('<I', v & 0xffffffff)
def be32(v): return struct.pack('>I', v)


def fsh():
    """SHPI with "!pal" (type 0x22) and "bgnd" (4x2 type 0x7B), as tests/test_fsh.c builds it"""
    pal_at = 16 + 2 * 8
    bmp_at = pal_at + 16 + 768
    size = bmp_at + 16 + 8
    head = b'SHPI' + u32(size) + u32(2) + b'LN32' + b'!pal' + u32(pal_at) + b'bgnd' + u32(bmp_at)
    pal = bytes([0x22]) + u24(0) + u16(256) + u16(3) + bytes(8) + bytes(i % 64 for i in range(768))
    bmp = bytes([0x7B]) + u24(16 + 8) + u16(4) + u16(2) + u16(-2) + u16(-3) + u16(10) + u16(20) + bytes(range(1, 9))
    return head + pal + bmp


def ffn():
    """two glyphs over a 4x2 4-bit atlas, as tests/test_ffn.c builds it"""
    atlas_at = 0x20 + 2 * 11
    head = bytearray(0x20)
    head[0:4] = b'FNTF'
    head[8] = 100
    head[0x0A] = 2
    head[0x12] = 20
    head[0x14] = 0x20
    head[0x1C] = atlas_at
    records = bytes([0x20, 0, 1, 1, 0, 0, 0, 0, 5, 0, 16]) + bytes([ord('A'), 0, 3, 2, 1, 0, 0, 0, 4, 1, 6])
    atlas = bytearray(16)
    atlas[0], atlas[4], atlas[6] = 0x7A, 4, 2
    return bytes(head) + records + bytes(atlas) + bytes([0x0F, 0x8A, 0x01, 0x23])


def wav(rate=22050, channels=1, frames=64):
    data = b''.join(u16((i * 900) % 30000 - 15000) for i in range(frames * channels))
    return (b'RIFF' + u32(36 + len(data)) + b'WAVEfmt ' + u32(16) + u16(1) + u16(channels) + u32(rate) +
            u32(rate * 2 * channels) + u16(2 * channels) + u16(16) + b'data' + u32(len(data)) + data)


def shpp():
    """an SHPP directory of four 4bpp textures hrz0..hrz3 (2x1 each, a 16-colour CLUT), as tests/test_psx_bigf.c"""
    tex_len, head, stride = 66, 16, 8
    names = [b'hrz0', b'hrz1', b'hrz2', b'hrz3']
    dir_len = head + stride * len(names)
    out = bytearray(b'SHPP' + u32(0) + u32(len(names)) + b'GIMX')
    for i, n in enumerate(names):
        out += n + u32(dir_len + tex_len * i)
    for i in range(len(names)):
        tex = bytearray(tex_len)
        tex[0], tex[1:4], tex[4:6], tex[6:8] = 0x40, u24(18), u16(2), u16(1)
        tex[16] = 0x21
        clut = 18
        tex[clut], tex[clut + 1:clut + 4], tex[clut + 4:clut + 6], tex[clut + 6:clut + 8] = 0x23, u24(48), u16(16), u16(1)
        tex[clut + 16 + 2:clut + 16 + 4] = u16(0x001f + i)
        tex[clut + 16 + 4:clut + 16 + 6] = u16(0x7c00)
        out += tex
    return bytes(out)


def bigf(name, member):
    entry = be32(0) + be32(len(member)) + name + b'\0'
    at = 16 + len(entry)
    entry = be32(at) + be32(len(member)) + name + b'\0'
    return b'BIGF' + be32(at + len(member)) + be32(1) + be32(0) + entry + member


LGS = (b"/* Depth ceuing definition X0,Y0, X1,Y1, X2,Y2, X3,Y3 */\r0,95  10,99  75,37  100,28\r"
       b"/* Dynamic channel system (start rgb) */\r109,95,52\r/* Dynamic channel system (end rgb) */\r109,95,52,\r"
       b"/* Number of light context */\r2\r/* context 1 */\r14,14  64  111,111,111  0,0,0\r20,40 96 10,20,30 200,100,50\r")
INF = (b"# vert offset near\r\n0.5\r\n# vert offset far\r\n0.25\r\n# zoffset near\r\n1\r\n# zoffset far\r\n2\r\n"
       b"# height near\r\n3\r\n# height far\r\n4\r\n# radius near\r\n5\r\n# radius far\r\n6\r\n"
       b"# colour top\r\n48 42 26\r\n# colour bottom\r\n84 69 40\r\n# dithering flag\r\n1\r\n# ratio of the blend\r\n32\r\n"
       b"# ring y offset\r\n-12\r\n# ring height\r\n100\r\n")
RULES = b"# node_from node_to point_from point_to value\n0 10 0 10 128\n20 40 2 8 255\n100 90 0 0 0\n"


def main():
    repo = sys.argv[1]
    seeds = {
        'fsh': {'seed_two_entries': fsh()},
        'shape_bank': {'seed_two_entries': fsh()},
        'ffn': {'seed_two_glyphs': ffn()},
        'wav': {'seed_mono': wav(), 'seed_stereo': wav(44100, 2, 32)},
        'psx_bigf': {'seed_sky_shpp': shpp(), 'seed_bigf': bigf(b'a.psh', shpp())},
        'validate': {'seed_fsh': bytes([7]) + fsh()},
        'psx_light': {'seed_lgs': bytes([0]) + LGS, 'seed_inf': bytes([2]) + INF, 'seed_panorama': bytes([3]) + shpp()},
        'terrain_rules': {'seed_rules': RULES},
    }
    for reader, files in seeds.items():
        folder = os.path.join(repo, 'tests', 'fuzz', reader)
        os.makedirs(folder, exist_ok=True)
        for name, data in files.items():
            with open(os.path.join(folder, name), 'wb') as f:
                f.write(data)


if __name__ == '__main__':
    main()
