#!/usr/bin/env python3
"""tools/masks_status.py: what is left of the original cars' masks and dashboards. A fake art folder and a fake data folder (stdlib only)."""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = os.path.join(HERE, "..", "tools", "masks_status.py")
failures = 0


def check(cond, what):
    global failures
    if not cond:
        failures += 1
        print("FAIL:", what)


def touch(path, mtime=None, data=b"x"):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)
    if mtime is not None:
        os.utime(path, (mtime, mtime))


def run(art, data, *extra):
    return subprocess.run([sys.executable, "-I", TOOL, "--art", art, "--data", data, *extra], capture_output=True, text=True)


def row(out, car):
    for line in out.splitlines():
        if line.split() and line.split()[0].lower() == car.lower():
            return line
    return None


with tempfile.TemporaryDirectory() as root:
    art, data = os.path.join(root, "art"), os.path.join(root, "data")
    # P911: every PSD has its PNG, a dash with mask and colour: done
    for tex in ("frnt", "rear", "frnt_lo"):
        touch(os.path.join(art, "P911", tex + ".psd"), 1000)
        touch(os.path.join(data, "car_masks", "P911", tex + "_mask.png"), 2000)
    touch(os.path.join(art, "P911", "dash.psd"), 1000)
    touch(os.path.join(art, "P911", "frnt.before-1603.psd"), 500)   # a backup is not a texture
    touch(os.path.join(data, "dash_masks", "P911", "dash_mask.png"), 2000)
    touch(os.path.join(data, "dash_masks", "P911", "dash_color.png"), 2000)
    # TSupra: the PSD is newer than its PNG (stale), one PSD has no PNG, a dash with a mask but no colour; the data folder spells it TSUPRA
    touch(os.path.join(art, "TSupra", "side.psd"), 3000)
    touch(os.path.join(art, "TSupra", "rear.psd"), 1000)
    touch(os.path.join(data, "car_masks", "TSupra", "side_mask.png"), 2000)
    touch(os.path.join(art, "TSupra", "dash.psd"), 1000)
    touch(os.path.join(data, "dash_masks", "TSUPRA", "dash_mask.png"), 2000)
    # WAGON: a traffic car, no dash: a dash is not asked of it; its mask exists with no PSD
    touch(os.path.join(art, "WAGON", "side.psd"), 1000)
    touch(os.path.join(data, "car_masks", "Wagon", "side_mask.png"), 2000)
    # CRX: data only, no PSD at all
    touch(os.path.join(data, "car_masks", "CRX", "side_mask.png"), 2000)

    r = run(art, data)
    check(r.returncode == 0, "exit 0: " + r.stderr)
    p911 = row(r.stdout, "P911")
    check(p911 is not None and "done" in p911, "P911 is done: %r" % p911)
    tsu = row(r.stdout, "TSupra")
    check(tsu is not None and "stale: side" in tsu and "no mask: rear" in tsu and "dash colour" in tsu, "TSupra todo: %r" % tsu)
    check(sum(1 for l in r.stdout.splitlines() if l.split() and l.split()[0].lower() == "tsupra") == 1, "one row for TSupra / TSUPRA")
    check(p911 is not None and " 3 PSD  3 PNG" in p911, "the counts of P911: %r" % p911)
    wag = row(r.stdout, "Wagon")
    check(wag is not None and "done" in wag and "dash" not in wag.lower(), "WAGON (traffic, no dash PSD) is done: %r" % wag)
    crx = row(r.stdout, "CRX")
    check(crx is not None and "no PSD" in crx, "CRX has no PSD: %r" % crx)
    check("frnt.before" not in r.stdout, "backups are not listed")
    check(r.stdout.rstrip().splitlines()[-1] == "done 2 of 4 cars", "a last line counts: %r" % r.stdout.splitlines()[-1:])

    # a drivable car with nothing of its dash; names that differ only in case (Frnt.PSD / frnt_mask.png); a stray folder; a folder where a file is expected
    touch(os.path.join(art, "ANSX", "Frnt.PSD"), 1000)
    touch(os.path.join(data, "car_masks", "ANSX", "frnt_mask.png"), 2000)
    os.makedirs(os.path.join(art, "ANSX", "rear.psd"))
    os.makedirs(os.path.join(art, "tracks", "CL1"))
    touch(os.path.join(art, "tracks", "CL1", "scenery_00.psd"), 1000)
    os.makedirs(os.path.join(art, "_old"))
    touch(os.path.join(art, "_old", "x.psd"), 1000)
    r2 = run(art, data)
    ansx = row(r2.stdout, "ANSX")
    check(ansx is not None and "no mask" not in ansx and "no dash PSD" in ansx and "dash mask" in ansx and "dash colour" in ansx, "ANSX: %r" % ansx)
    check("tracks" not in r2.stdout, "a folder of other PSDs is not a car")
    check("_old" not in r2.stdout, "a stray folder is not a car")
    touch(os.path.join(art, "TSupra", "dash.psd"), 9000)   # newer than TSUPRA's dash mask
    check("stale: dash" in (row(run(art, data).stdout, "TSupra") or ""), "a dash PSD newer than its mask is stale")
    os.utime(os.path.join(art, "TSupra", "dash.psd"), (1000, 1000))
    # --todo lists only what is not done
    t = run(art, data, "--todo")
    check(row(t.stdout, "P911") is None and row(t.stdout, "TSupra") is not None, "--todo hides the done ones")
    check("done " in t.stdout.splitlines()[-1], "--todo still ends with the count")
    nodata = run(art, os.path.join(root, "empty-data"))
    check(nodata.returncode == 2 and "empty-data" in nodata.stderr, "a missing data folder is said: %r" % nodata.stderr)
    # a missing folder is said, not a traceback
    bad = run(os.path.join(root, "nope"), data)
    check(bad.returncode != 0 and "Traceback" not in bad.stderr and "nope" in bad.stderr, "a missing art folder is said: %r" % bad.stderr)
    # the tool writes nothing under the folders it reads
    def snapshot():
        return sorted((os.path.join(d, f), os.stat(os.path.join(d, f)).st_mtime, os.path.getsize(os.path.join(d, f))) for d, _, fs in os.walk(root) for f in fs)
    before = snapshot()
    run(art, data)
    after = snapshot()
    check(before == after, "read only")

print("%d failures" % failures)
sys.exit(1 if failures else 0)
