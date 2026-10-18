/* The joystick's options (joy_options.h). */
#include "game/sys/joy_options.h"

static const uint8_t IN_TURN[JOY_AXES] = { 1, 2, 4, 8 };

/* 0043d1a0 */
void joy_options_default(JoyState *j)
{
    for (int k = 0; k < JOY_BUTTONS; k++) j->button_on[k] = 1;
    for (int k = 0; k < JOY_AXES; k++) j->axis_on[k] = 1;
    j->forces_on = 1;
    for (int k = 0; k < JOY_BUTTONS; k++) j->button_bit[k] = IN_TURN[k];
    for (int k = 0; k < JOY_AXES; k++) j->axis_slot[k] = IN_TURN[k];
    for (int k = 0; k < JOY_BUTTONS; k++) j->invert[k] = 0;
    for (int k = 0; k < JOY_AXES; k++) j->axis_invert[k] = 0;
}

/* 0043d37c */
void joy_options_check(JoyState *j)
{
    int plain = 1;
    for (int k = 0; k < JOY_BUTTONS; k++)
        if (j->button_on[k] == 0 || j->invert[k] != 0 || j->button_bit[k] != k) plain = 0;
    if (!plain) j->raw = 0;
    for (int k = 0; k < JOY_AXES; k++)
        if (j->axis_on[k] != 0 && j->axis_invert[k] != 0) j->upright = 0;
    for (int k = 0; k < JOY_AXES; k++)
        if (j->axis_on[k] != 0 && j->axis_slot[k] != IN_TURN[k]) j->raw_axes = 0;
}
