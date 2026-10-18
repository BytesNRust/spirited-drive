/* The joystick as the game starts (0043c744 and below): which device, its
 * options and calibration, and the force-feedback stick. */
#ifndef TNFS_JOY_START_H
#define TNFS_JOY_START_H

#include "game/frontend/game_file.h"
#include "game/sys/joystick.h"

enum { JOY_CALIBRATION_SIZE = 0x5c };  /* joystick.cfg: JoyState's 004fce88..004fcee4 */

typedef struct JoyStartOps {
    void *ctx;
    void (*calibrate)(void *ctx, int32_t mode);  /* 0043bc00: eax (the calibration screen, dropped) */
    void (*forces_open)(void *ctx);              /* 0043cb50: the force-feedback stick (IFORCE.DLL's) */
} JoyStartOps;

/* 0043c980 (all registers kept): "<dir>joystick.cfg" (dir 00524198) read
 * whole: JOY_CALIBRATION_SIZE bytes, *present (004fcdcc) 1, the bytes
 * copied through a "joystick" block (00474c8c, 004884d8, 004884c8, then
 * 00475260) over 004fce88, the stick polled (0043d8d0) and its x and y
 * checked (each within 0x4b of its mid, wrapping, and within its low /
 * high; signed); none, any other size (the file removed) or a check
 * failed: calibrate(0). Then with *present and 004fce2e (forces_on):
 * forces_open. */
void joy_calibration_load(JoyState *j, int32_t *present, const char *dir, int32_t device, const GameFileOps *files,
                          const JoyOps *joy, const JoyStartOps *ops);

#endif
