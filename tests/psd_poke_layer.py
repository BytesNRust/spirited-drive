#!/usr/bin/env python3
"""Paints a square into the `mask` layer of a layered PSD, in place (the other layers stay byte for byte): a stand-in for an artist saving the PSD again.
usage: psd_poke_layer.py FILE.psd VALUE   (the square is the top left quarter of the canvas; VALUE 0..255 goes into every channel of the layer)"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
import psd_edit  # noqa: E402


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    path, value = sys.argv[1], int(sys.argv[2])
    psd = psd_edit.read_psd(path)
    w, h = psd.width, psd.height
    for i, layer in enumerate(psd.layers):
        if layer.name != "mask":
            continue
        planes = {}
        for cid, _ in layer.channels:
            plane = bytearray(layer.plane(cid, w, h) or bytes(w * h))
            for y in range(max(1, h // 2)):
                for x in range(max(1, w // 2)):
                    plane[y * w + x] = value
            planes[cid] = bytes(plane)
        psd.layers[i] = layer.replace(planes, w, h)
        psd_edit.write_psd(path, psd)
        return
    sys.exit("no layer named mask in %s" % path)


main()
