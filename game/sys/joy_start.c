/* The joystick as the game starts (joy_start.h). */
#include "game/sys/joy_start.h"

#include "game/sys/joy_options.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

enum { PATH_SIZE = 0x50, NEAR_MID = 0x4b };

static int within(int32_t v, int32_t mid, int32_t low, int32_t high)
{
    return (int32_t)((uint32_t)mid - NEAR_MID) <= v && (int32_t)((uint32_t)mid + NEAR_MID) >= v && v >= low &&
           v <= high;
}

/* joystick.cfg read over the calibration: 1 when it was there whole and the stick sits at its middle */
static int calibration_read(JoyState *j, int32_t *present, const char *dir, int32_t device, const GameFileOps *files,
                            const JoyOps *joy)
{
    char path[PATH_SIZE];
    void *data = NULL;
    int32_t flags = 0, size = 0;
    snprintf(path, sizeof path, "%sjoystick.cfg", dir);
    files->read(files->ctx, path, &data, &flags, &size);
    if (data == NULL) return 0;
    if (size != JOY_CALIBRATION_SIZE) {
        files->release(files->ctx, data);
        files->remove_file(files->ctx, path);
        return 0;
    }
    *present = 1;
    uint8_t *block = files->alloc(files->ctx, "joystick", size, 0);
    files->copy(files->ctx, data, block, size);
    files->release(files->ctx, data);
    memcpy((uint8_t *)j + offsetof(JoyState, x_min), block, JOY_CALIBRATION_SIZE);
    files->block_free(files->ctx, block);
    joy_poll(j, device, joy);
    const int32_t x = j->axis[0], y = j->axis[1];
    return within(y, j->y_mid, j->y_low, j->y_high) && within(x, j->x_mid, j->x_low, j->x_high);
}

/* 0043c980 */
void joy_calibration_load(JoyState *j, int32_t *present, const char *dir, int32_t device, const GameFileOps *files,
                          const JoyOps *joy, const JoyStartOps *ops)
{
    if (!calibration_read(j, present, dir, device, files, joy)) ops->calibrate(ops->ctx, 0);
    if (*present != 0 && j->forces_on != 0) ops->forces_open(ops->ctx);
}

enum { STEERING, PEDALS, GEARS, KEYS };
static const int16_t KEY_DEFAULTS[JOY_CONTROLS - KEYS] = { 0x4b00, 0x4d00, 0x4800, 0x5000, 'A', 'Z', ' ', 'C', 'H',
                                                            'M' };
static const int16_t KEY_GERMAN_Z = 'Y', KEY_GERMAN_C = 'K';

static int a_joystick(int16_t device) { return device > 0 && device < 4; }

/* 0043c744 */
void joy_start(JoyStart *s, JoyState *j, int16_t *controls, int16_t german, const char *dir,
               const GameFileOps *files, const JoyOps *joy, const JoyStartOps *ops)
{
    s->present = 0;
    s->found = 0;
    const int32_t n = ops->count(ops->ctx);
    if (n != 0) {
        for (int32_t i = 0; (int16_t)i < n; i++) {
            char name[JOY_NAME_SIZE];
            int32_t buttons;
            uint32_t flags;
            if (ops->caps(ops->ctx, (int16_t)i, name, &buttons, &flags) == 0) continue;
            s->buttons = buttons;
            for (int k = 0; k < JOY_AXES; k++) s->has_axis[k] = (int32_t)(flags & (1u << k));
            s->device = (int16_t)i;
            s->found = 1;
            break;
        }
    }
    if (s->found != 0) {
        joy_options_load(j, dir, files);
        for (int k = 0; k < JOY_AXES; k++) s->has_axis[k] = s->has_axis[k] != 0 && j->axis_on[k] != 0;
        joy_calibration_load(j, &s->present, dir, s->device, files, joy, ops);
    }
    if (s->present != 0) return;
    if (!a_joystick(controls[STEERING]) && !a_joystick(controls[GEARS]) && !a_joystick(controls[PEDALS])) return;
    controls[STEERING] = controls[GEARS] = controls[PEDALS] = 0;
    for (int k = KEYS; k < JOY_CONTROLS; k++) controls[k] = KEY_DEFAULTS[k - KEYS];
    if (german != 0) controls[KEYS + 5] = KEY_GERMAN_Z, controls[KEYS + 7] = KEY_GERMAN_C;
}
