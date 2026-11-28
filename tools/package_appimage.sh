#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Packs a built tnfs into Spirited-Drive-<version>-linux-x86_64.AppImage, with the same README beside it.
#
#   tools/package_appimage.sh BUILD_DIR OUT_DIR
#
# Needs APPIMAGETOOL (the appimagetool AppImage) and APPIMAGE_RUNTIME (the type2 runtime put in front of the file), both pinned and their SHA256
# checked by the workflow, and SDL_LICENSE (SDL's LICENSE.txt, from the sources it was built from).
# BUILD_DIR holds tnfs (configure it with -DTNFS_RELEASE=ON, SDL3 linked statically, on the oldest system to support: its glibc is the minimum) and
# generated/tnfs_version.c. The AppImage holds usr/bin/tnfs, our own data/ beside it (only what git tracks, where the game reads it: SDL_GetBasePath),
# the licences and the README. No libraries travel with it: SDL3 is inside the program and opens the system's X11, Wayland and sound libraries itself.
# No game data: players bring their own CD.
# The working folder is a new OUT_DIR/stage.XXXXXX each run and is left there: this script removes nothing.
set -eu

[ $# -eq 2 ] || { echo "usage: $0 BUILD_DIR OUT_DIR" >&2; exit 2; }
BUILD_DIR=$(cd "$1" && pwd)
mkdir -p "$2"
OUT_DIR=$(cd "$2" && pwd)
SRC_DIR=$(cd "$(dirname "$0")/.." && pwd)
EXE="$BUILD_DIR/tnfs"
VERSION_C="$BUILD_DIR/generated/tnfs_version.c"
: "${APPIMAGETOOL:?set APPIMAGETOOL to the appimagetool AppImage}"
: "${APPIMAGE_RUNTIME:?set APPIMAGE_RUNTIME to the type2 runtime file}"
: "${SDL_LICENSE:?set SDL_LICENSE to the LICENSE.txt of the SDL sources the program was built with}"
[ -f "$EXE" ] || { echo "no tnfs in $BUILD_DIR" >&2; exit 1; }
[ -f "$VERSION_C" ] || { echo "no generated/tnfs_version.c in $BUILD_DIR" >&2; exit 1; }
[ -f "$SDL_LICENSE" ] || { echo "no SDL licence at $SDL_LICENSE" >&2; exit 1; }
VERSION=$(sed -n 's/^const char tnfs_version\[\] = "\(.*\)";.*/\1/p' "$VERSION_C")
[ -n "$VERSION" ] || { echo "cannot read the version from $VERSION_C" >&2; exit 1; }

# SDL must be inside the program: a libSDL3.so it wants from the system is missing on most of the distributions it is for
if readelf -d "$EXE" | grep -q 'NEEDED.*libSDL3'; then
    echo "$EXE wants a libSDL3.so from the system: build SDL3 with SDL_STATIC=ON SDL_SHARED=OFF" >&2
    exit 1
fi

STAGE=$(mktemp -d "$OUT_DIR/stage.XXXXXX")
APP="$STAGE/SpiritedDrive.AppDir"
DOC="$APP/usr/share/doc/spirited-drive"
mkdir -p "$APP/usr/bin" "$APP/usr/share/applications" "$DOC/LICENSES/third-party/SDL3"
chmod 755 "$APP"
cp "$EXE" "$APP/usr/bin/tnfs"
strip "$APP/usr/bin/tnfs"

# our own data: only the files git tracks under data/ (a dev machine's untracked files never ship)
git -C "$SRC_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1 || { echo "$SRC_DIR is not a git checkout: no data/ in the package" >&2; exit 1; }
(cd "$SRC_DIR" && git ls-files -z -- data | xargs -0 -I{} sh -c 'mkdir -p "$1/$(dirname "$2")" && cp "$2" "$1/$2"' _ "$APP/usr/bin" {})
[ -d "$APP/usr/bin/data/car_masks" ] || { echo "the package has no data/car_masks" >&2; exit 1; }

for doc in LICENSE NOTICE.md; do cp "$SRC_DIR/$doc" "$DOC/"; done
cp "$SRC_DIR"/LICENSES/* "$DOC/LICENSES/"
cp "$SDL_LICENSE" "$DOC/LICENSES/third-party/SDL3/LICENSE.txt"

for size in 16 32 48 64 128 256 512; do
    dir="$APP/usr/share/icons/hicolor/${size}x${size}/apps"
    mkdir -p "$dir"
    cp "$SRC_DIR/data/icon/spirited_$size.png" "$dir/spirited-drive.png"
done
cp "$SRC_DIR/data/icon/spirited_256.png" "$APP/spirited-drive.png"
ln -s spirited-drive.png "$APP/.DirIcon"

cat > "$APP/spirited-drive.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Spirited Drive
Comment=The Need for Speed SE (1996), rebuilt to run natively. Bring your own CD.
Exec=tnfs
Icon=spirited-drive
Categories=Game;
Terminal=false
StartupWMClass=tnfs
X-AppImage-Version=$VERSION
EOF
cp "$APP/spirited-drive.desktop" "$APP/usr/share/applications/"

# the program runs from the folder it is called from, so a relative path on the command line (--user, --install) means what the player typed
cat > "$APP/AppRun" <<'EOF'
#!/bin/sh
here=$(dirname "$(readlink -f "$0")")
exec "$here/usr/bin/tnfs" "$@"
EOF
chmod +x "$APP/AppRun"

APPIMAGE_NAME="Spirited-Drive-$VERSION-linux-x86_64.AppImage"
README="$OUT_DIR/README-linux.txt"
# the text follows the approved wording of the README and the release notes (tests/readme_wording.py checks this file too)
cat > "$README" <<EOF
Spirited Drive $VERSION (Linux, x86_64)

The Need for Speed SE (1996), rebuilt to run natively on today's computers. Free and open source.

How to start
1. Put $APPIMAGE_NAME in any folder.
2. Make it executable: in the file manager, Properties, allow running it as a program; or in a terminal: chmod +x $APPIMAGE_NAME
3. Run it: double click, or ./$APPIMAGE_NAME in a terminal. You do not need libfuse2 (libfuse2t64). If it stops with a FUSE message, run it with ./$APPIMAGE_NAME --appimage-extract-and-run, or install your system's fuse3 package (sudo apt install fuse3).
4. On the first start, point the game at your own CD, a folder with its files, or an ISO image of your own disc. It copies what it needs, and you can put the CD away.

The first start's windows are your system's: the file chooser needs your desktop's portal (xdg-desktop-portal) or zenity. Desktops normally have one of them. If no window appears or no file chooser opens, install zenity with your package manager (sudo apt install zenity on Ubuntu, Mint, Zorin or elementary; sudo dnf install zenity on Fedora) and start again, or give the folder yourself: ./$APPIMAGE_NAME --data /path/to/the/game/files

Pads and wheels. Xbox, PlayStation and Switch pads normally work with nothing to install, plugged in before or while the game runs. Set the buttons in Options, Controls setup. If Steam is running, it may also turn the pad into keys or mouse moves: close Steam, or add the AppImage to Steam as a non-Steam game and start it from there. Rumble and force feedback need write access to the device; the log says "no write access" when it is missing.
Logitech G25, G27, G29 and Driving Force wheels: without access to the wheel's raw device, the system's own driver gives the knocks and the steady push but no vibrations. To give access, make the file /etc/udev/rules.d/70-spirited-drive-wheel.rules (as root) with this one line, then unplug the wheel and plug it again. The line is for a G29 (c24f); for another wheel put its own number, which lsusb shows (G27 c29b, G25 c299, Driving Force GT c29a):
KERNEL=="hidraw*", ATTRS{idVendor}=="046d", ATTRS{idProduct}=="c24f", TAG+="uaccess"
Thrustmaster and Fanatec wheels may need a driver your distribution does not include before they give force feedback.

Sound goes through your system's PipeWire or PulseAudio, or ALSA when neither answers. The log's "audio driver" line says which one. To choose another: SDL_AUDIO_DRIVER=pulseaudio ./$APPIMAGE_NAME (or pipewire, or alsa).

To have it in your applications menu with its icon, add it with an AppImage manager (Gear Lever or AppImageLauncher, for example); on its own an AppImage does not add itself there.

Bring your own CD. Spirited Drive contains no game data: you need your own copy of The Need for Speed Special Edition for PC (1996).

Three ways to play: Stock '96 (the 1996 game as it shipped), Restored '96 (the same game on a modern screen) and Restomod '26 (weather, four times of day, night lighting, cars that shine and more, all of it still 8-bit, palette-indexed art). Not sure? Take Restored '96. You can change it at any time on the Game Mode screen.

This Linux version is a first test build. It needs a 64-bit PC and a distribution from 2022 or newer (glibc 2.35, as in Ubuntu 22.04).

Your player folder is ~/.local/share/Spirited Drive/ (\$XDG_DATA_HOME/Spirited Drive/ if you set that variable). It keeps your settings, your records and your copy of the game's files, and they stay when you replace the AppImage with a newer one. To keep them somewhere else, a portable install for instance, start the game with --user and a folder: ./$APPIMAGE_NAME --user /path/to/folder
The update notice uses your system's curl; without it, or without internet, the game starts as usual and shows no notice.
The log, spirited-drive.log, is in that folder too. In the game: Customize Game, page 3, Open the log folder. Attach it when you report a bug, with the name and version of your distribution: https://github.com/BytesNRust/spirited-drive/issues (please do not attach files from the original game).

Licences are inside the AppImage, under usr/share/doc/spirited-drive/ (see them with --appimage-extract).

This game is not finished and is not meant to be: what you find shapes what comes next.
EOF
cp "$README" "$DOC/README.txt"

# --appimage-extract-and-run: the tool is an AppImage itself and a build machine may have no FUSE
ARCH=x86_64 "$APPIMAGETOOL" --appimage-extract-and-run --no-appstream --runtime-file "$APPIMAGE_RUNTIME" "$APP" "$OUT_DIR/$APPIMAGE_NAME"
echo "$OUT_DIR/$APPIMAGE_NAME"
