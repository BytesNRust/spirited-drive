"""tools/frame_pick.py on folders of fake frames (the game's capture PPMs, menu_walk.sh's cap/): a one-colour frame (black, or a fade's
flat grey) is never a menu's picture; the fallback walks back from the frame before the newest (the newest can be cut short) to the
first that is not one colour, and fails when every frame is."""
import os
import subprocess
import sys
import tempfile

TOOL = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools", "frame_pick.py")
W, H = 8, 4
failures = 0


def check(cond, what):
    global failures
    if not cond:
        failures += 1
        print("FAIL:", what)


def ppm(path, pixels, cut=0):
    data = b"P6\n%d %d\n255\n" % (W, H) + bytes(pixels)
    with open(path, "wb") as f:
        f.write(data[: len(data) - cut] if cut else data)


def flat(rgb):
    return list(rgb) * (W * H)


def picture(seed):
    return [(seed * 7 + i * 13) % 256 for i in range(W * H * 3)]


def run(*args):
    r = subprocess.run([sys.executable, "-I", TOOL, *args], capture_output=True, text=True)
    return r.returncode, r.stdout.strip(), r.stderr.strip()


with tempfile.TemporaryDirectory() as tmp:
    def folder(name, frames):
        d = os.path.join(tmp, name)
        os.makedirs(d)
        for i, (pixels, cut) in enumerate(frames):
            ppm(os.path.join(d, "frame_%05d.ppm" % i), pixels, cut)
        return d

    one = folder("one", [(flat((0, 0, 0)), 0), (flat((40, 40, 40)), 0), (picture(1), 0), (picture(1), 37)])
    for i, want in enumerate([0, 0, 1, 1]):   # black, a flat grey: one colour; a picture, and one cut short, not
        code, _, _ = run("--one-colour", os.path.join(one, "frame_%05d.ppm" % i))
        check(code == (0 if want == 0 else 1), "frame %d: one colour is %s" % (i, want == 0))

    code, out, _ = run("--fallback", one)   # the newest left out: frame 2
    check(code == 0 and out == "frame_00002.ppm", "fallback of a still picture: %s %r" % (code, out))

    fade = folder("fade", [(picture(2), 0), (flat((0, 0, 0)), 0), (flat((0, 0, 0)), 0)])   # caught in a fade to black: back to frame 0
    code, out, _ = run("--fallback", fade)
    check(code == 0 and out == "frame_00000.ppm", "fallback past black frames: %s %r" % (code, out))

    black = folder("black", [(flat((0, 0, 0)), 0)] * 3)
    code, out, err = run("--fallback", black)
    check(code == 1 and out == "" and "one colour" in err, "every frame black: %s %r %r" % (code, out, err))

    empty = folder("empty", [])
    code, out, _ = run("--fallback", empty)
    check(code == 1 and out == "", "no frame: %s %r" % (code, out))

print("frame_pick: %d failures" % failures)
sys.exit(1 if failures else 0)
