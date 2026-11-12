/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: how long one of our own screens (the boot screen, our credits pages) stays and what moves it on, pure so it can be tested. A screen stays its
 * time on the game's timer (100 ticks a second, timer.h), not a count of frames; a press moves it on only when it began on this screen: each source
 * (the keyboard, the pointer's buttons, a pad) counts once it has been seen up here (or holding only what does nothing here), so a key or a button still held from the screen before, or read
 * again before the events after a fade were pumped, does not move it on a second time. */
#ifndef TNFS_SCREEN_HOLD_H
#define TNFS_SCREEN_HOLD_H

#include <stdint.h>

typedef enum { HOLD_STAY, HOLD_NEXT, HOLD_LEAVE } HoldStep;   /* stay; the next screen (the time is up, or a press); leave the rest */

enum { HOLD_KEYS, HOLD_BUTTONS, HOLD_PAD, HOLD_SOURCES };

typedef struct {
    int down;         /* anything held on this source now */
    HoldStep press;   /* what what is held does: HOLD_STAY for nothing (a key the screen does not take) */
} HoldSource;

typedef struct {
    uint32_t start, ticks;
    unsigned armed;   /* bit k: source k seen up since the screen began */
} ScreenHold;

/* the screen begins at `now`, for `ticks` of the timer */
ScreenHold screen_hold_start(uint32_t now, uint32_t ticks);

/* one look at the sources at `now`: what the screen does */
HoldStep screen_hold_step(ScreenHold *h, uint32_t now, const HoldSource src[HOLD_SOURCES]);

#endif
