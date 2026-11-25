#!/usr/bin/env python3
"""Photo at node and Test drive give the game the same sun and the same light, on the 16 CD tracks x 4 moments (point 15 of notes/anuncio-estado-2026-11-25.md).

For each of the 64 (track, moment) pairs the real game runs twice, once with the command and environment the Studio builds for its photo and once with the test drive's
(tests/light_command_print.c prints them), headless, one frame at node 300, with the same player files so only what the Studio's commands change can differ. Both must give
  * the same `sun shadow:` line (azimuth, elevation, shadow, reach) and the same `track light:` file, and
  * the same picture in the lower half of the frame (road and car; the drive's editor overlay covers the top).
The photo's race has rivals and the drive's is a time trial: `--type 0` is added to the photo's arguments here, rivals are not light.
usage: editor_photo_drive_same_light.py <tnfs> <original dir> <light_command_print>      Exit 77 (skipped) without the CD's data. About 3-4 minutes of CPU.
"""
import os
import subprocess
import sys
import tempfile

TRACKS = ["AL1", "AL2", "AL3", "CL1", "CL2", "CL3", "CY1", "CY2", "CY3", "TR1", "TR2", "TR3", "TR4", "TR5", "TR6", "TR7"]
MOMENTS = [("A", "dawn"), ("B", "day"), ("C", "dusk"), ("N", "night")]
NODE = 300
LOOK = ("profile=restomod\nfullscreen=0\nrender_scale=1\nlamp_halos=1\nshine_soft=1\nreflectors=1\nheadlight_beams=1\nsun_shadow=1\nbody_shine=4\ncar_colours=1\n"
        "car_soft_shadow=1\ntexture_filter=1\nlight_effects=1\nlamp_flares=1\nperspective=1\nfog=1\nfog_sample=1\nsky_fill=1\nsky_fill_below=1\npolice_glow=1\ntraffic_halo=1\n")


def ppm(path):
    data = open(path, "rb").read()
    fields, at = [], 0
    while len(fields) < 4:
        while data[at:at + 1].isspace():
            at += 1
        start = at
        while not data[at:at + 1].isspace():
            at += 1
        fields.append(data[start:at])
    return int(fields[1]), int(fields[2]), data[at + 1:]


def lines_of(text, prefixes):
    return sorted({l for l in text.splitlines() if any(l.startswith(p) for p in prefixes)})


def run(kind, track, letter, word, game, original, printer, work):
    user = os.path.join(work, "user")
    mods = os.path.join(work, "mods")
    os.makedirs(user, exist_ok=True)
    os.makedirs(mods, exist_ok=True)
    shot = os.path.join(work, "%s_%s_%s.ppm" % (track, word, kind))
    out = subprocess.run([printer, kind, game, original, user, mods, track, letter, str(NODE), shot], capture_output=True, text=True)
    if out.returncode != 0:
        return None, "the command was not built: " + out.stderr.strip()
    argv = [l[2:] for l in out.stdout.splitlines() if l.startswith("A ")]
    env = dict(l[2:].split("=", 1) for l in out.stdout.splitlines() if l.startswith("E "))
    if kind == "photo":
        argv += ["--type", "0"]
    else:
        argv += ["--node", "4", "--screenshot", shot]   # the drive's window is not wanted here: its frame, headless, after the same ticks
        env.update({"TNFS_AUTOPILOT": "1", "TNFS_FRAMES": "1", "SDL_VIDEO_DRIVER": "dummy", "SDL_RENDER_DRIVER": "software", "SDL_AUDIO_DRIVER": "dummy", "SDL_AUDIODRIVER": "dummy"})
    env.update({"TNFS_VERBOSE": "1", "SDL_RENDER_DRIVER": "software", "SDL_AUDIODRIVER": "dummy"})
    for key in ("PATH", "HOME", "TMPDIR"):
        if key in os.environ:
            env.setdefault(key, os.environ[key])
    with open(os.path.join(user, "port.cfg"), "w") as f:
        f.write("profile=restomod\nfullscreen=0\n")
    with open(os.path.join(user, "restomod.cfg"), "w") as f:
        f.write(LOOK + "light_time=%s\n" % word)
    try:
        done = subprocess.run(argv, env=env, cwd=work, capture_output=True, text=True, timeout=120)
    except subprocess.TimeoutExpired:
        return None, "the game did not end in 120 s"
    if done.returncode != 0 or not os.path.exists(shot):
        return None, "the game ended with status %d and %s" % (done.returncode, "a picture" if os.path.exists(shot) else "no picture")
    log = open(os.path.join(user, "spirited-drive.log"), errors="replace").read() if os.path.exists(os.path.join(user, "spirited-drive.log")) else ""
    return (lines_of(done.stdout + done.stderr + log, ("sun shadow:", "track light:")), ppm(shot)), ""


def main():
    game, original, printer = sys.argv[1:4]
    if not os.path.isdir(original):
        print("skipped: no " + original)
        return 77
    failures = 0
    with tempfile.TemporaryDirectory(prefix="tnfs_photo_drive_") as work:
        for track in TRACKS:
            for letter, word in MOMENTS:
                p, why_p = run("photo", track, letter, word, game, original, printer, work)
                d, why_d = run("drive", track, letter, word, game, original, printer, work)
                if p is None or d is None:
                    print("FAIL %s %s: photo %s / drive %s" % (track, word, why_p or "ok", why_d or "ok"))
                    failures += 1
                    continue
                (lp, (wp, hp, pp)), (ld, (wd, hd, pd)) = p, d
                if not lp or not any(l.startswith("sun shadow:") for l in lp):
                    print("FAIL %s %s: the photo's run logged no sun line: %s" % (track, word, lp))
                    failures += 1
                elif lp != ld:
                    print("FAIL %s %s: the sun or the light file differs:\n  photo %s\n  drive %s" % (track, word, lp, ld))
                    failures += 1
                if (wp, hp) != (wd, hd):
                    print("FAIL %s %s: the frames are %dx%d and %dx%d" % (track, word, wp, hp, wd, hd))
                    failures += 1
                    continue
                low = (hp // 2) * wp * 3
                if pp[low:] != pd[low:]:
                    same = sum(1 for i in range(low, wp * hp * 3, 3) if pp[i:i + 3] == pd[i:i + 3])
                    print("FAIL %s %s: the lower half of the frame differs (%.2f %% of its pixels equal)" % (track, word, 100.0 * same / (wp * (hp - hp // 2))))
                    failures += 1
    print("photo vs drive: %d pairs, %d failures" % (len(TRACKS) * len(MOMENTS), failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
