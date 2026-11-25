/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: the key chord that toggles full screen: Alt+Enter (Windows, Linux) and Cmd+Ctrl+F (macOS), in every mode. The platform layer asks this of each key press; the chord is the whole press (extra modifiers, Shift for one, make it a
 * different key), so Enter on its own, Alt+F or Cmd+F stay with the game. */
#ifndef TNFS_FULLSCREEN_KEY_H
#define TNFS_FULLSCREEN_KEY_H

#include <stdbool.h>

/* USB HID usage numbers, as SDL's scancodes are (checked against SDL in platform_input_sdl3.c) */
enum { FULLSCREEN_SC_F = 9, FULLSCREEN_SC_RETURN = 40, FULLSCREEN_SC_KP_ENTER = 88 };

/* the modifiers held, either side of each */
enum { FULLSCREEN_MOD_ALT = 1, FULLSCREEN_MOD_CTRL = 2, FULLSCREEN_MOD_GUI = 4, FULLSCREEN_MOD_SHIFT = 8 };

/* 1 when `scancode` pressed with exactly `mods` held is the toggle on that system (`apple`: macOS, where Cmd is the GUI key) */
bool fullscreen_key_is_toggle(int scancode, unsigned mods, bool apple);

#endif
