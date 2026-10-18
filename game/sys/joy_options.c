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
