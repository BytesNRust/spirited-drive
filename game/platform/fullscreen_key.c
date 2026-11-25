/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: see fullscreen_key.h. */
#include "game/platform/fullscreen_key.h"

bool fullscreen_key_is_toggle(int scancode, unsigned mods, bool apple)
{
    if (apple) return scancode == FULLSCREEN_SC_F && mods == (FULLSCREEN_MOD_GUI | FULLSCREEN_MOD_CTRL);
    return (scancode == FULLSCREEN_SC_RETURN || scancode == FULLSCREEN_SC_KP_ENTER) && mods == FULLSCREEN_MOD_ALT;
}
