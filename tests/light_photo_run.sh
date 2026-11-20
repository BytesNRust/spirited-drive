#!/bin/sh
# One real run of tools/light_photo.sh on a CD track: the sheet is 1280x960 (four halved pictures), and the four times of day give four different pictures.
# Needs the CD's data (original/): the test is only registered when it is there. usage: light_photo_run.sh TNFS REPO WORK [DATA]
set -u
TNFS=$1 REPO=$2 WORK=$3 DATA=${4:-$2/original}
mkdir -p "$WORK"
WORK=$(mktemp -d "$WORK/run.XXXXXX")
trap 'rc=$?; if [ "$rc" -ne 0 ] && [ "${TNFS_KEEP_RUN:-0}" = 1 ]; then echo "kept: $WORK" >&2; else find "$WORK" -delete; fi' EXIT
sh "$REPO/tools/light_photo.sh" "$TNFS" "$DATA" CY1 70 "$WORK/out" > "$WORK/run.out" 2>&1 || { cat "$WORK/run.out" >&2; echo "FAIL: light_photo.sh failed" >&2; exit 1; }
python3 -I - "$WORK/out" <<'PY' || exit 1
import struct, sys
out = sys.argv[1]
def size(path):
    with open(path, "rb") as f:
        head = f.read(24)
    return struct.unpack(">II", head[16:24])
assert size(out + "/CY1_70_sheet.png") == (1280, 960), size(out + "/CY1_70_sheet.png")
pics = [open(out + "/CY1_70_%s.png" % t, "rb").read() for t in "ABCN"]
assert len({bytes(p) for p in pics}) == 4, "two times of day gave the same picture"
print("light_photo_run: ok")
PY
