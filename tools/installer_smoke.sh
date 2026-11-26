#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# The Windows installer on a Windows machine (the CI runner is disposable; do not run this on a PC whose installed game you care about: it installs into, updates and
# removes %LOCALAPPDATA%\Programs\Spirited Drive and Spirited Drive's Start menu entry):
#   1. silent install into the default folder: the program, its data and the Start menu shortcut are there, and no administrator was needed
#   2. the installed program starts (--mods-list, no window, exit 0)
#   3. an update over it (the same installer run again) removes what the program carried before and the new one does not (a stale file in data\, a stale DLL, the old tnfs.exe),
#      and leaves what is not the program's (a file of the player's beside the program)
#   4. the uninstaller removes the program and the shortcut and keeps the player's folder (%APPDATA%\Spirited Drive, here a marker file)
#
#   tools/installer_smoke.sh SETUP.EXE        (MSYS2's shell; leaves nothing installed when it passes)
set -u
[ $# -eq 1 ] && [ -f "$1" ] || { echo "usage: $0 SETUP.EXE" >&2; exit 2; }
SETUP=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
export MSYS2_ARG_CONV_EXCL='*'   # /VERYSILENT and the like are options, not paths: MSYS must not turn them into paths
APP="$(cygpath -u "$LOCALAPPDATA")/Programs/Spirited Drive"
SHORTCUT="$(cygpath -u "$APPDATA")/Microsoft/Windows/Start Menu/Programs/Spirited Drive.lnk"
USER_DIR="$(cygpath -u "$APPDATA")/Spirited Drive"
WORK=$(mktemp -d "${TMPDIR:-/tmp}/installer_smoke.XXXXXX")
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
done_or_stop() { [ "$failures" -eq 0 ] || { echo "installer_smoke: $failures failure(s)" >&2; tail -40 "$WORK"/*.log 2>/dev/null >&2; exit 1; }; }

[ ! -e "$APP" ] || { echo "installer_smoke: $APP already exists: not touching a machine with the game installed" >&2; exit 2; }
[ ! -e "$USER_DIR" ] || { echo "installer_smoke: $USER_DIR already exists: not touching a machine with the game's data" >&2; exit 2; }

# 1. install
timeout -k 5 300 "$SETUP" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART "/LOG=$(cygpath -w "$WORK/install.log")"
rc=$?
[ "$rc" -eq 0 ] || fail "the installer ended with status $rc"
[ -f "$APP/Spirited Drive.exe" ] || fail "no Spirited Drive.exe in $APP"
[ -f "$APP/SDL3.dll" ] || fail "no SDL3.dll in $APP"
[ -d "$APP/data/car_masks" ] || fail "no data/car_masks in $APP"
[ -f "$APP/unins000.exe" ] || fail "no uninstaller in $APP"
[ -f "$SHORTCUT" ] || fail "no Start menu shortcut at $SHORTCUT"
# what Explorer may read as something it is not: no file of the install starts like a zip ("PK"), and the folder's top level is listed in the log
zips=$(find "$APP" -type f -exec sh -c 'for f; do [ "$(head -c 2 "$f")" = "PK" ] && echo "$f"; done' _ {} +)
[ -z "$zips" ] || fail "files of the install start like a zip (Explorer may ask for the last disk of a multi-volume set): $zips"
echo "installed folder (top level):"; ls -la "$APP"
done_or_stop

# 2. the installed program starts
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy
timeout -k 5 60 "$APP/Spirited Drive.exe" --user "$(cygpath -w "$WORK/user")" --mods-list > "$WORK/run.log" 2>&1
rc=$?
[ "$rc" -eq 0 ] || fail "the installed program ended with status $rc: $(tail -3 "$WORK/run.log")"
[ -s "$WORK/user/spirited-drive.log" ] || fail "the installed program left no log"
done_or_stop

# 3. an update over it (the same folder, no folder below it)
mkdir -p "$APP/data" && echo old > "$APP/data/stale.txt" && echo old > "$APP/stale.dll" && echo mine > "$APP/player-file.bin" && echo old > "$APP/tnfs.exe"
timeout -k 5 300 "$SETUP" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART "/LOG=$(cygpath -w "$WORK/update.log")"
rc=$?
[ "$rc" -eq 0 ] || fail "the update ended with status $rc"
[ -f "$APP/Spirited Drive.exe" ] && [ -d "$APP/data/car_masks" ] || fail "the update left the program incomplete"
[ ! -d "$APP/Spirited Drive" ] || fail "the update went one level below: $APP/Spirited Drive exists"   # same folder as the first install, no nesting
[ ! -e "$APP/data/stale.txt" ] || fail "the update kept data/stale.txt: what the new version does not carry stays"
[ ! -e "$APP/stale.dll" ] || fail "the update kept stale.dll"
[ ! -e "$APP/tnfs.exe" ] || fail "the update kept tnfs.exe (the program's old name: it would start the old version)"
[ -f "$APP/player-file.bin" ] || fail "the update deleted a file that is not the program's"
done_or_stop
rm -f "$APP/player-file.bin"

# 4. uninstall: the program goes, the player's folder stays
mkdir -p "$USER_DIR" && echo kept > "$USER_DIR/marker.txt"
timeout -k 5 300 "$APP/unins000.exe" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART "/LOG=$(cygpath -w "$WORK/uninstall.log")"
for _ in $(seq 1 60); do [ -e "$APP/Spirited Drive.exe" ] || break; sleep 1; done   # the uninstaller's first process may return before its second one has finished
[ ! -e "$APP/Spirited Drive.exe" ] || fail "the uninstaller left the program in $APP"
[ ! -e "$APP/data" ] || fail "the uninstaller left $APP/data"
[ ! -e "$SHORTCUT" ] || fail "the uninstaller left the Start menu shortcut"
[ -f "$USER_DIR/marker.txt" ] || fail "the uninstaller touched the player's folder: $USER_DIR/marker.txt is gone"
rm -rf "$USER_DIR"
done_or_stop
rm -rf "$WORK"
echo "installer_smoke: ok"
