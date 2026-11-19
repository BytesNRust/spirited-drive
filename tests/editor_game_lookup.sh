#!/bin/sh
# Studio finds the game when both apps sit in one Applications folder (the DMG's layout), whatever its working directory is (from the Finder it is /), and
# says what it looked at when the game is not beside it. The apps are copies in a scratch Applications folder; the game is a script that records how it was run.
# usage: editor_game_lookup.sh EDITOR GENERIC_FAM     (headless)
set -u
editor=$1
fam=$2
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }

pack() {   # pack DIR: Spirited Drive Studio.app and Spirited Drive.app in DIR, as tools/package_macos.sh lays them out
    studio="$1/Spirited Drive Studio.app"
    game="$1/Spirited Drive.app"
    mkdir -p "$studio/Contents/MacOS" "$studio/Contents/Resources" "$game/Contents/MacOS"
    cp "$editor" "$studio/Contents/MacOS/tnfs-track-editor"
    cp "$fam" "$studio/Contents/Resources/GENERIC.FAM"
    for app in "$studio" "$game"; do
        exe=tnfs; [ "$app" = "$studio" ] && exe=tnfs-track-editor
        printf '<?xml version="1.0" encoding="UTF-8"?>\n<plist version="1.0"><dict><key>CFBundleExecutable</key><string>%s</string><key>CFBundleIdentifier</key><string>x.%s</string><key>CFBundlePackageType</key><string>APPL</string></dict></plist>\n' "$exe" "$exe" > "$app/Contents/Info.plist"
    done
    printf '#!/bin/sh\necho "$@" > "%s/ran"\n' "$work" > "$game/Contents/MacOS/tnfs"
    chmod +x "$game/Contents/MacOS/tnfs"
}

run() {   # run STUDIO_BINARY PROJECT_NAME: the headless build + install from the working directory /, as the Finder starts an app
    (cd / && "$1" "$work/docs/$2" --user "$work/user" --frames 2 --build --install) >"$work/log_$2" 2>&1
}

# 1. both apps in one Applications folder, started from /
mkdir -p "$work/Applications" "$work/docs"
pack "$work/Applications"
rm -f "$work/ran"
run "$work/Applications/Spirited Drive Studio.app/Contents/MacOS/tnfs-track-editor" one || fail "install from the Applications layout: $(cat "$work/log_one")"
[ -s "$work/ran" ] || fail "the game was not run: $(cat "$work/log_one")"
grep -q -- "--install" "$work/ran" 2>/dev/null || fail "the game was run without --install: $(cat "$work/ran" 2>/dev/null)"
grep -q "was not found" "$work/log_one" && fail "Studio said the game was not found: $(cat "$work/log_one")"

# 2. the same layout in a folder with spaces and a symlink in its path (a volume, ~/Applications, a folder dragged anywhere)
mkdir -p "$work/My Apps/Games" "$work/links"
pack "$work/My Apps/Games"
ln -s "$work/My Apps/Games" "$work/links/apps"
rm -f "$work/ran"
run "$work/links/apps/Spirited Drive Studio.app/Contents/MacOS/tnfs-track-editor" two || fail "install from a folder with spaces and a link: $(cat "$work/log_two")"
[ -s "$work/ran" ] || fail "the game was not run from the second layout: $(cat "$work/log_two")"

# 3. Studio alone: the install says what it looked at and what to do
mkdir -p "$work/Alone"
studio="$work/Alone/Spirited Drive Studio.app"
mkdir -p "$studio/Contents/MacOS" "$studio/Contents/Resources"
cp "$editor" "$studio/Contents/MacOS/tnfs-track-editor"
cp "$fam" "$studio/Contents/Resources/GENERIC.FAM"
rm -f "$work/ran"
run "$studio/Contents/MacOS/tnfs-track-editor" three && [ ! -e /Applications/"Spirited Drive.app" ] && fail "an install with no game beside it was accepted"
[ ! -e "$work/ran" ] || fail "a game ran that should not exist"

[ "$failures" -eq 0 ] && echo "editor_game_lookup: ok"
exit "$failures"
