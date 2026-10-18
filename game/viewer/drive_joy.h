/* The joystick and its force-feedback stick for the game (game/sys/joy_*,
 * race_forces) on the platform's (platform.h). Without a joystick nothing is
 * found, the pad is not present and the race's force ops do nothing. */
#ifndef TNFS_DRIVE_JOY_H
#define TNFS_DRIVE_JOY_H

#include "game/race/input_read.h"
#include "game/race/race_state.h"
#include "game/sys/joy_start.h"

/* 0043c744 once (later calls do nothing): the first joystick, joyopt.cfg and
 * joystick.cfg from the config directory; without joystick.cfg (the
 * calibration screen, 0043bc00, dropped) the stick calibrated over its whole
 * travel about where it rests. controls are 0052ad26's words, put back to
 * the keys' when a joystick's were kept and there is no pad. */
void drive_joy_start(int16_t *controls, int16_t german);

const JoyStart *drive_joy_state(void);

/* 0043d598 into the race's pad (004fce54, 004fce58, 004fce68) */
void drive_joy_pad_read(InputPad *pad);

/* The race's force ops on the stick, and 0052b320 (race->effects) set when
 * it opened. */
void drive_joy_install(RaceState *race);

#endif
