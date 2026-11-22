#!/bin/sh
# The Track light window, headless (--ask light): it opens on a light folder, takes the photo of the working copy through the game (a fake one here, which records the
# light files it was given), and leaves the folder as it was; a folder with no track_light/ leaves the start screen up with the reason. No CD data and no real game.
# usage: editor_light_window.sh EDITOR REPO
set -u
editor=$1 repo=$2
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }

mkdir -p "$work/light/track_light" "$work/cd" "$work/bare" "$work/tmp"
export TMPDIR="$work/tmp/"
cp "$repo/data/track_light/AL1B.txt" "$work/light/track_light/"
cat > "$work/fake_game" <<'SH'
#!/bin/sh
shot= mods=
while [ $# -gt 0 ]; do case $1 in --screenshot) shot=$2; shift;; --mods) mods=$2; shift;; esac; shift; done
{ echo "time=$TNFS_LIGHT_TIME view=$TNFS_VIEW nohud=${TNFS_NO_HUD:-0}"; head -2 "$mods"/track_light/*.txt; } > "$FAKE_RECORD"
printf 'P6\n1280 960\n255\n' > "$shot"
head -c 3686400 /dev/zero | LC_ALL=C tr '\000' '\100' >> "$shot"
SH
chmod +x "$work/fake_game"
export FAKE_RECORD="$work/record"
before=$(cksum < "$work/light/track_light/AL1B.txt")

"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --window 1280x800 --frames 200 --shot "$work/window.png" > "$work/out" 2>&1 ||
    fail "the window did not run: $(tail -3 "$work/out")"
[ -s "$work/window.png" ] || fail "no picture of the window"
grep -q "^time=day view=1 nohud=1$" "$work/record" 2>/dev/null || fail "the photo was not taken at day, at the bumper (kind 1), without the HUD: $(cat "$work/record" 2>/dev/null)"
grep -q "^track AL1 time B " "$work/record" 2>/dev/null || fail "the game was not given AL1B's file: $(cat "$work/record" 2>/dev/null)"
[ "$(cksum < "$work/light/track_light/AL1B.txt")" = "$before" ] || fail "the light file changed without a Save"
[ -z "$(ls "$work/tmp")" ] || fail "the photo left its scratch folder behind: $(ls "$work/tmp")"

# the View selector: each --light-view is the game's chase kind (Bumper 1, Dash 0, near 2, mid 3, far 4, High 12), and the choice is kept in the ini for the next run
i=0
for want in 1 0 2 3 4 12; do
    rm -f "$work/record"
    "$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --light-view "$i" --window 1280x800 --frames 80 --user "$work/viewuser$i" --shot "$work/view$i.png" > "$work/outv" 2>&1 ||
        fail "--light-view $i did not run: $(tail -3 "$work/outv")"
    grep -q "^time=day view=$want nohud=1$" "$work/record" 2>/dev/null || fail "--light-view $i: the game was not given TNFS_VIEW=$want: $(cat "$work/record" 2>/dev/null)"
    i=$((i + 1))
done
grep -q "^LightView=5$" "$work/viewuser5/track-editor.ini" 2>/dev/null || fail "the High view was not kept in the ini: $(cat "$work/viewuser5/track-editor.ini" 2>/dev/null)"
rm -f "$work/record"
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --window 1280x800 --frames 80 --user "$work/viewuser5" --shot "$work/viewkept.png" > "$work/outv" 2>&1 ||
    fail "the run on the kept view did not run: $(tail -3 "$work/outv")"
grep -q "^time=day view=12 nohud=1$" "$work/record" 2>/dev/null || fail "the kept view (High) was not used by the next run: $(cat "$work/record" 2>/dev/null)"
# the HUD checkbox: off by default (nohud=1 above), --light-hud 1 puts it back, kept in the ini like the view
rm -f "$work/record"
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --light-hud 1 --window 1280x800 --frames 80 --user "$work/huduser" --shot "$work/hud.png" > "$work/outv" 2>&1 ||
    fail "--light-hud 1 did not run: $(tail -3 "$work/outv")"
grep -q "^time=day view=1 nohud=0$" "$work/record" 2>/dev/null || fail "--light-hud 1: the game still got TNFS_NO_HUD: $(cat "$work/record" 2>/dev/null)"
grep -q "^LightHud=1$" "$work/huduser/track-editor.ini" 2>/dev/null || fail "the HUD choice was not kept in the ini"
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --light-hud 2 --frames 5 > "$work/outbad" 2>&1 && fail "--light-hud 2 was accepted"
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --light-view 6 --frames 5 > "$work/outbad" 2>&1 && fail "--light-view 6 was accepted"

"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/bare" --ask light --window 1280x800 --frames 20 --shot "$work/bare.png" > "$work/out2" 2>&1 ||
    fail "a folder with no track_light/ ended the editor: $(tail -3 "$work/out2")"
[ -s "$work/bare.png" ] || fail "no picture for a folder with no track_light/"
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --window 1280x800 --frames 30 --steps print-panels > "$work/panels" 2>&1 ||
    fail "print-panels in the Track light window: $(tail -3 "$work/panels")"
[ "$(grep -c '^panel [0-9]*: .*: open, drawn' "$work/panels")" = 3 ] && grep -q '^unlisted:$' "$work/panels" || fail "the Track light window's panels are not all in the Window menu: $(cat "$work/panels")"
# the Tunnels section: AL1's two tunnels (its places file) listed as all, Own look and a lamp strength written as `tunnel`/`tunnel_lamp` lines over exactly the tunnel's nodes, Save, and
# Remove own look back to the file as it was, byte for byte
mkdir -p "$work/light/track"
cp "$repo/data/track/AL1.txt" "$work/light/track/"
places_before=$(cksum < "$work/light/track/AL1.txt")
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --frames 30 --steps print-tunnels,tunnel-strength:1,220,print-tunnels,save > "$work/tunnels" 2>&1 ||
    fail "the Tunnels steps: $(tail -3 "$work/tunnels")"
grep -q "^tunnels: 2; all: light 100.0 lamps 150 reach 1.8$" "$work/tunnels" || fail "the All row is not the file's tunnel_tone and the game's day lamps: $(grep '^tunnels' "$work/tunnels")"
[ "$(grep -c '^tunnel 1: 1836-1847 place 1 as all' "$work/tunnels")" = 1 ] || fail "the second tunnel is not listed as all: $(grep '^tunnel ' "$work/tunnels")"
grep -q "^tunnel 1: 1836-1847 place 1 own light 100.0\* lamps 220\* reach 1.8$" "$work/tunnels" || fail "Own look and the strength did not make the tunnel's own look: $(grep '^tunnel ' "$work/tunnels")"
grep -q "^tunnel 1836 1847 100 1/0.8/0.15$" "$work/light/track_light/AL1B.txt" && grep -q "^tunnel_lamp 1836 1847 220$" "$work/light/track_light/AL1B.txt" || fail "Save did not write the tunnel's lines: $(grep -n '^tunnel' "$work/light/track_light/AL1B.txt")"
[ "$(cksum < "$work/light/track/AL1.txt")" = "$places_before" ] || fail "an Own look changed the places file"
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --frames 30 --steps print-tunnels,tunnel-unown:1,print-tunnels,save > "$work/tunnels" 2>&1 ||
    fail "the Remove own look steps: $(tail -3 "$work/tunnels")"
grep -q "^tunnel 1: 1836-1847 place 1 own" "$work/tunnels" && grep -q "^tunnel 1: 1836-1847 place 1 as all" "$work/tunnels" || fail "Remove own look did not take the look back: $(grep '^tunnel ' "$work/tunnels")"
[ "$(cksum < "$work/light/track_light/AL1B.txt")" = "$before" ] || fail "after Remove own look the light file is not the one it was"
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --frames 30 --steps tunnel-unown:1 > "$work/tunnels" 2>&1 && fail "Remove own look on a tunnel with none went"
"$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --ask light --window 1280x800 --frames 40 --shot "$work/tunnels.png" > "$work/tunnels" 2>&1 || fail "the window with the Tunnels section: $(tail -3 "$work/tunnels")"
[ "$failures" = 0 ] && echo "ok"
exit "$failures"
