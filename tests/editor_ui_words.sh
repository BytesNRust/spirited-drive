#!/bin/sh
# Words Studio's interface never shows (owner's rule, 2026-11-20): it names no program of someone else's ("GIMP": say "an external image editor"). Checked in the
# strings of editor/*.c and editor/*.cpp; a comment may name one.
# usage: editor_ui_words.sh REPO
set -u
repo=$1
found=$(grep -n -i '"[^"]*gimp[^"]*"' "$repo"/editor/*.c "$repo"/editor/*.cpp)
if [ -n "$found" ]; then
    echo "FAIL: Studio's text names GIMP (say \"an external image editor\"):" >&2
    echo "$found" >&2
    exit 1
fi
echo "editor_ui_words: ok"
