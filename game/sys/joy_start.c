/* The joystick as the game starts (joy_start.h). */
#include "game/sys/joy_start.h"

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
