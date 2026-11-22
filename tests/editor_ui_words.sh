#!/bin/sh
# Words Studio's interface never shows (owner's rule, 2026-11-20, widened 2026-11-22): it names no program of someone else's ("GIMP", "Blender", "Photoshop": say
# "an external image editor", "the 3D editor"). Checked in the strings of editor/*.c and editor/*.cpp, of studio/*.c (the commands' words), of the kit's README
# (core/formats/car_kit.c) and in docs/modding*; a comment may name one.
# usage: editor_ui_words.sh REPO
set -u
repo=$1
found=$(grep -n -i -E '"[^"]*(gimp|blender|photoshop)[^"]*"' "$repo"/editor/*.c "$repo"/editor/*.cpp "$repo"/studio/*.c "$repo"/core/formats/car_kit.c)
docs=$(grep -n -i -E 'gimp|blender|photoshop' "$repo"/docs/modding.md "$repo"/docs/modding/*.md)
if [ -n "$found" ] || [ -n "$docs" ]; then
    echo "FAIL: Studio's text names a program of someone else's (say \"an external image editor\", \"the 3D editor\"):" >&2
    echo "$found" >&2
    echo "$docs" >&2
    exit 1
fi
echo "editor_ui_words: ok"
