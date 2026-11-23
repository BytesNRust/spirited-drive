#!/bin/sh
# A panel that draws from the clock moves by itself, and a window at rest draws only twice a second (editor/idle.h): such a panel must be named in app_animating (editor/idle.c) or it
# would stutter at 2 frames a second. This fails when a file of editor/ uses the clock (ImGui::GetTime, SDL_GetTicks, DeltaTime) and is not one of the two listed, each with its entry in
# app_animating: model_panel.cpp (Auto-rotate, Slow wheels, police lights) and preview_panel.cpp (police lights). A new one: add it to app_animating, then to the list here.
# usage: editor_animated_panels.sh REPO
set -u
repo=$1
known="model_panel.cpp preview_panel.cpp"
found=""
for f in "$repo"/editor/*.cpp; do
    b=$(basename "$f")
    case " $known " in *" $b "*) continue;; esac
    grep -nE 'ImGui::GetTime\(\)|SDL_GetTicks\(|DeltaTime' "$f" > /dev/null 2>&1 && found="$found $b"
done
if [ -n "$found" ]; then
    echo "FAIL: a panel draws from the clock but is not named in app_animating (editor/idle.c), so a window at rest draws it twice a second:$found" >&2
    exit 1
fi
echo "editor_animated_panels: ok"
