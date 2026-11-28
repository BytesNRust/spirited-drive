#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Runs the AppImage in a real Linux window and takes pictures of the screen: X11 (Xvfb with openbox) and Wayland (weston, headless), sound off. It runs in a
# throwaway Ubuntu 24.04 container with no network, so nothing of the game is sent anywhere.
#
#   tools/appimage_desktop_smoke.sh APPIMAGE OUT_DIR [DATA_DIR]
#
# Without DATA_DIR (CI: no game data exists there) it reaches the first-start dialog and, with Return, the folder chooser.
# With DATA_DIR (a developer's own copy of the CD, mounted read-only) it reaches the boot screen and what Return brings after it.
# Xvfb has no window manager of its own, and without one a full screen request is ignored (the game stays 854x480): openbox runs there, and with DATA_DIR the window
# must be the screen's size. On Wayland the pictures are for the eye: nothing there tells where a window was put.
# OUT_DIR gets the screen pictures (PNG) and the game's output of each run. The pictures with DATA_DIR show the game's art: they stay out of the repo.
set -eu

IMAGE=spirited-drive-desktop-smoke

if [ "${1:-}" != --inside ]; then
    [ $# -ge 2 ] || { echo "usage: $0 APPIMAGE OUT_DIR [DATA_DIR]" >&2; exit 2; }
    APPIMAGE=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
    mkdir -p "$2"
    OUT=$(cd "$2" && pwd)
    # the script goes in as an argument, not as a mounted file: OrbStack served an earlier version of a mounted script after it was edited
    self=$(cat "$0")
    [ -f "$APPIMAGE" ] || { echo "no AppImage at $APPIMAGE" >&2; exit 1; }
    # what a desktop already has (the libraries SDL opens, a font, zenity for the choosers) plus the two display servers and the picture tools
    docker build -q -t "$IMAGE" - <<'EOF'
FROM ubuntu:24.04
RUN apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
    xvfb openbox xauth xdotool x11-utils xfonts-base imagemagick weston zenity fonts-dejavu-core \
    libx11-6 libxext6 libxrandr2 libxcursor1 libxi6 libxfixes3 libxss1 libxtst6 libxkbcommon0 \
    libwayland-client0 libwayland-cursor0 libwayland-egl1 libdecor-0-0 libegl1 libgl1 libgl1-mesa-dri libasound2t64 >/dev/null
EOF
    if [ $# -ge 3 ]; then
        DATA=$(cd "$3" && pwd)
        docker run --rm --network none -v "$APPIMAGE:/sd.AppImage:ro" -v "$OUT:/out" -v "$DATA:/gamedata:ro" "$IMAGE" sh -c "$self" smoke --inside
    else
        docker run --rm --network none -v "$APPIMAGE:/sd.AppImage:ro" -v "$OUT:/out" "$IMAGE" sh -c "$self" smoke --inside
    fi
    exit 0
fi

# ---- inside the container ----
cd /tmp
cp /sd.AppImage sd.AppImage
chmod +x sd.AppImage
./sd.AppImage --appimage-extract >/dev/null
APP=/tmp/squashfs-root/AppRun
# a desktop always has a UTF-8 locale: without one zenity refuses the buttons' "..." and no dialog comes up
export LANG=C.UTF-8 SDL_AUDIO_DRIVER=dummy XDG_RUNTIME_DIR=/tmp/run
mkdir -m 700 run
status=0
fail() { echo "FAIL: $1" >&2; status=1; }

# until SECONDS COMMAND...: the command tried twice a second until it succeeds
until_ok() {
    tries=$(($1 * 2))
    shift
    while [ "$tries" -gt 0 ]; do
        "$@" >/dev/null 2>&1 && return 0
        tries=$((tries - 1))
        sleep 0.5
    done
    return 1
}

# a picture that is one flat colour is a window that drew nothing
drawn() {
    [ -s "$1" ] || { fail "no picture $1"; return; }
    colours=$(identify -format %k "$1")
    echo "$(basename "$1"): $(identify -format %wx%h "$1"), $colours colours"
    [ "$colours" -gt 8 ] || fail "$(basename "$1") shows next to nothing ($colours colours)"
}

# the pixels that differ between two pictures by more than FUZZ (all of them when they cannot be compared)
differing() { compare -fuzz "$3" -metric AE "$1" "$2" null: 2>&1 | awk '{ n = ($1 ~ /^[0-9]/) ? $1 + 0 : 1000000; printf "%d", n; exit }'; }

# resting TAKE NAME: TAKE NAME until two in a row are the same picture, so a fade-in is over: the screen before the game, to tell its drawing from
resting() {
    tries=60
    "$1" "$2" || return 1
    while [ "$tries" -gt 0 ]; do
        tries=$((tries - 1))
        cp "/out/$2.png" /tmp/before.png
        sleep 0.5
        "$1" "$2" || return 1
        [ "$(differing /tmp/before.png "/out/$2.png" 0%)" -lt 500 ] && return 0
    done
    fail "$2: the screen never came to rest"
}

# settled TAKE NAME BASE: TAKE NAME until the picture is drawn and is not BASE (what the screen showed before: more than a change of tone or of the clock),
# 30 s at most. No fixed wait: a loaded machine takes longer to draw. From that first picture it goes on only a little, for one that has stopped changing
# (a window opening): an animation keeps the sixth, and a screen that goes on to another keeps the last one seen.
settled() {
    tries=60
    seen=0
    while [ "$tries" -gt 0 ]; do
        tries=$((tries - 1))
        "$1" "$2" || return 1
        if [ "$(identify -format %k "/out/$2.png")" -gt 8 ] && [ "$(differing "$3" "/out/$2.png" 8%)" -gt 10000 ]; then
            [ "$seen" -gt 0 ] && [ "$(differing /tmp/previous.png "/out/$2.png" 0%)" -lt 500 ] && break
            cp "/out/$2.png" /tmp/previous.png
            seen=$((seen + 1))
            [ "$seen" -ge 6 ] && break
        elif [ "$seen" -gt 0 ]; then
            cp /tmp/previous.png "/out/$2.png"
            break
        fi
        sleep 0.5
    done
    drawn "/out/$2.png"
    [ "$seen" -gt 0 ] || fail "$2: the screen did not change in 30 s"
}

windows_up() { xdotool search --onlyvisible --name "$1"; }
grab_x11() { import -window root "/out/$1.png"; }
shot_x11() { grab_x11 "$1"; drawn "/out/$1.png"; }
logged() { grep -aq "$1" "$2"; }

Xvfb :99 -screen 0 1280x800x24 -nolisten tcp >/out/xvfb.txt 2>&1 &
export DISPLAY=:99
until_ok 20 xdpyinfo || { echo "Xvfb did not start" >&2; cat /out/xvfb.txt >&2; exit 1; }
openbox >/out/openbox.txt 2>&1 &
openbox_pid=$!
until_ok 20 sh -c 'xprop -root _NET_SUPPORTING_WM_CHECK | grep -q "window id"' || { echo "openbox did not start" >&2; cat /out/openbox.txt >&2; exit 1; }

grab_x11 x11-empty-screen
if [ ! -d /gamedata ]; then
    mkdir user-x11
    SDL_VIDEO_DRIVER=x11 timeout -k 3 90 "$APP" --user /tmp/user-x11 --verbose >/out/x11-first-start.txt 2>&1 &
    if until_ok 30 windows_up "Spirited Drive"; then
        settled grab_x11 x11-first-start-dialog /out/x11-empty-screen.png
        # Return takes the default button (choose the CD's folder): the system's folder chooser must come up
        xdotool search --onlyvisible --name "Spirited Drive" | head -1 | xargs -I{} xdotool windowactivate --sync {} key Return 2>/dev/null || xdotool key Return
        if until_ok 20 pgrep -x zenity; then
            settled grab_x11 x11-folder-chooser /out/x11-first-start-dialog.png
        else
            shot_x11 x11-after-return
            fail "no folder chooser after Return on the first-start dialog"
        fi
    else
        fail "no first-start dialog on X11"
        cat /out/x11-first-start.txt >&2
    fi
    pkill -f squashfs-root 2>/dev/null || true
    pkill -x zenity 2>/dev/null || true
else
    mkdir user-x11
    printf 'profile=restored\npsx_dir=\n' > user-x11/port.cfg
    SDL_VIDEO_DRIVER=x11 timeout -k 3 90 "$APP" --data /gamedata --user /tmp/user-x11 --verbose >/out/x11-game.txt 2>&1 &
    if until_ok 60 logged 'mark:screen=boot' /out/x11-game.txt; then
        settled grab_x11 x11-boot-screen /out/x11-empty-screen.png
        # the key that skips the first movie, which brings the title screen (during the boot screen it would only end that)
        until_ok 30 logged 'mark:movie=' /out/x11-game.txt || fail "no movie after the boot screen"
        window=$(xdotool search --onlyvisible --name "Spirited Drive" | head -1)
        [ -n "$window" ] || fail "no game window on X11"
        xdotool windowfocus "$window" 2>/dev/null || true
        xdotool key Return
        until_ok 20 logged 'mark:screen=title' /out/x11-game.txt || fail "Return from the keyboard did not bring the title screen"
        settled grab_x11 x11-after-return /out/x11-boot-screen.png
        size=$(xdotool getwindowgeometry "$window" | sed -n 's/.*Geometry: *//p')
        [ "$size" = 1280x800 ] || fail "the game's window is ${size:-unknown}, not the screen's 1280x800: full screen did not take"
    else
        shot_x11 x11-no-boot-screen
        fail "the boot screen did not come up on X11"
        tail -30 /out/x11-game.txt >&2
    fi
    pkill -f squashfs-root 2>/dev/null || true
fi

kill "$openbox_pid" 2>/dev/null || true
# Wayland: weston with no screen of its own; its screenshooter writes the picture into the folder it runs in
unset DISPLAY
export WAYLAND_DISPLAY=sd-wayland
weston --backend=headless --renderer=pixman --width=1280 --height=800 --socket=sd-wayland --debug --idle-time=0 >/out/weston.txt 2>&1 &
shots=0
grab_wayland() {
    shots=$((shots + 1))   # a folder of its own for each picture: the screenshooter names its files by the time
    mkdir "shot-$shots"
    (cd "shot-$shots" && timeout -k 3 20 weston-screenshooter) >>/out/weston.txt 2>&1 || true
    found=$(ls "shot-$shots" | head -1)
    [ -n "$found" ] || { fail "weston took no picture for $1"; return 1; }
    cp "shot-$shots/$found" "/out/$1.png"
}
if until_ok 20 test -S "$XDG_RUNTIME_DIR/sd-wayland"; then
    mkdir user-wayland
    resting grab_wayland wayland-empty-screen
    if [ ! -d /gamedata ]; then
        SDL_VIDEO_DRIVER=wayland GDK_BACKEND=wayland timeout -k 3 60 "$APP" --user /tmp/user-wayland --verbose >/out/wayland-first-start.txt 2>&1 &
        if until_ok 30 pgrep -x zenity; then
            settled grab_wayland wayland-first-start-dialog /out/wayland-empty-screen.png
        else
            fail "no first-start dialog on Wayland (SDL shows it with zenity there)"
            cat /out/wayland-first-start.txt >&2
        fi
    else
        printf 'profile=restored\npsx_dir=\n' > user-wayland/port.cfg
        SDL_VIDEO_DRIVER=wayland timeout -k 3 90 "$APP" --data /gamedata --user /tmp/user-wayland --verbose >/out/wayland-game.txt 2>&1 &
        if until_ok 60 logged 'mark:screen=boot' /out/wayland-game.txt; then
            settled grab_wayland wayland-boot-screen /out/wayland-empty-screen.png
        else
            fail "the boot screen did not come up on Wayland"
            tail -30 /out/wayland-game.txt >&2
        fi
    fi
    pkill -f squashfs-root 2>/dev/null || true
    pkill -x zenity 2>/dev/null || true
else
    fail "weston did not start"
    cat /out/weston.txt >&2
fi

chmod -R a+rX /out
[ "$status" = 0 ] && echo "appimage_desktop_smoke: ok"
exit "$status"
