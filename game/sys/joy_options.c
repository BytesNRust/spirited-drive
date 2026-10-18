/* The joystick's options (joy_options.h). */
#include "game/sys/joy_options.h"

#include <stddef.h>
#include <stdio.h>

enum { PATH_SIZE = 0x50 };

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

/* the bit a button lands on as joyopt.cfg keeps it (1, 2, 4, 8) made its number */
static uint8_t bit_number(uint8_t bit)
{
    switch (bit) {
    case 1: return 0;
    case 2: return 1;
    case 4: return 2;
    case 8: return 3;
    default: return bit;
    }
}

/* 0043d284 */
void joy_options_load(JoyState *j, const char *dir, const GameFileOps *files)
{
    char path[PATH_SIZE];
    void *data = NULL;
    int32_t flags = 0, size = 0;
    j->raw = 1;
    j->raw_axes = 1;
    j->upright = 1;
    snprintf(path, sizeof path, "%sjoyopt.cfg", dir);
    files->read(files->ctx, path, &data, &flags, &size);
    if (data == NULL) {
        joy_options_default(j);
    } else if (size == JOY_OPTIONS_SIZE) {
        files->copy(files->ctx, data, (uint8_t *)j + offsetof(JoyState, unknown_ce1c), JOY_OPTIONS_SIZE);
        files->release(files->ctx, data);
        joy_options_check(j);
    } else {
        files->release(files->ctx, data);
        files->remove_file(files->ctx, path);
        joy_options_default(j);
    }
    for (int k = 0; k < JOY_BUTTONS; k++) j->button_bit[k] = bit_number(j->button_bit[k]);
}

static void turn_over(int16_t on, int32_t low, int32_t high, int32_t *v)
{
    if (on == 0) return;
    if (*v > high) *v = high;
    *v = (int32_t)((uint32_t)low + (uint32_t)high - (uint32_t)*v);
}

/* 0043db50 */
void joy_turn_over(const JoyState *j, int32_t *x, int32_t *y, int32_t *z, int32_t *r)
{
    turn_over(j->axis_invert[0], j->x_min, j->x_max, x);
    turn_over(j->axis_invert[1], j->y_min, j->y_max, y);
    turn_over(j->axis_invert[2], j->z_min, j->z_max, z);
    turn_over(j->axis_invert[3], j->r_min, j->r_max, r);
}
