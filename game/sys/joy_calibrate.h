/* The calibration's sums (0043bc00's, the calibration screen's, which is
 * dropped): the scales and the menu thresholds from each axis's travel. */
#ifndef TNFS_JOY_CALIBRATE_H
#define TNFS_JOY_CALIBRATE_H

#include "game/sys/joystick.h"

/* 0043bc00 once the stick is measured (004fce88..004fcedc's mins, mids and
 * maxes set): x's and y's travel below and above the mid (004fce94 / 98,
 * 004fceb0 / b4), the menu thresholds half of each beyond the min and the
 * mid (004fce9c / a0, 004fceb8 / bc), z's travel (004fcecc / d0); each but
 * z's above (and y's above, kept twice) kept to 0x7ffa. Then the pedals,
 * half z's and r's spans less 1 (004fced4, 004fcee0); the travels above 1
 * become 16.16 scales, 0x23 over the travel + 1 (z's above 0x26) and with
 * pedals (004fcdc4 r or 004fcdc0 z on) the pedals above 1, 0x24 over the
 * same. */
void joy_calibrate(JoyState *j, int32_t pedals);

#endif
