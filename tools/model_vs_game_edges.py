#!/usr/bin/env python3
"""How much of the shine's soft-edge dither the Studio's Model shows, against the game, for the same car
(notes/dev2-model-vs-game-soft-edges-2026-11-23.md has the numbers it gave).

usage: python3 -I tools/model_vs_game_edges.py WORKDIR KIT_DIR [GAME_CAR_INDEX]
  WORKDIR          an empty scratch folder
  KIT_DIR          the car's kit, made by `tnfs-track-editor --export-car NAME --into DIR --cd original --port-data data`
                   (the Diablo: LDIABL, the game's --car 1)
  GAME_CAR_INDEX   the game's --car for the same car (default 1)
Needs build/rel/tnfs, build/rel/tnfs-track-editor and original/; about 15 minutes. Not a ctest.

The measure does not depend on the car's size on screen: the pixels the soft edges change (shine_soft on against off, the same frame) over the pixels
the shine changes (body_shine on against off). It depends on which side of the car is in view (the rear about 2 %, the nose and the sides about 0.9 %),
so the Model and the game are compared from the same side.
"""
import os
import struct
import subprocess
import sys
import zlib

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ENV = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_AUDIO_DRIVER="dummy", SDL_AUDIODRIVER="dummy")
GAME_VIEWS = (4,)               # 4: the rear, close
MODEL_ORBITS = ("0,10,1100", "90,10,1100")   # yaw, pitch, distance: 0 is the rear; the distance puts the car at about the game's size
SCALES = (2, 3)


def read_png(path):
    data = open(path, "rb").read()
    w, h, _, ctype = struct.unpack(">IIBB", data[16:26])
    bpp = 4 if ctype == 6 else 3
    i, idat = 8, b""
    while i < len(data):
        n, t = struct.unpack(">I4s", data[i:i + 8])
        if t == b"IDAT":
            idat += data[i + 8:i + 8 + n]
        i += 12 + n
    raw, stride, rows, prev, p = zlib.decompress(idat), w * bpp, [], bytearray(w * bpp), 0
    for _ in range(h):
        f, line = raw[p], bytearray(raw[p + 1:p + 1 + stride])
        p += 1 + stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1:
                line[x] = (line[x] + a) & 255
            elif f == 2:
                line[x] = (line[x] + b) & 255
            elif f == 3:
                line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(bytes(line))
        prev = line
    return w * h, b"".join(rows), bpp


def read_ppm(path):
    parts = open(path, "rb").read().split(b"\n", 3)
    w, h = map(int, parts[1].split())
    return w * h, parts[3], 3


def differ(a, b, bpp):
    return sum(1 for i in range(0, len(a), bpp) if a[i:i + bpp] != b[i:i + bpp])


def game(work, car, scale, shine, soft, view):
    user = f"{work}/game_user_{scale}_{shine}"
    os.makedirs(user, exist_ok=True)
    cfg = f"profile=restomod\nrender_scale={scale}\nbody_shine={4 if shine else 0}\n"
    for name in ("port.cfg", "restomod.cfg"):
        open(f"{user}/{name}", "w").write(cfg)
    out = f"{work}/game_{scale}_{shine}_{soft}_{view}.ppm"
    env = dict(ENV, TNFS_SHINE_SOFT=str(soft), TNFS_AUTOPILOT="1", TNFS_FRAMES="1", TNFS_WEATHER="clear", TNFS_VIEW=str(view), TNFS_START_NODE="300")
    subprocess.run([f"{REPO}/build/rel/tnfs", "--data", "original", "--user", user, "--drive", "AL1", "--car", str(car), "--racers", "1", "--node", "6", "--screenshot", out],
                   cwd=REPO, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=600, check=True)
    return read_ppm(out)


def model(work, kit, scale, shine, soft, orbit):
    out = f"{work}/model_{scale}_{shine}_{soft}_{orbit.replace(',', '_')}.png"
    subprocess.run([f"{REPO}/build/rel/tnfs-track-editor", "--user", f"{work}/studio_user", "--cd", f"{REPO}/original", "--port-data", f"{REPO}/data", "--workspace", f"{work}/studio_ws",
                    kit, "--render-scale", str(scale), "--soft", "on" if soft else "off", "--shine", "on" if shine else "off", "--orbit", orbit, "--frames", "30",
                    "--steps", "psd-wait:5", "--model-shot", out], cwd=REPO, env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=600, check=True)
    return read_png(out)


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    work, kit = sys.argv[1], sys.argv[2]
    car = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    os.makedirs(work, exist_ok=True)
    print("source  scale  camera         frame_px  shine_changes  soft_changes  soft/shine")
    for scale in SCALES:
        for view in GAME_VIEWS:
            _, off, bpp = game(work, car, scale, 0, 0, view)
            px, on0, _ = game(work, car, scale, 1, 0, view)
            _, on1, _ = game(work, car, scale, 1, 1, view)
            sh, so = differ(off, on0, bpp), differ(on0, on1, bpp)
            print(f"game    {scale}      view {view:<9} {px:>9} {sh:>14} {so:>13}  {100.0 * so / max(sh, 1):.2f} %")
        for orbit in MODEL_ORBITS:
            _, off, bpp = model(work, kit, scale, 0, 0, orbit)
            px, on0, _ = model(work, kit, scale, 1, 0, orbit)
            _, on1, _ = model(work, kit, scale, 1, 1, orbit)
            sh, so = differ(off, on0, bpp), differ(on0, on1, bpp)
            print(f"model   {scale}      {orbit:<14} {px:>9} {sh:>14} {so:>13}  {100.0 * so / max(sh, 1):.2f} %")


main()
