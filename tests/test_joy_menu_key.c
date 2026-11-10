/* The menus' joystick keys (0043d4c0, joy_keys) and port's menu_pad_cancel: the first button gives Space (0x20, accept),
 * the second Enter (0xd, accept) -- a pad's A and B by default (control_map.c: the first two virtual buttons are the gear up
 * and gear down bindings, A and B) -- and with the cancel on the second gives Escape (0x1b) instead, what the keyboard's
 * Escape gives in the same menus; the first still accepts, and with no button down nothing is. */
#include "game/sys/joystick.h"
#include "test.h"

static uint32_t down;   /* the buttons the fake pad has down */

static int32_t read_pad(void *ctx, int32_t device, uint32_t *buttons, int32_t axis[JOY_AXES])
{
    (void)ctx, (void)device;
    *buttons = down;
    for (int k = 0; k < JOY_AXES; k++) axis[k] = 0;
    return 1;
}

static int32_t key_with(uint32_t buttons, int cancel)
{
    JoyState j = { 0 };
    j.raw = 1, j.raw_axes = 1;   /* the buttons as read, the axes where they were */
    const JoyOps ops = { NULL, read_pad };
    down = buttons;
    (void)joy_keys(&j, 1, 0, &ops);
    return joy_menu_key(j.button_key, cancel);
}

int main(void)
{
    enum { SPACE = 0x20, ENTER = 0xd, ESCAPE = 0x1b };
    CHECK_EQ_INT(key_with(0, 0), 0);
    CHECK_EQ_INT(key_with(0, 1), 0);
    CHECK_EQ_INT(key_with(1, 0), SPACE);   /* the exe's: the first button is Space */
    CHECK_EQ_INT(key_with(2, 0), ENTER);   /* and the second Enter */
    CHECK_EQ_INT(key_with(1, 1), SPACE);   /* cancel on: the first still accepts */
    CHECK_EQ_INT(key_with(2, 1), ESCAPE);  /* the second is Escape, not Enter */
    CHECK_EQ_INT(key_with(3, 1), SPACE);   /* both down: the first, as before */
    CHECK_EQ_INT(key_with(4, 1), 0);       /* a third button is no menu key, as before */
    CHECK_EQ_INT(joy_menu_key(0x4800, 1), 0x4800);   /* only the buttons' keys are touched */
    return TEST_REPORT();
}
