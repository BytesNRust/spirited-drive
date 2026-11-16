"""menu_walk.sh's frames (the game's capture PPMs): which are a menu's picture.

  frame_pick.py --one-colour FRAME   exit 0 when every pixel of FRAME is one colour (black, a fade's flat grey): no menu's picture; 1 not
  frame_pick.py --fallback DIR       the newest frame of DIR, the newest itself left out (the game is killed while it writes frames, and
                                     the last can be cut short), that is not one colour, walking back; exit 1 when there is none (no
                                     frame, or every frame one colour), the reason on stderr
"""
import os
import sys


def pixels(path):
    """the PPM's pixel bytes (P6, maxval 255, no comments: the game's capture writer)"""
    with open(path, "rb") as f:
        data = f.read()
    parts = data.split(b"\n", 3)
    if len(parts) < 4 or parts[0] != b"P6":
        raise ValueError("%s is not the game's PPM" % path)
    return parts[3]


def one_colour(path):
    p = pixels(path)
    n = len(p) // 3
    return n > 0 and p[: n * 3] == p[:3] * n


def fallback(folder):
    frames = sorted(f for f in os.listdir(folder) if f.endswith(".ppm"))
    if not frames:
        print("frame_pick: no frame in %s" % folder, file=sys.stderr)
        return 1
    for name in reversed(frames[:-1] if len(frames) > 1 else frames):
        if not one_colour(os.path.join(folder, name)):
            print(name)
            return 0
    print("frame_pick: every frame in %s is one colour (black, or a fade's)" % folder, file=sys.stderr)
    return 1


def main(argv):
    if len(argv) == 3 and argv[1] == "--one-colour":
        return 0 if one_colour(argv[2]) else 1
    if len(argv) == 3 and argv[1] == "--fallback":
        return fallback(argv[2])
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
