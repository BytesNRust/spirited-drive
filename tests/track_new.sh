#!/bin/sh
# A track made from a text layout (BYTES-0009 AC6): it passes validate, goes into a slot of a mod directory, and the
# track viewer shows it from the first node to the last. A bad layout stops with its file and line, and nothing is
# overwritten without --force.
# usage: track_new.sh TNFS STUDIO ORIGINAL_DIR
set -u
tnfs=$1
studio=$2
orig=$3
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
export SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software

[ -f "$orig/SIMDATA/MISC/AL1.TRI" ] || { echo "skip: no AL1.TRI"; exit 0; }

cat > "$work/loop.trk" <<'LAYOUT'
# a stage: straight, a climbing right-hander with banking, a long left, a flat run-out
section 24
section 40 turn=90 grade=3 bank=6 width=50
section 30 turn=-120 grade=-3 bank=-6
section 30 bank=0 grade=0 width=40
LAYOUT
segments=124

for template in AL1 TR1; do
    out="$work/out_$template"
    mod="$work/mod_$template"
    "$studio" track new "$work/loop.trk" --template "$template" --out "$out" --stem MYSTAGE --data "$orig" >"$work/log" 2>&1 || { fail "track new from $template: $(cat "$work/log")"; continue; }
    [ -f "$out/SIMDATA/MISC/MYSTAGE.TRI" ] || fail "$template: no MYSTAGE.TRI written"
    [ -f "$out/SIMDATA/MISC/MYSTAGEINFO" ] || fail "$template: the INFO file was not copied"
    "$studio" validate "$out" >"$work/log" 2>&1 || fail "$template: the generated track does not validate: $(cat "$work/log")"
    grep -q '^error' "$work/log" && fail "$template: validate reported errors: $(cat "$work/log")"   # TR1's own CD defect is a warning
    "$studio" track new "$work/loop.trk" --template "$template" --out "$out" --stem MYSTAGE --data "$orig" >/dev/null 2>"$work/log" && fail "$template: a second run overwrote the output"
    grep -q "exist" "$work/log" || fail "$template: the refusal does not say a file exists: $(cat "$work/log")"
    "$studio" track new "$work/loop.trk" --template "$template" --out "$out" --stem MYSTAGE --data "$orig" --force >/dev/null 2>&1 || fail "$template: --force was refused"

    "$studio" slot replace track "$template" "$out" --stem MYSTAGE --mods "$mod" --data "$orig" >"$work/log" 2>&1 || { fail "$template: slot replace: $(cat "$work/log")"; continue; }

    # the viewer walks the track: first node, the curves, the end; every frame renders and the views differ
    last=$((segments * 4 - 4))
    seen=""
    for node in 0 20 60 100 140 200 260 320 380 $last; do
        "$tnfs" --data "$orig" --mods "$mod" --track "$template" --node "$node" --screenshot "$work/$template-$node.ppm" >"$work/log" 2>&1 || { fail "$template: viewer failed at node $node: $(tail -3 "$work/log")"; continue; }
        [ -s "$work/$template-$node.ppm" ] || fail "$template: no image at node $node"
        seen="$seen $(cksum < "$work/$template-$node.ppm" | cut -d' ' -f1)"
    done
    distinct=$(printf '%s\n' $seen | sort -u | wc -l | tr -d ' ')
    [ "$distinct" -ge 9 ] || fail "$template: only $distinct different views along the track"
    "$tnfs" --data "$orig" --mods "$mod" --track "$template" --node $((segments * 4 + 100)) --screenshot "$work/$template-past.ppm" >/dev/null 2>&1 && : # past the end: must not crash the shell test
done

# the generated track is not the original one
"$tnfs" --data "$orig" --track AL1 --node 120 --screenshot "$work/orig.ppm" >/dev/null 2>&1
[ -f "$work/AL1-100.ppm" ] && [ "$(cksum < "$work/orig.ppm")" != "$(cksum < "$work/AL1-100.ppm")" ] || fail "the mod's AL1 looks like the original"

# bad layouts: exit 1, file and line named, nothing written
printf 'section 20\nsection 10 turn=abc\n' > "$work/bad.trk"
"$studio" track new "$work/bad.trk" --template AL1 --out "$work/bad_out" --data "$orig" >/dev/null 2>"$work/log" && fail "a bad layout was accepted"
grep -q "bad.trk" "$work/log" && grep -q "line 2" "$work/log" || fail "the bad layout is not reported with file and line: $(cat "$work/log")"
[ -e "$work/bad_out" ] && fail "a bad layout still wrote files"
printf 'section 3\n' > "$work/short.trk"
"$studio" track new "$work/short.trk" --template AL1 --out "$work/short_out" --data "$orig" >/dev/null 2>&1 && fail "a 3-segment track was accepted"
"$studio" track new "$work/loop.trk" --template NOPE --out "$work/x" --data "$orig" >/dev/null 2>"$work/log" && fail "an unknown template slot was accepted"
"$studio" track new "$work/nothere.trk" --template AL1 --out "$work/x" --data "$orig" >/dev/null 2>&1 && fail "a missing layout file was accepted"

[ "$failures" -eq 0 ] && echo "track_new: ok"
exit $((failures != 0))
