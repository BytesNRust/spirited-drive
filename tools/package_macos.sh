#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Packs a built tnfs into "TNFS Restomod.app" and a DMG, with SDL3 inside the bundle.
#
#   tools/package_macos.sh BUILD_DIR OUT_DIR [ICNS]
#
# BUILD_DIR holds the tnfs binary (configure it with -DTNFS_RELEASE=ON) and generated/tnfs_version.h.
# OUT_DIR receives "TNFS Restomod.app" and TNFS-Restomod-<version>-macos.dmg.
# ICNS is the app icon (optional until we have our own).
#
# No game data goes in: players bring their own CD. Our own data (data/ in the source tree: car masks, the
# environment map, light maps) goes in Contents/Resources/data. The app is signed ad hoc (the beta is unsigned:
# Gatekeeper asks once, "Open Anyway").
set -eu

usage() { echo "usage: $0 BUILD_DIR OUT_DIR [ICNS]" >&2; exit 2; }
[ $# -ge 2 ] || usage
BUILD_DIR=$(cd "$1" && pwd)
OUT_DIR=$2
ICNS=${3:-}
SRC_DIR=$(cd "$(dirname "$0")/.." && pwd)

APP_NAME="TNFS Restomod"
BUNDLE_ID="com.bytesnrust.tnfs-restomod"
MIN_MACOS="11.0"

BIN="$BUILD_DIR/tnfs"
VERSION_H="$BUILD_DIR/generated/tnfs_version.h"
[ -x "$BIN" ] || { echo "no tnfs binary in $BUILD_DIR" >&2; exit 1; }
[ -f "$VERSION_H" ] || { echo "no generated/tnfs_version.h in $BUILD_DIR" >&2; exit 1; }
VERSION=$(sed -n 's/^#define TNFS_VERSION "\(.*\)".*/\1/p' "$VERSION_H")
[ -n "$VERSION" ] || { echo "cannot read TNFS_VERSION from $VERSION_H" >&2; exit 1; }
# CFBundleShortVersionString takes only numbers: 0.9.0-beta.1 -> 0.9.0
SHORT_VERSION=$(printf '%s' "$VERSION" | sed 's/[-+].*//')

mkdir -p "$OUT_DIR"
OUT_DIR=$(cd "$OUT_DIR" && pwd)
APP="$OUT_DIR/$APP_NAME.app"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Frameworks" "$APP/Contents/Resources"

cp "$BIN" "$APP/Contents/MacOS/tnfs"
chmod u+w "$APP/Contents/MacOS/tnfs"

# SDL3: copy whatever the binary links (Nix store, Homebrew or a CI build) and point the binary at the copy.
SDL_PATH=$(otool -L "$BIN" | awk '/libSDL3[.0-9]*\.dylib/ {print $1; exit}')
[ -n "$SDL_PATH" ] || { echo "tnfs does not link SDL3?" >&2; exit 1; }
case "$SDL_PATH" in
    @rpath/*)
        SDL_FILE=""
        for dir in $(otool -l "$BIN" | awk '/LC_RPATH/ {getline; getline; print $2}'); do
            [ -f "$dir/$(basename "$SDL_PATH")" ] && SDL_FILE="$dir/$(basename "$SDL_PATH")" && break
        done
        [ -n "$SDL_FILE" ] || { echo "cannot find $SDL_PATH on the binary's rpaths" >&2; exit 1; }
        ;;
    *) SDL_FILE=$SDL_PATH ;;
esac
cp "$SDL_FILE" "$APP/Contents/Frameworks/libSDL3.0.dylib"
chmod u+w "$APP/Contents/Frameworks/libSDL3.0.dylib"
install_name_tool -id "@rpath/libSDL3.0.dylib" "$APP/Contents/Frameworks/libSDL3.0.dylib"
install_name_tool -change "$SDL_PATH" "@executable_path/../Frameworks/libSDL3.0.dylib" "$APP/Contents/MacOS/tnfs"
# The builder's rpaths (Nix store, CI temp dirs) mean nothing on a player's Mac: drop them.
for rp in $(otool -l "$APP/Contents/MacOS/tnfs" | awk '/cmd LC_RPATH/ {getline; getline; print $2}'); do
    install_name_tool -delete_rpath "$rp" "$APP/Contents/MacOS/tnfs"
done

# Our own data: only the files git tracks under data/ (a dev machine's untracked files never ship).
if git -C "$SRC_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    (cd "$SRC_DIR" && git ls-files -z -- data | cpio -0 -pdm --quiet "$APP/Contents/Resources")
else
    echo "warning: $SRC_DIR is not a git checkout: no data/ in the bundle" >&2
fi
for doc in LICENSE NOTICE.md; do
    [ -f "$SRC_DIR/$doc" ] && cp "$SRC_DIR/$doc" "$APP/Contents/Resources/"
done
if [ -d "$SRC_DIR/LICENSES" ]; then
    cp -R "$SRC_DIR/LICENSES" "$APP/Contents/Resources/LICENSES"
fi
ICON_KEY=""
if [ -n "$ICNS" ]; then
    cp "$ICNS" "$APP/Contents/Resources/tnfs.icns"
    ICON_KEY="<key>CFBundleIconFile</key><string>tnfs</string>"
fi

cat > "$APP/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>$APP_NAME</string>
    <key>CFBundleDisplayName</key><string>$APP_NAME</string>
    <key>CFBundleIdentifier</key><string>$BUNDLE_ID</string>
    <key>CFBundleExecutable</key><string>tnfs</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>$SHORT_VERSION</string>
    <key>CFBundleVersion</key><string>$VERSION</string>
    <key>LSMinimumSystemVersion</key><string>$MIN_MACOS</string>
    <key>LSApplicationCategoryType</key><string>public.app-category.racing-games</string>
    <key>NSHighResolutionCapable</key><true/>
    <key>NSHumanReadableCopyright</key><string>© 2026 Bytes &amp; Rust and contributors. GPL-3.0-or-later. Not affiliated with Electronic Arts.</string>
    $ICON_KEY
</dict>
</plist>
EOF
plutil -lint "$APP/Contents/Info.plist" >/dev/null

# Nothing may point outside the bundle or the system.
for f in "$APP/Contents/MacOS/tnfs" "$APP/Contents/Frameworks/libSDL3.0.dylib"; do
    if otool -L "$f" | tail -n +2 | awk '{print $1}' | grep -v -E '^(/usr/lib/|/System/|@rpath/libSDL3|@executable_path/)'; then
        echo "$f links a library outside the bundle (above)" >&2
        exit 1
    fi
done
if strings "$APP/Contents/MacOS/tnfs" | grep -q "$SRC_DIR"; then
    echo "the binary holds the source tree's path: configure with -DTNFS_RELEASE=ON" >&2
    exit 1
fi

codesign --force --sign - "$APP/Contents/Frameworks/libSDL3.0.dylib"
codesign --force --sign - "$APP"
codesign --verify --strict "$APP"

ARCHS=$(lipo -archs "$APP/Contents/MacOS/tnfs")
DMG="$OUT_DIR/TNFS-Restomod-$VERSION-macos.dmg"
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
cp -R "$APP" "$STAGE/"
ln -s /Applications "$STAGE/Applications"
rm -f "$DMG"
hdiutil create -quiet -volname "$APP_NAME $VERSION" -srcfolder "$STAGE" -format UDZO -fs HFS+ "$DMG"

echo "app:  $APP ($ARCHS)"
echo "dmg:  $DMG"
