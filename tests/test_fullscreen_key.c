/* SPDX-License-Identifier: GPL-3.0-or-later */
/* fullscreen_key: the chord that toggles full screen, and only it. */
#include "game/platform/fullscreen_key.h"
#include "test.h"

int main(void)
{
    /* Windows and Linux: Alt+Enter, the keypad's too */
    CHECK(fullscreen_key_is_toggle(FULLSCREEN_SC_RETURN, FULLSCREEN_MOD_ALT, false));
    CHECK(fullscreen_key_is_toggle(FULLSCREEN_SC_KP_ENTER, FULLSCREEN_MOD_ALT, false));
    CHECK(!fullscreen_key_is_toggle(FULLSCREEN_SC_RETURN, 0, false));                                           /* Enter alone is the menus' */
    CHECK(!fullscreen_key_is_toggle(FULLSCREEN_SC_RETURN, FULLSCREEN_MOD_ALT | FULLSCREEN_MOD_SHIFT, false));   /* a different chord */
    CHECK(!fullscreen_key_is_toggle(FULLSCREEN_SC_RETURN, FULLSCREEN_MOD_ALT | FULLSCREEN_MOD_CTRL, false));
    CHECK(!fullscreen_key_is_toggle(FULLSCREEN_SC_RETURN, FULLSCREEN_MOD_CTRL, false));
    CHECK(!fullscreen_key_is_toggle(FULLSCREEN_SC_RETURN, FULLSCREEN_MOD_GUI, false));
    CHECK(!fullscreen_key_is_toggle(FULLSCREEN_SC_F, FULLSCREEN_MOD_ALT, false));                               /* Alt+F is not it */
    CHECK(!fullscreen_key_is_toggle(FULLSCREEN_SC_F, FULLSCREEN_MOD_GUI | FULLSCREEN_MOD_CTRL, false));         /* nor the Mac's chord there */
    /* macOS: Cmd+Ctrl+F */
    CHECK(fullscreen_key_is_toggle(FULLSCREEN_SC_F, FULLSCREEN_MOD_GUI | FULLSCREEN_MOD_CTRL, true));
    CHECK(!fullscreen_key_is_toggle(FULLSCREEN_SC_F, FULLSCREEN_MOD_GUI, true));                                /* Cmd+F alone */
    CHECK(!fullscreen_key_is_toggle(FULLSCREEN_SC_F, FULLSCREEN_MOD_CTRL, true));
    CHECK(!fullscreen_key_is_toggle(FULLSCREEN_SC_F, 0, true));
    CHECK(!fullscreen_key_is_toggle(FULLSCREEN_SC_F, FULLSCREEN_MOD_GUI | FULLSCREEN_MOD_CTRL | FULLSCREEN_MOD_SHIFT, true));
    CHECK(!fullscreen_key_is_toggle(FULLSCREEN_SC_RETURN, FULLSCREEN_MOD_ALT, true));                           /* Alt+Enter is not the Mac's */
    CHECK(!fullscreen_key_is_toggle(FULLSCREEN_SC_RETURN, FULLSCREEN_MOD_GUI | FULLSCREEN_MOD_CTRL, true));
    return TEST_REPORT();
}
