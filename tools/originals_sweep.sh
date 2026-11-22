#!/bin/sh
# The originals sweep: every migrated original car kit (the 20 of the originals workspace, traffic and police included) is walked end to end in Studio and the game, headless,
# and ONE report says, per kit: it opens, its masks' classes (a police car must have lamp classes 240..252), the night Preview (head lights, and the police lights where the kit has
# lamp classes), the In-game photo, Test drive starting and stopping, and an outside edit of a mask showing in the Preview / the photo.
# A kit is COPIED to a scratch folder first: the source (originals-workspace/cars) and art/ are only read, never written. A step that must work and does not is a FAIL (exit 1);
# an edit that does not show is a NOTE (a texture the night view does not light may legitimately not change).
# usage: originals_sweep.sh EDITOR TNFS ORIGINAL_DIR CARS_DIR REPORT [KIT...]      (KIT: folder names in CARS_DIR; default all)
editor=$1; tnfs=$2; orig=$3; cars=$4; report=$5; shift 5
[ -x "$editor" ] && [ -x "$tnfs" ] && [ -r "$orig/SIMDATA/CARFAMS/P911.CFM" ] && [ -d "$cars" ] || { echo "usage: $0 EDITOR TNFS ORIGINAL_DIR CARS_DIR REPORT [KIT...] (needs the built editor, the game and the CD's files)" >&2; exit 2; }
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/originals-sweep-XXXXXX") || exit 1
trap 'python3 -c "import shutil,sys; shutil.rmtree(sys.argv[1], ignore_errors=True)" "$work"' EXIT
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy
guard=
for t in timeout gtimeout; do command -v "$t" >/dev/null 2>&1 && { guard="$t -k 5 300"; break; }; done
if [ $# -gt 0 ]; then kits=$*; else kits=$(cd "$cars" && for d in */; do [ -f "$d/mod.ini" ] && echo "${d%/}"; done); fi
runs=0
studio() { runs=$((runs + 1)); $guard "$editor" "$@" --user "$work/player$runs" >"$work/log" 2>&1; }
png_ok() { [ -s "$1" ] && [ "$(head -c 8 "$1" | od -An -c | tr -d ' ')" = '211PNG\r\n032\n' ]; }
fails=0; notes=0; n=0
{
printf '# Originals sweep: %s\n\n' "$(date +%Y-%m-%d)"
printf 'Kits from `%s` (copied to scratch, never written). Editor and game as given; headless, night view, head lights.\n\n' "$cars"
printf '| kit | opens | classes | police classes | Preview night | Preview police | In-game photo | Test drive | edit in Preview | edit in photo |\n|---|---|---|---|---|---|---|---|---|---|\n'
} > "$report"
detail="$work/detail.txt"; : > "$detail"
for kit in $kits; do
    n=$((n + 1))
    [ -f "$cars/$kit/mod.ini" ] || { echo "FAIL: $kit is not a kit in $cars" >&2; fails=$((fails + 1)); continue; }
    k="$work/kits/$kit"
    python3 -c "import shutil,sys; shutil.copytree(sys.argv[1], sys.argv[2], ignore=shutil.ignore_patterns('build'))" "$cars/$kit" "$k" || { echo "FAIL: copy of $kit" >&2; fails=$((fails + 1)); continue; }
    row="| $kit"
    ok() { row="$row | ok"; }
    bad() { row="$row | **FAIL**"; fails=$((fails + 1)); printf '%s: %s: %s\n' "$kit" "$1" "$(tail -3 "$work/log" | tr '\n' ' ')" >> "$detail"; echo "FAIL: $kit: $1" >&2; }
    note() { row="$row | note"; notes=$((notes + 1)); printf '%s: %s\n' "$kit" "$1" >> "$detail"; }
    first=$(cd "$k/art-generated/car_masks" 2>/dev/null && ls ./*_mask.png 2>/dev/null | head -1 | sed 's#^\./##; s#_mask\.png$##')
    [ -f "$k/art-generated/car_masks/side_mask.png" ] && first=side
    # 1. it opens
    if studio "$k" --frames 4; then ok; else bad "does not open"; fi
    # 2. the masks' classes
    if python3 -I "$here/originals_sweep_classes.py" "$k" > "$work/classes.txt" 2>&1; then
        lamp=$(tr ' ' '\n' < "$work/classes.txt" | grep -cE '^(24[0-9]|25[0-2])=')
        row="$row | $(wc -l < "$work/classes.txt" | tr -d ' ') masks"
        printf '%s classes:\n%s\n' "$kit" "$(sed 's/^/    /' "$work/classes.txt")" >> "$detail"
        if [ "$lamp" -gt 0 ]; then row="$row | yes"; haslamp=1; else row="$row | no"; haslamp=0; fi
        [ "$kit" = copmust ] && [ "$haslamp" -eq 0 ] && { fails=$((fails + 1)); echo "FAIL: copmust has no lamp class 240..252" >&2; printf 'copmust: no lamp class\n' >> "$detail"; }
    else row="$row | **FAIL** | -"; fails=$((fails + 1)); haslamp=0; echo "FAIL: $kit: masks: $(cat "$work/classes.txt")" >&2; fi
    # 3. the night Preview and the police-light Preview
    pv() { studio "$k" --orbit 30,20 --view night --lights "$1" --preview-tex "$first" --frames 14 --shot "$2"; }
    if pv head "$work/pv-before.png" && png_ok "$work/pv-before.png"; then ok; else bad "night Preview"; fi
    if [ "$haslamp" -eq 1 ]; then
        if pv police "$work/pv-police.png" && png_ok "$work/pv-police.png"; then if cmp -s "$work/pv-before.png" "$work/pv-police.png"; then note "the police lights do not change the Preview"; else ok; fi; else bad "police Preview"; fi
    else row="$row | -"; fi
    # 4. the In-game photo, 5. Test drive
    photo() { studio "$k" --game "$tnfs" --cd "$orig" --race-photo --night --race-png "$1" --frames 4; }
    if photo "$work/ph-before.png" && png_ok "$work/ph-before.png"; then ok; else bad "In-game photo"; fi
    if studio "$k" --game "$tnfs" --cd "$orig" --frames 2 --steps race,race-log,race-stop \
        && grep -q '^race log: race car: slot 0 is ' "$work/log" && grep -q '^race end: Stopped.' "$work/log"; then ok; else bad "Test drive"; fi
    # 6. an outside edit of the first mask (the whole texture to the fixed black, class 224): the Preview and the photo may show it
    if [ -n "$first" ] && python3 -I "$here/../tests/mask_poke.py" "$k/art-generated/car_masks/${first}_mask.png" 0 0 4096 4096 224; then
        if pv head "$work/pv-after.png" && ! cmp -s "$work/pv-before.png" "$work/pv-after.png"; then ok; else note "the edit of $first does not show in the night Preview"; fi
        if photo "$work/ph-after.png" && ! cmp -s "$work/ph-before.png" "$work/ph-after.png"; then ok; else note "the edit of $first does not show in the In-game photo"; fi
    else row="$row | - | -"; fi
    echo "$row |" >> "$report"
    echo "swept $kit"
done
{
printf '\n%d kits, %d FAIL, %d note(s). `ok` = the step ran and its picture/log is right; `note` = an edit that did not change the picture (not a failure); `-` = not applicable (no lamp classes).\n' "$n" "$fails" "$notes"
printf '\n## Details\n\n```\n'; cat "$detail"; printf '```\n'
} >> "$report"
echo "report: $report ($n kits, $fails FAIL, $notes notes)"
[ "$fails" -eq 0 ]
