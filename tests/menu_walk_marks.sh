#!/bin/sh
# tools/menu_walk.sh's exit when a WAIT times out, with a stand-in for the game (no game data): the input script's own lines hold the text of the
# mark it waits for, and the walk must not take them for the screen. Usage: menu_walk_marks.sh <repo>
repo=$1
command -v zsh >/dev/null 2>&1 && command -v timeout >/dev/null 2>&1 || { echo "skipped: no zsh or no timeout (tools/menu_walk.sh needs both)"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/walk_marks.XXXXXX") || exit 1
trap 'find "$work" -type f -delete; find "$work" -depth -type d -empty -delete' EXIT
# the stand-in: writes $FAKE_LOG's lines to the user folder's log ("-" a pause of 3 s), one frame, and stays until the walk stops it
cat > "$work/game" <<'END'
#!/bin/sh
while [ $# -gt 0 ]; do [ "$1" = --user ] && user=$2; shift; done
printf 'P6\n2 1\n255\n\001\002\003\200\201\202' > "$TNFS_CAPTURE_DIR/frame_00001.ppm"
printf '%s\n' "$FAKE_LOG" | while IFS= read -r line; do
    if [ "$line" = - ]; then sleep 3; else printf '%s\n' "$line" >> "$user/spirited-drive.log"; fi
done
n=0; while [ $n -lt 30 ]; do sleep 1; n=$((n + 1)); done
END
chmod +x "$work/game"
printf 'tracks=dune-run\n' > "$work/solo.ini"
mkdir "$work/mods"
failed=0
# walk <case> <target> <exit wanted> <log lines>
walk() {
    FAKE_LOG=$4 DATA="$work" MODS="$work/mods" EDITION="$work/solo.ini" TAIL=0 STILL=2 FRESH=0 QUIET=1 zsh "$repo/tools/menu_walk.sh" "$work/game" "$work/$1" "$2" >"$work/$1.log" 2>&1 < /dev/null
    rc=$?
    if [ $rc -eq "$3" ]; then echo "ok: $1 (exit $rc)"; else echo "FAIL: $1: exit $rc, wanted $3"; cat "$work/$1.log"; failed=1; fi
}
walk reached central 0 "mark:screen=name_box
input script: step 6: waiting for mark:screen=central
mark:screen=central"
walk never-opened central 3 "mark:screen=name_box
input script: step 6: waiting for mark:screen=central
input script: step 6: WAIT mark:screen=central timed out after 60 s"
walk never-content content 6 "mark:screen=game_mode
input script: step 5: waiting for mark:screen=content
input script: step 5: WAIT mark:screen=content timed out after 60 s"
walk card added-track 0 "mark:screen=central
mark:card=16"
walk never-card added-track 3 "mark:screen=central
input script: step 8: waiting for mark:card=16
-
input script: step 8: WAIT mark:card=16 timed out after 60 s"
walk never-page2 records-p2 3 "mark:screen=car_records
input script: step 12: waiting for mark:records_page=1
-
input script: step 12: WAIT mark:records_page=1 timed out after 60 s"
exit $failed
