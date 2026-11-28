#!/bin/sh
# The car card's lists (the Showcase photos, the Colors) number their rows from 0: no two items may share an ID, or Remove of one row presses the other. The scan step puts the mouse
# on a grid over the window's left half (ImGui flags a shared ID only under the mouse, and only for items on screen), a point every 12 px down, as a button is 15 high; Studio
# draws one frame in twenty of it. `sabotage` gives each list's rows one ID (the step same-ids) and expects the same sweep to fail, so what it skips drawing cannot hide the bug.
# Both lists' last Remove button must be inside what is swept: the card grew past 760 px once and the photo rows went unchecked, saying nothing.
# usage: editor_card_ids.sh EDITOR [sabotage]
. "$(dirname "$0")/editor_headless_common.sh"
mode=${2:-clean}
mkdir -p "$work/kit/model/near/bank" "$work/kit/art-generated/car_masks" "$work/kit/photos"
printf 'id=k1\nname=Kit One\nversion=1\ntype=car\nstem=K1\nsound_from=911\npaints=Red:c8102e,Blue:1040c0\nphotos=a.png,b.png\n' > "$work/kit/mod.ini"
printf 'x' > "$work/kit/art-generated/car_masks/rsid_mask.png"
printf 'x' > "$work/kit/model/near/bank/0010_rsid.png"
for name in a b; do
    python3 -c "
import struct, sys, zlib
w, h = 450, 254
raw = b''.join(b'\\x00' + b'\\x30\\x60\\xc0\\xff' * w for _ in range(h))
def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
open(sys.argv[1], 'wb').write(b'\\x89PNG\\r\\n\\x1a\\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))
" "$work/kit/photos/$name.png" || fail "could not write a showcase picture"
done

# sweep STEPS_BEFORE: the run's output in $work/log, its code the editor's
sweep() { "$editor" "$work/kit" --window 960x900 --frames 3 --steps "${1}scan:40,12,ui-rect:photo1_remove,ui-rect:colour1_remove" >"$work/log" 2>&1; }
swept() {   # both lists' last Remove drawn inside the part of the window the scan goes over
    for button in photo1_remove colour1_remove; do
        awk -v w="$button" '$1 == "ui" && $2 == "rect:" && $3 == w { found = 1; if ($4 >= 480 || $5 + $7 > 890) bad = 1 } END { exit !(found && !bad) }' "$work/log" \
            || fail "$button is not inside what the scan sweeps (960x900, left half): $(grep '^ui rect' "$work/log")"
    done
}

if [ "$mode" = sabotage ]; then
    for list in 0 1; do   # 0 the photos, 1 the colors, one at a time: neither may hide behind the other
        if sweep "same-ids:$list,"; then fail "the sweep passed with the rows of list $list sharing an ID"
        else grep -q "two items share an ID" "$work/log" || fail "list $list: the run failed for another reason: $(tail -3 "$work/log")"
        fi
        swept
    done
else
    sweep "" || fail "two items of the card share an ID, or a list's Remove is not drawn: $(grep -v '^imgui error' "$work/log" | tail -2)"
    swept
fi

[ $failures -eq 0 ] && echo "editor_card_ids ($mode): ok"
exit $failures
