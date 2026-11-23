#!/bin/sh
# Owner's rule (2026-11-20): no loose explanatory text in the panels; the explanation is behind a (?) marker or a tooltip, and only state captions are on screen. A heuristic over the
# car panels and the Track light panels: a text call (TextWrapped, TextDisabled, TextUnformatted, muted, muted_wrapped) given a plain sentence of seven words or more, with no
# number or name put into it, is an explanation, not a state. The few sentences that are a notice or an empty state are listed below, each with its reason.
# usage: editor_loose_text.sh REPO
set -u
repo=$1
files="car_panels.cpp preview_panel.cpp model_panel.cpp mask_panel.cpp changes_panel.cpp drive_panel.cpp light_panel.cpp light_track_panel.cpp light_compare_panel.cpp"
# allowed: the project's notice (it must stay in view), the start screen's tagline, a dialog's question, a state that names what to do
allow="do not share|Make cars and tracks for|Lost Vegas has no time of day|the game's own cars only|Every mask is the game's own|Choose a texture to see|already has a card.png|Regenerating replaces|Running: the game is loading"
found=""
for f in $files; do
    hits=$(grep -nE '(TextWrapped|TextDisabled|TextUnformatted|muted_wrapped|muted)\("[^"%]*( [^" %]+){6,}"' "$repo/editor/$f" | grep -vE "$allow" || true)
    [ -z "$hits" ] || found="$found
$(echo "$hits" | sed "s|^|$f:|")"
done
if [ -n "$found" ]; then
    echo "FAIL: loose explanatory text in a panel (put it behind a (?) marker or a tooltip, or list it in tests/editor_loose_text.sh with its reason):$found" >&2
    exit 1
fi
echo "editor_loose_text: ok"
