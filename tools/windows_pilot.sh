#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# The Windows pilot: on a Windows PC that has the CD's files in original/, build the game and run every test once, into one log. Run it in the MSYS2 UCRT64 shell,
# from the checkout:  bash tools/windows_pilot.sh
# It writes only inside the checkout, under build/pilot: the build, the temporary files, the folders the game and the tests would put in the user's profile, and the log.
# It removes nothing and asks for no administrator rights. The log can hold names and bytes read from the CD: send it privately, never publish it.
# PILOT_WITHOUT_CD=1 runs the same flow without original/ (how CI tries this script).
set -euo pipefail

say() { printf '== %s\n' "$*"; }
die() { printf 'windows_pilot: %s\n' "$*" >&2; exit 1; }

repo=$(cd "$(dirname "$0")/.." && pwd)
[ -f "${repo:?}/CMakeLists.txt" ] || die "run it from the checkout: $repo has no CMakeLists.txt"
[ "${MSYSTEM:-}" = UCRT64 ] || die "open the 'MSYS2 UCRT64' shell (MSYSTEM is '${MSYSTEM:-unset}')"

missing=
for tool in gcc cmake ninja python zsh unzip zip timeout git diff rsvg-convert; do
    command -v "$tool" >/dev/null 2>&1 || missing="$missing $tool"
done
python -c 'import PIL' >/dev/null 2>&1 || missing="$missing python-pillow"
[ -z "$missing" ] || die "missing:$missing (see the pacman line in the instructions)"

cd_state="with the CD"
if [ "${PILOT_WITHOUT_CD:-0}" = 1 ]; then
    cd_state="WITHOUT the CD (a try of the script)"
else
    [ -d "${repo:?}/original/SIMDATA" ] || die "no $repo/original/SIMDATA: copy the CD's folders into original/ first"
fi

out=${repo:?}/build/pilot
mkdir -p "${out:?}/tmp" "${out:?}/pref" "${out:?}/profile/Roaming" "${out:?}/profile/Local" "${out:?}/home"
log=${out:?}/piloto-windows.log
: > "${log:?}"

# the game's user folder is asked of Windows itself when a run names none: noted before and after, so that a folder made outside the checkout is seen
real_appdata=
if [ -n "${APPDATA:-}" ]; then real_appdata=$(cygpath -u "$APPDATA"); fi
profile_folder() {
    if [ -n "$real_appdata" ] && [ -e "$real_appdata/Spirited Drive" ]; then echo present; else echo absent; fi
}
before=$(profile_folder)

# everything a test or the game would put elsewhere, inside build/pilot
export TMPDIR="${out:?}/tmp" TMP="${out:?}/tmp" TEMP="${out:?}/tmp"
export HOME="${out:?}/home"
export TNFS_PREF_DIR="${out:?}/pref/"
APPDATA=$(cygpath -w "${out:?}/profile/Roaming")
LOCALAPPDATA=$(cygpath -w "${out:?}/profile/Local")
export APPDATA LOCALAPPDATA
export USERPROFILE="$APPDATA"
export PYTHONDONTWRITEBYTECODE=1 CCACHE_DISABLE=1
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy

jobs=$(nproc)
{
    echo "Spirited Drive Windows pilot, $cd_state"
    date -u '+%Y-%m-%d %H:%M UTC'
    echo "commit: $(git -C "${repo:?}" rev-parse HEAD)"
    echo "system: $(uname -sr), $jobs processors"
    gcc --version | head -1
    cmake --version | head -1
    python --version
    echo "game's folder in the real profile before: $before"
    if [ -e /c/tmp ] || [ -e "$(pwd -W | cut -c1)":/tmp ]; then echo "a \\tmp folder exists at a drive's root: some tests write there"; fi
} 2>&1 | tee -a "$log"

say "configure"
configured=0
cmake -S "${repo:?}" -B "${out:?}/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DTNFS_EDITOR=OFF 2>&1 | tee -a "$log" || configured=$?
[ "$configured" = 0 ] || die "cmake failed ($configured): send $log"

say "build (keeps going after an error)"
built=0
cmake --build "${out:?}/build" -- -k 0 2>&1 | tee -a "$log" || built=$?

say "tests (long: the CD tests run the game)"
tested=0
ctest --test-dir "${out:?}/build" --output-on-failure --timeout 900 -j "$jobs" 2>&1 | tee -a "$log" || tested=$?

{
    echo
    echo "== pilot summary"
    echo "build exit: $built    ctest exit: $tested"
    echo "warnings in the build: $(grep -c 'warning:' "$log" || true)"
    echo "tests registered: $(ctest --test-dir "${out:?}/build" -N | grep -c 'Test *#' || true)"
    echo "game's folder in the real profile before: $before, after: $(profile_folder)"
} 2>&1 | tee -a "$log"

# the checkout's own path, in both spellings, out of the copy to send: it usually holds the Windows user name
send=${out:?}/piloto-windows-enviar.log
sed -e "s|$(cygpath -m "${repo:?}")|<checkout>|g" -e "s|$(cygpath -w "${repo:?}" | sed 's/\\/\\\\/g')|<checkout>|g" -e "s|${repo:?}|<checkout>|g" "$log" > "${send:?}"
(cd "${out:?}" && zip -q piloto-windows-enviar.zip piloto-windows-enviar.log)
say "done. Send privately: $send (or the .zip beside it). Do not publish it: it can hold data read from the CD."
