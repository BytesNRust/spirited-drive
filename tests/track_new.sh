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
# the player's folder is this run's own: a tnfs with no --user would write its log, settings and replay in the real one
export TNFS_PREF_DIR="$work/pref"
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
export SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software

[ -f "$orig/SIMDATA/MISC/AL1.TRI" ] || { echo "skip: no AL1.TRI"; exit 77; }

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
    # the track as a piece (dep59 step 5): --mods serves no changed track, so the viewer walks the piece, by its id, in Restomod
    pc="$work/piece_$template"
    "$studio" track new "$work/loop.trk" --template "$template" --piece --id "walk-$(echo "$template" | tr 'A-Z' 'a-z')" --stem MYSTAGE --out "$pc" --data "$orig" >"$work/log" 2>&1 || { fail "$template: track new --piece: $(cat "$work/log")"; continue; }
    walk="walk-$(echo "$template" | tr 'A-Z' 'a-z')"
    mkdir -p "$work/user_$template" && echo "profile=restomod" > "$work/user_$template/port.cfg"

    # the viewer walks the track: first node, the curves, the end; every frame renders and the views differ
    last=$((segments * 4 - 4))
    seen=""
    for node in 0 20 60 100 140 200 260 320 380 $last; do
        "$tnfs" --data "$orig" --user "$work/user_$template" --mods "$pc" --drive "$walk" --car 5 --racers 1 --node "$node" --screenshot "$work/$template-$node.ppm" >"$work/log" 2>&1 || { fail "$template: viewer failed at node $node: $(tail -3 "$work/log")"; continue; }
        [ -s "$work/$template-$node.ppm" ] || fail "$template: no image at node $node"
        seen="$seen $(cksum < "$work/$template-$node.ppm" | cut -d' ' -f1)"
    done
    distinct=$(printf '%s\n' $seen | sort -u | wc -l | tr -d ' ')
    [ "$distinct" -ge 9 ] || fail "$template: only $distinct different views along the track"
    "$tnfs" --data "$orig" --user "$work/user_$template" --mods "$pc" --drive "$walk" --car 5 --racers 1 --node $((segments * 4 + 100)) --screenshot "$work/$template-past.ppm" >/dev/null 2>&1 && : # past the end: must not crash the shell test
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

# a track piece (dep59 step 5, T6): a card, the stage's files under the stem and the number, nothing else; a stem of the CD's is refused
piece="$work/piece"
"$studio" track new "$work/loop.trk" --template AL1 --piece --id my-stage --stem MYSTAGE --out "$piece" --data "$orig" >"$work/log" 2>&1 || fail "piece: track new --piece: $(cat "$work/log")"
for f in mod.ini SIMDATA/MISC/MYSTAGE1.TRI SIMDATA/MISC/MYSTAGE1INFO SIMDATA/ETRACKFM/MYSTAGE1_001.FAM SIMDATA/GTRACKFM/MYSTAGE1_001.FAM; do
    [ -f "$piece/$f" ] || fail "piece: $f was not written"
done
for line in type=track stem=MYSTAGE kind=stage segments=1 id=my-stage; do
    grep -qx "$line" "$piece/mod.ini" || fail "piece: mod.ini lacks $line: $(cat "$piece/mod.ini" 2>/dev/null)"
done
"$studio" validate "$piece" >"$work/log" 2>&1 || fail "piece: it does not validate: $(cat "$work/log")"
for stem in CYX ALPES TR9 TOOLONGSTEM; do
    "$studio" track new "$work/loop.trk" --template AL1 --piece --id bad --stem "$stem" --out "$work/bad_$stem" --data "$orig" >/dev/null 2>"$work/log" && fail "piece: --stem $stem was accepted"
    grep -q "$stem" "$work/log" || fail "piece: the refusal of --stem $stem does not name it: $(cat "$work/log")"
    [ -e "$work/bad_$stem" ] && fail "piece: --stem $stem still wrote files"
done
"$studio" track new "$work/loop.trk" --template AL1 --piece --stem MYSTAGE --out "$work/x" --data "$orig" >/dev/null 2>&1 && fail "piece: no --id was accepted"
"$studio" track new "$work/loop.trk" --template AL1 --id x --out "$work/x" --data "$orig" >/dev/null 2>&1 && fail "--id without --piece was accepted"

# port (newcomer pains): the card keeps what a modder wrote by hand when --force rewrites it, writes author and description, names the
# data root a missing template is looked for in, and a tint that is not #RRGGBB is refused by validate and by mod pack
keep="$work/keep"
"$studio" track new "$work/loop.trk" --template AL1 --piece --id keep-me --stem KEEPME --out "$keep" --data "$orig" --author Ann --description "a loop" >"$work/log" 2>&1 ||
    fail "keep: track new --piece: $(cat "$work/log")"
grep -qx 'author=Ann' "$keep/mod.ini" || fail "keep: --author is not in mod.ini: $(cat "$keep/mod.ini")"
grep -qx 'description=a loop' "$keep/mod.ini" || fail "keep: --description is not in mod.ini"
"$studio" track new "$work/loop.trk" --template AL1 --piece --id keep-me --stem KEEPME --out "$work/keep_plain" --data "$orig" >/dev/null 2>&1 || fail "keep: track new without the flags"
grep -q '^# author=' "$work/keep_plain/mod.ini" || fail "keep: no author key is shown when none is given"
grep -q '^# description=' "$work/keep_plain/mod.ini" || fail "keep: no description key is shown when none is given"
"$studio" validate "$work/keep_plain" >/dev/null 2>&1 || fail "keep: the card with the placeholders does not validate"
# the hand edits: a version bumped, and the keys the generator does not write
sed 's/^version=0\.1$/version=0.3/' "$keep/mod.ini" > "$work/mod.new" && cp "$work/mod.new" "$keep/mod.ini"
printf 'length=14.2 km\ntimes=day,dusk\nshowcase_tint=#C08040\nfuture_key=kept\n' >> "$keep/mod.ini"
"$studio" track new "$work/loop.trk" --template AL1 --piece --id keep-me --stem KEEPME --out "$keep" --data "$orig" --force >"$work/log" 2>&1 ||
    fail "keep: --force over the piece: $(cat "$work/log")"
for line in version=0.3 author=Ann 'description=a loop' 'length=14.2 km' 'times=day,dusk' '#C08040' future_key=kept; do
    grep -qF -- "$line" "$keep/mod.ini" || fail "keep: --force lost $line: $(cat "$keep/mod.ini")"
done
grep -qx 'showcase_tint=#C08040' "$keep/mod.ini" || fail "keep: --force lost the showcase_tint"
[ "$(grep -c '^id=' "$keep/mod.ini")" -eq 1 ] || fail "keep: the card holds two id= lines"
# a card that is not a card is not overwritten: the refusal names it
mkdir -p "$work/broken" && printf 'id=BAD\n' > "$work/broken/mod.ini"
"$studio" track new "$work/loop.trk" --template AL1 --piece --id keep-me --stem KEEPME --out "$work/broken" --data "$orig" --force >/dev/null 2>"$work/log" &&
    fail "keep: --force overwrote a damaged mod.ini"
grep -q "fix it, or move it aside" "$work/log" || fail "keep: the damaged mod.ini is not explained: $(cat "$work/log")"
# the data root: a template that is not there names the folder it looked in and the flag that sets it
"$studio" track new "$work/loop.trk" --template AL1 --out "$work/nodata" --data "$work/nowhere" >/dev/null 2>"$work/log" && fail "a missing data root was accepted"
grep -q "$work/nowhere" "$work/log" && grep -q -- "--data" "$work/log" || fail "the missing data root is not named with --data: $(cat "$work/log")"
# a tint that is not #RRGGBB: validate and mod pack refuse it
sed 's/^showcase_tint=.*/showcase_tint=orange/' "$keep/mod.ini" > "$work/mod.tint" && cp "$work/mod.tint" "$keep/mod.ini"
"$studio" validate "$keep" >"$work/log" 2>&1 && fail "validate accepted showcase_tint=orange"
grep -q "PIECE.TINT" "$work/log" || fail "validate does not name PIECE.TINT: $(cat "$work/log")"
"$studio" mod pack "$keep" "$work/keep.zip" >"$work/log" 2>&1 && fail "mod pack accepted showcase_tint=orange"
[ -e "$work/keep.zip" ] && fail "mod pack wrote a zip for a bad tint"

[ "$failures" -eq 0 ] && echo "track_new: ok"
exit $((failures != 0))
