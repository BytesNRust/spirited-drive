#!/usr/bin/env python3
"""One-time migration (dep103): starting mask PSDs for the original tracks that have none (TR2, TR5, TR7 by default), one per texture of the track (road slots and scenery boards).
Each PSD has the texture on a locked layer, an empty `mask` layer and a hidden `draft` guess (tools/track_mask_psd.py --all); Studio reads them as "no mask painted" until the owner paints.
A PSD already in the workspace is never overwritten.

usage: track_starter_psds.py <track_mask_draft> <data dir with SIMDATA/ETRACKFM> <workspace tracks dir> [TRACK ...]
Writes <workspace tracks dir>/<TRACK>/art-source/masks/<name>.psd (the folder must exist when the track is in the workspace; it is made when not). Prints one line per track and the totals.
Needs Pillow. The drafts hold the game's pixels and go to a temporary folder that is removed after.
"""
import os
import shutil
import subprocess
import sys
import tempfile

DEFAULT_TRACKS = ["TR2", "TR5", "TR7"]


def main(argv):
    if len(argv) < 4:
        print(__doc__)
        return 2
    draft_tool, cd_data, tracks_dir = argv[1:4]
    tracks = argv[4:] or DEFAULT_TRACKS
    here = os.path.dirname(os.path.abspath(__file__))
    work = tempfile.mkdtemp(prefix="starter_psds_")
    try:
        drafts, psds = os.path.join(work, "drafts"), os.path.join(work, "psd")
        subprocess.run([draft_tool, cd_data, drafts] + tracks, check=True, stdout=subprocess.DEVNULL)
        subprocess.run([sys.executable, os.path.join(here, "track_mask_psd.py"), drafts, psds, "--all"], check=True, stdout=subprocess.DEVNULL)
        written = kept = 0
        for track in tracks:
            src = os.path.join(psds, track)
            if not os.path.isdir(src):
                print("%s: no PSDs made" % track)
                return 1
            dst = os.path.join(tracks_dir, track, "art-source", "masks")
            os.makedirs(dst, exist_ok=True)
            w = k = 0
            for name in sorted(os.listdir(src)):
                if not name.endswith(".psd"):
                    continue
                if os.path.exists(os.path.join(dst, name)):
                    k += 1
                else:
                    shutil.copyfile(os.path.join(src, name), os.path.join(dst, name))
                    w += 1
            print("%s: %d written, %d already there" % (track, w, k))
            written, kept = written + w, kept + k
        print("starter PSDs: %d written, %d kept" % (written, kept))
        return 0
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
