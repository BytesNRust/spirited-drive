#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Packs a built tnfs into "Spirited Drive.app", the track editor into "Spirited Drive Studio.app", and both into one DMG,
# each with SDL3 inside its bundle.
#
#   tools/package_macos.sh BUILD_DIR OUT_DIR [ICNS]
#
# BUILD_DIR holds the tnfs binary (configure it with -DTNFS_RELEASE=ON), tnfs-track-editor (built unless -DTNFS_EDITOR=OFF: without it
# the DMG has the game only) and generated/tnfs_version.c.
# OUT_DIR receives "Spirited Drive.app", "Spirited Drive Studio.app" and Spirited-Drive-<version>-macos.dmg.
# With REQUIRE_EDITOR=1 in the environment a missing editor binary is an error (a release must not ship without Studio).
# ICNS is the game's icon; without it, the source tree's data/icon/tnfs.icns (tools/spirited_icon.py makes it).
# STUDIO_ICNS is Studio's: the file in the environment, else editor/icon/studio.icns (tools/studio_icon.py makes it), else ICNS.
#
# No game data goes in: players bring their own CD. Our own data (data/ in the source tree: car masks, the
# environment map, light maps) goes in the game's Contents/Resources/data; Studio's generic art (editor/art/GENERIC.FAM) in its Contents/Resources. The apps are signed ad hoc (the beta is unsigned:
# Gatekeeper asks once, "Open Anyway").
set -eu

usage() { echo "usage: $0 BUILD_DIR OUT_DIR [ICNS]" >&2; exit 2; }
[ $# -ge 2 ] || usage
BUILD_DIR=$(cd "$1" && pwd)
OUT_DIR=$2
ICNS=${3:-}
SRC_DIR=$(cd "$(dirname "$0")/.." && pwd)
[ -n "$ICNS" ] || { [ -f "$SRC_DIR/data/icon/tnfs.icns" ] && ICNS="$SRC_DIR/data/icon/tnfs.icns"; } || true
STUDIO_ICNS=${STUDIO_ICNS:-}
[ -n "$STUDIO_ICNS" ] || { [ -f "$SRC_DIR/editor/icon/studio.icns" ] && STUDIO_ICNS="$SRC_DIR/editor/icon/studio.icns"; } || true
[ -n "$STUDIO_ICNS" ] || STUDIO_ICNS=$ICNS

APP_NAME="Spirited Drive"
STUDIO_NAME="Spirited Drive Studio"
BUNDLE_ID="com.bytesnrust.spirited-drive"
STUDIO_ID="com.bytesnrust.spirited-drive-studio"
MIN_MACOS="11.0"

BIN="$BUILD_DIR/tnfs"
EDITOR_BIN="$BUILD_DIR/tnfs-track-editor"
VERSION_H="$BUILD_DIR/generated/tnfs_version.c"
[ -x "$BIN" ] || { echo "no tnfs binary in $BUILD_DIR" >&2; exit 1; }
[ -f "$VERSION_H" ] || { echo "no generated/tnfs_version.c in $BUILD_DIR" >&2; exit 1; }
VERSION=$(sed -n 's/^const char tnfs_version\[\] = "\(.*\)";.*/\1/p' "$VERSION_H")
[ -n "$VERSION" ] || { echo "cannot read TNFS_VERSION from $VERSION_H" >&2; exit 1; }
# CFBundleShortVersionString takes only numbers: 0.9.0-beta.1 -> 0.9.0
SHORT_VERSION=$(printf '%s' "$VERSION" | sed 's/[-+].*//')

mkdir -p "$OUT_DIR"
OUT_DIR=$(cd "$OUT_DIR" && pwd)

# pack_app NAME BUNDLE_ID EXECUTABLE BINARY CATEGORY WITH_DATA
# One bundle in OUT_DIR: the binary as Contents/MacOS/EXECUTABLE, SDL3 beside it, the licences, the icon, an Info.plist; WITH_DATA=1 puts
# our own data/ in it (the game's). Nothing may point outside the bundle or the system; the bundle is signed ad hoc.
pack_app() {
    name=$1 id=$2 exe=$3 src=$4 category=$5 with_data=$6 icns=${7:-}
    app="$OUT_DIR/$name.app"
    rm -rf "$app"
    mkdir -p "$app/Contents/MacOS" "$app/Contents/Frameworks" "$app/Contents/Resources"
    cp "$src" "$app/Contents/MacOS/$exe"
    chmod u+w "$app/Contents/MacOS/$exe"

    # SDL3: copy whatever the binary links (Nix store, Homebrew or a CI build) and point the binary at the copy.
    sdl_path=$(otool -L "$src" | awk '/libSDL3[.0-9]*\.dylib/ {print $1; exit}')
    [ -n "$sdl_path" ] || { echo "$exe does not link SDL3?" >&2; exit 1; }
    case "$sdl_path" in
        @rpath/*)
            sdl_file=""
            for dir in $(otool -l "$src" | awk '/LC_RPATH/ {getline; getline; print $2}'); do
                [ -f "$dir/$(basename "$sdl_path")" ] && sdl_file="$dir/$(basename "$sdl_path")" && break
            done
            [ -n "$sdl_file" ] || { echo "cannot find $sdl_path on the binary's rpaths" >&2; exit 1; }
            ;;
        *) sdl_file=$sdl_path ;;
    esac
    cp "$sdl_file" "$app/Contents/Frameworks/libSDL3.0.dylib"
    chmod u+w "$app/Contents/Frameworks/libSDL3.0.dylib"
    install_name_tool -id "@rpath/libSDL3.0.dylib" "$app/Contents/Frameworks/libSDL3.0.dylib"
    install_name_tool -change "$sdl_path" "@executable_path/../Frameworks/libSDL3.0.dylib" "$app/Contents/MacOS/$exe"
    # The builder's rpaths (Nix store, CI temp dirs) mean nothing on a player's Mac: drop them.
    for rp in $(otool -l "$app/Contents/MacOS/$exe" | awk '/cmd LC_RPATH/ {getline; getline; print $2}' | sort -u); do   # a universal binary lists each rpath once per architecture
        install_name_tool -delete_rpath "$rp" "$app/Contents/MacOS/$exe"
    done

    # Our own data: only the files git tracks under data/ (a dev machine's untracked files never ship).
    if [ "$with_data" = 1 ]; then
        if git -C "$SRC_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
            (cd "$SRC_DIR" && git ls-files -z -- data | cpio -0 -pdm --quiet "$app/Contents/Resources")
        else
            echo "warning: $SRC_DIR is not a git checkout: no data/ in the bundle" >&2
        fi
    fi
    for doc in LICENSE NOTICE.md; do
        [ -f "$SRC_DIR/$doc" ] && cp "$SRC_DIR/$doc" "$app/Contents/Resources/"
    done
    if [ -d "$SRC_DIR/LICENSES" ]; then
        cp -R "$SRC_DIR/LICENSES" "$app/Contents/Resources/LICENSES"
    fi
    if [ "$exe" = tnfs-track-editor ]; then   # the generic art a new project copies (editor/art/README.md): without it Studio's first build is refused
        [ -f "$SRC_DIR/editor/art/GENERIC.FAM" ] || { echo "editor/art/GENERIC.FAM is missing: Studio would ship without its generic art" >&2; exit 1; }
        cp "$SRC_DIR/editor/art/GENERIC.FAM" "$app/Contents/Resources/GENERIC.FAM"
        # Convert PSD (Masks window) runs these two scripts with the Mac's python3 (psd-tools and Pillow are the user's: the button says so)
        mkdir -p "$app/Contents/Resources/tools"
        for script in car_mask_from_psd.py dash_mask_from_psd.py; do
            [ -f "$SRC_DIR/tools/$script" ] || { echo "tools/$script is missing: Studio's Convert PSD would not work" >&2; exit 1; }
            cp "$SRC_DIR/tools/$script" "$app/Contents/Resources/tools/$script"
        done
    fi
    icon_key=""
    if [ -n "$icns" ]; then
        icon_file=$(basename "$icns" .icns)
        cp "$icns" "$app/Contents/Resources/$icon_file.icns"
        icon_key="<key>CFBundleIconFile</key><string>$icon_file</string>"
    fi

    cat > "$app/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>$name</string>
    <key>CFBundleDisplayName</key><string>$name</string>
    <key>CFBundleIdentifier</key><string>$id</string>
    <key>CFBundleExecutable</key><string>$exe</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>$SHORT_VERSION</string>
    <key>CFBundleVersion</key><string>$VERSION</string>
    <key>LSMinimumSystemVersion</key><string>$MIN_MACOS</string>
    <key>LSApplicationCategoryType</key><string>$category</string>
    <key>NSHighResolutionCapable</key><true/>
    <key>NSHumanReadableCopyright</key><string>© 2026 Bytes &amp; Rust and contributors. GPL-3.0-or-later. Not affiliated with Electronic Arts.</string>
    $icon_key
</dict>
</plist>
EOF
    plutil -lint "$app/Contents/Info.plist" >/dev/null

    for f in "$app/Contents/MacOS/$exe" "$app/Contents/Frameworks/libSDL3.0.dylib"; do
        if otool -L "$f" | awk '/^\t/ {print $1}' | grep -v -E '^(/usr/lib/|/System/|@rpath/libSDL3|@executable_path/)'; then
            echo "$f links a library outside the bundle (above)" >&2
            exit 1
        fi
    done
    if strings "$app/Contents/MacOS/$exe" | grep -qF "$SRC_DIR"; then
        echo "$exe holds the source tree's path: configure with -DTNFS_RELEASE=ON" >&2
        exit 1
    fi

    codesign --force --sign - "$app/Contents/Frameworks/libSDL3.0.dylib"
    codesign --force --sign - "$app"
    codesign --verify --strict "$app"
}

pack_app "$APP_NAME" "$BUNDLE_ID" tnfs "$BIN" public.app-category.racing-games 1 "$ICNS"
APP="$OUT_DIR/$APP_NAME.app"
ARCHS=$(lipo -archs "$APP/Contents/MacOS/tnfs")

STUDIO=""
if [ -x "$EDITOR_BIN" ]; then
    pack_app "$STUDIO_NAME" "$STUDIO_ID" tnfs-track-editor "$EDITOR_BIN" public.app-category.developer-tools 0 "$STUDIO_ICNS"
    STUDIO="$OUT_DIR/$STUDIO_NAME.app"
    # one set of architectures for both apps: a universal game with an arm64-only editor would ship a Studio that fails on an Intel Mac
    sorted_archs() { lipo -archs "$1" | tr ' ' '\n' | sort | tr '\n' ' '; }
    [ "$(sorted_archs "$STUDIO/Contents/MacOS/tnfs-track-editor")" = "$(sorted_archs "$APP/Contents/MacOS/tnfs")" ] \
        || { echo "the editor's architectures differ from the game's ($ARCHS)" >&2; exit 1; }
elif [ "${REQUIRE_EDITOR:-0}" = 1 ]; then
    echo "no tnfs-track-editor in $BUILD_DIR (REQUIRE_EDITOR=1): build it, or configure without -DTNFS_EDITOR=OFF" >&2
    exit 1
else
    echo "no tnfs-track-editor in $BUILD_DIR: the DMG has the game only" >&2
fi

DMG="$OUT_DIR/Spirited-Drive-$VERSION-macos.dmg"
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
cp -R "$APP" "$STAGE/"
[ -z "$STUDIO" ] || cp -R "$STUDIO" "$STAGE/"
ln -s /Applications "$STAGE/Applications"
rm -f "$DMG"
hdiutil create -quiet -volname "$APP_NAME $VERSION" -srcfolder "$STAGE" -format UDZO -fs HFS+ "$DMG"

echo "app:  $APP ($ARCHS)"
[ -z "$STUDIO" ] || echo "app:  $STUDIO ($ARCHS)"
echo "dmg:  $DMG"
