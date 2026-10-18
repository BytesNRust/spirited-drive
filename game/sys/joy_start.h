/* The joystick as the game starts (0043c744 and below): which device, its
 * options and calibration, and the force-feedback stick. */
#ifndef TNFS_JOY_START_H
#define TNFS_JOY_START_H

#include "game/frontend/game_file.h"
#include "game/sys/joystick.h"

enum {
    JOY_CALIBRATION_SIZE = 0x5c,  /* joystick.cfg: JoyState's 004fce88..004fcee4 */
    JOY_NAME_SIZE = 0x50,         /* 0048d0b0's name buffer */
    JOY_CONTROLS = 13             /* 0052ad26: the steering, pedals and gears devices, then ten keys */
};

/* 004fcdb4..004fcdcc and 004bb908 */
typedef struct JoyStart {
    int32_t found;               /* 004fcdb4: a device answered (the controls' Calibrate item) */
    int32_t has_axis[JOY_AXES];  /* 004fcdb8: x, y, z, r there and on */
    int32_t buttons;             /* 004fcdc8: its buttons */
    int32_t present;             /* 004fcdcc: joystick.cfg read (the pad present) */
    int32_t device;              /* 004bb908 */
} JoyStart;

typedef struct JoyStartOps {
    void *ctx;
    void (*calibrate)(void *ctx, int32_t mode);  /* 0043bc00: eax (the calibration screen, dropped) */
    void (*forces_open)(void *ctx);              /* 0043cb50: the force-feedback stick (IFORCE.DLL's) */
    int32_t (*count)(void *ctx);                 /* 0048d040: the devices (joyGetNumDevs) */
    /* 0048d0b0 (cdecl device, name, &buttons, &flags): nonzero when the
     * device answers; flags 1 x, 2 y, 4 z, 8 r (and higher, a dword) */
    int32_t (*caps)(void *ctx, int32_t device, char *name, int32_t *buttons, uint32_t *flags);
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

/* 0043c744 (all registers kept): 004fcdcc and 004fcdb4 0; the first of the
 * devices (count, signed 16-bit numbers) whose caps answer: its buttons,
 * axes and number kept, found 1. Found: joyopt.cfg (0043d284), each axis
 * kept only while on (004fce26), joystick.cfg (0043c980). Then without the
 * pad present, any of the steering, gears and pedals controls (0052ad26,
 * 0052ad2a, 0052ad28) at 1..3 (a joystick's): the three 0 and the keys
 * the defaults (the arrows, 'A' / 'Z', space, 'C', 'H', 'M'; German 'Y'
 * for 'Z' and 'K' for 'C'). */
void joy_start(JoyStart *s, JoyState *j, int16_t *controls, int16_t german, const char *dir,
               const GameFileOps *files, const JoyOps *joy, const JoyStartOps *ops);

#endif
