#!/bin/sh
# The car panels' widgets clicked with the input script (ui_probe.h), from fixture kits of two of the game's own cars, no game files: a colour's name box clicked, cleared with the
# keyboard and typed into (ui-text), Up and Down and Add color and Remove clicked, then Copy to game repo and the Copy button of its window: the row of the car's table in the
# repo's core/formats/car_paint.c has the change and nothing else moved. The Diablo (LDIABL) has two colours, the NSX (ANSX) four.
# usage: editor_car_panel_clicks.sh EDITOR
. "$(dirname "$0")/editor_headless_common.sh"
repo_src="$(cd "$(dirname "$0")/.." && pwd)"
clicked() { "$editor" "$@" --window 1280x800 --frames 30; }

make_kit() {   # STEM PAINTS: the kit, a repo with the car_paint.c as it is
    stem=$1 kit=$work/kit-$1 repo=$work/repo-$1 table=$work/repo-$1/core/formats/car_paint.c
    mkdir -p "$kit/art-generated/car_masks" "$kit/model/near/bank" "$repo/data/car_masks/$stem" "$repo/core/formats"
    cp "$repo_src/core/formats/car_paint.c" "$table"
    printf 'id=orig\nname=Orig\nversion=1\ntype=car\nstem=%s\nsound_from=911\npaints=%s\n' "$stem" "$2" > "$kit/mod.ini"
}
changed_lines() { diff "$table" "$repo_src/core/formats/car_paint.c" | grep -c '^[<>]'; }

# the NSX: the third colour's name edited by keyboard and text, Save, Copy to game repo, the window's Copy
make_kit ANSX "Berlina Black:0e0e10,Grand Prix White:f1f1ee,Formula Red:b3121e,Citrous Yellow Metallic:e6d94a"
clicked "$kit" --port-data "$repo/data" --steps ui-at:colour2_name,0.98,key:BackSpace,key:BackSpace,key:BackSpace,ui-text:Ink,paint-print,save,ui-click:copy_repo,ui-click:copy_dialog_copy,status > "$work/log" 2>&1 || fail "ANSX clicks: $(cat "$work/log")"
grep -q "^paints: Berlina Black #0e0e10 Grand Prix White #f1f1ee Formula Ink #b3121e Citrous Yellow Metallic #e6d94a$" "$work/log" || fail "ANSX: the name typed into the box is not in the card: $(grep '^paints' "$work/log")"
grep -q "^name=\|paints=.*Formula Ink:b3121e" "$kit/mod.ini" || fail "ANSX: Save did not write the new name: $(cat "$kit/mod.ini")"
grep -q '{ "Formula Ink", { 179, 18, 30 } }' "$table" || fail "ANSX: Copy to game repo did not put the new name in the table: $(diff "$table" "$repo_src/core/formats/car_paint.c")"
[ "$(changed_lines)" = 2 ] || fail "ANSX: more than the one line of the row changed: $(diff "$table" "$repo_src/core/formats/car_paint.c")"
# Copy with nothing left to copy is not offered (the button is off)

# Up and Down: the second colour moved up, then down again; the row follows in the repo after Copy
clicked "$kit" --port-data "$repo/data" --steps ui-click:colour1_up,paint-print,ui-click:colour0_down,paint-print > "$work/log" 2>&1 || fail "ANSX up/down: $(cat "$work/log")"
grep -q "^paints: Grand Prix White #f1f1ee Berlina Black #0e0e10 Formula Ink" "$work/log" || fail "ANSX: Up did not move the second colour first: $(grep '^paints' "$work/log")"
grep '^paints' "$work/log" | tail -1 | grep -q "^paints: Berlina Black #0e0e10 Grand Prix White #f1f1ee Formula Ink" || fail "ANSX: Down did not put it back: $(grep '^paints' "$work/log")"

# the Diablo: two colours; Add color makes a third, named and cleared by the keyboard, Save, Copy: the row has three colours
make_kit LDIABL "Rosso Diablo:b3121e,30th Anniversary Lavende:9878c8"
clicked "$kit" --port-data "$repo/data" --steps ui-click:add_color,ui-at:colour2_name,0.98,key:BackSpace,key:BackSpace,key:BackSpace,key:BackSpace,key:BackSpace,key:BackSpace,key:BackSpace,ui-text:Nero,paint-print,save,ui-click:copy_repo,ui-click:copy_dialog_copy,status > "$work/log" 2>&1 || fail "LDIABL clicks: $(cat "$work/log")"
grep -q "^paints: Rosso Diablo #b3121e 30th Anniversary Lavende #9878c8 Nero #808080$" "$work/log" || fail "LDIABL: Add color and the typed name are not in the card: $(grep '^paints' "$work/log")"
grep -A2 "\"LDiabl\"" "$table" | grep -q "{ \"Nero\", { 128, 128, 128 } }" || fail "LDIABL: the third colour is not in the table's row: $(diff "$table" "$repo_src/core/formats/car_paint.c")"
grep -q '"30th Anniversary Lavender"' "$table" || fail "LDIABL: the game's full name was cut in the table"
# Remove on the third colour: back to two, the row is the game's own again (nothing left to copy)
clicked "$kit" --port-data "$repo/data" --steps ui-click:colour2_remove,paint-print,save,ui-click:copy_repo,print-publish,ui-click:copy_dialog_copy,status > "$work/log" 2>&1 || fail "LDIABL remove: $(cat "$work/log")"
grep -q "^paints: Rosso Diablo #b3121e 30th Anniversary Lavende #9878c8$" "$work/log" || fail "LDIABL: Remove did not take the third colour away: $(grep '^paints' "$work/log")"
grep -A2 "\"LDiabl\"" "$table" | grep -q "\"Nero\"" && fail "LDIABL: the removed colour is still in the table"
clicked "$kit" --port-data "$repo/data" --steps ui-click:no_such_button > "$work/log" 2>&1 && fail "a click on a widget that is not there went"

[ "$failures" = 0 ] && echo ok
exit "$failures"
