#!/bin/sh
# Window > Reset layout (--reset-layout): a layout the user has messed up (a panel floating off the window, one collapsed, one moved to another dock, the
# splits dragged) comes back as the first run's dock of that mode, for a track, a car's kit and a masks project; the same messed layout without the reset
# stays messed (so the comparison can fail).
# usage: editor_layout_reset.sh EDITOR
set -u
editor=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }

# the docking of a layout file: every dock node (no positions: a floating window's Pos is not the dock's), and each window's dock, tab and collapse
cat > "$work/docks.py" <<'PY'
import re, sys
def docks(path):
    out, window = [], None
    for line in open(path):
        line = line.rstrip("\n")
        m = re.match(r"\[Window\]\[(.*)\]$", line)
        if m:
            window = m.group(1)
            continue
        if line.startswith("["):
            window = None
        if window is not None and re.match(r"(DockId|Collapsed)=", line):
            out.append(window + " " + line)
        if line.strip().startswith(("DockSpace", "DockNode")):
            out.append(re.sub(r" (Pos|Size)=\S+", "", line.strip()))
    return sorted(out)
a, b = docks(sys.argv[1]), docks(sys.argv[2])
if a != b:
    for x in sorted(set(a) ^ set(b)):
        print(("clean: " if x in a else "reset: ") + x)
    sys.exit(1)
PY

# a messed-up layout: `float` floating far off the window, `fold` collapsed, `move` docked with `onto`, every split dragged
cat > "$work/mess.py" <<'PY'
import re, sys
src, dst, float_, fold, move, onto = sys.argv[1:7]
text = open(src).read()
dock_of = dict(re.findall(r"\[Window\]\[(.*?)\]\n(?:.*\n)*?DockId=(0x[0-9A-F]+),\d+", text))
def window(name, fn):
    global text
    text = re.sub(r"(\[Window\]\[%s\]\n)((?:[^\[\n].*\n)*)" % re.escape(name), lambda m: m.group(1) + fn(m.group(2)), text)
window(float_, lambda body: re.sub(r"DockId=.*\n", "", re.sub(r"Pos=.*", "Pos=5000,5000", body)))
window(fold, lambda body: re.sub(r"Collapsed=0", "Collapsed=1", body))
window(move, lambda body: re.sub(r"DockId=0x[0-9A-F]+,\d+", "DockId=%s,9" % dock_of[onto], body))
text = re.sub(r"SizeRef=(\d+),(\d+)", lambda m: "SizeRef=%d,%d" % (int(m.group(1)) * 2 // 3 + 1, int(m.group(2)) * 3 // 4 + 1), text)
open(dst, "w").write(text)
PY

# check NAME TARGET FLOAT FOLD MOVE ONTO: a clean start's layout; the messed one reset gives it back; the messed one not reset does not
check() {
    name=$1 target=$2
    mkdir -p "$work/$name-clean" "$work/$name-reset" "$work/$name-kept"
    "$editor" "$target" --user "$work/$name-clean" --frames 8 >"$work/log" 2>&1 || { fail "$name: a clean start: $(cat "$work/log")"; return; }
    [ -s "$work/$name-clean/track-editor.ini" ] || { fail "$name: the clean start kept no layout"; return; }
    python3 "$work/mess.py" "$work/$name-clean/track-editor.ini" "$work/$name-reset/track-editor.ini" "$3" "$4" "$5" "$6" || { fail "$name: the layout could not be messed up"; return; }
    cp "$work/$name-reset/track-editor.ini" "$work/$name-kept/track-editor.ini"
    "$editor" "$target" --user "$work/$name-kept" --frames 12 >"$work/log" 2>&1 || fail "$name: a run on the messed layout: $(cat "$work/log")"
    python3 "$work/docks.py" "$work/$name-clean/track-editor.ini" "$work/$name-kept/track-editor.ini" >/dev/null && fail "$name: the messed layout is the clean one (the check proves nothing)"
    "$editor" "$target" --user "$work/$name-reset" --reset-layout --frames 12 >"$work/log" 2>&1 || fail "$name: --reset-layout: $(cat "$work/log")"
    python3 "$work/docks.py" "$work/$name-clean/track-editor.ini" "$work/$name-reset/track-editor.ini" >"$work/diff" || fail "$name: Reset layout did not give the first run's dock back: $(cat "$work/diff")"
}

mkdir -p "$work/track"
check track "$work/track" Points Node "Road profile" Limits

mkdir -p "$work/kit/model/near/bank" "$work/kit/art-generated/car_masks"
printf 'id=k1\nname=Kit One\nversion=1\ntype=car\nstem=K1\nsound_from=911\n' > "$work/kit/mod.ini"
printf 'x' > "$work/kit/art-generated/car_masks/rsid_mask.png"
check car "$work/kit" Car Model Preview Masks

mkdir -p "$work/game/cd/SIMDATA/CARFAMS" "$work/game/data/car_masks/Wagon" "$work/exports"
printf 'cfm' > "$work/game/cd/SIMDATA/CARFAMS/WAGON.CFM"
python3 -c "
import sys, struct
open(sys.argv[1], 'wb').write(b'\x89PNG\r\n\x1a\n' + struct.pack('>I4sIIBB', 13, b'IHDR', 100, 50, 8, 0) + bytes(20))
" "$work/game/data/car_masks/Wagon/side_mask.png"
"$editor" --export-car Wagon --into "$work/exports" --cd "$work/game/cd" --port-data "$work/game/data" --frames 2 >"$work/log" 2>&1 || fail "export of a masks project: $(cat "$work/log")"
check masks "$work/exports/wagon-masks" Masks Car Preview Masks

[ "$failures" -eq 0 ] && echo "editor_layout_reset: ok"
exit "$failures"
