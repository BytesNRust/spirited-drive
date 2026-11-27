/* port (replay bare): a double click on the picture while a replay plays hides the replay's controls and the
 * pointer, and another shows them again, so a replay can be watched or recorded with nothing over it. The original
 * always draws them. Bare, the mouse presses nothing (the buttons are not there to see); the keys go on as before. */
#ifndef TNFS_REPLAY_BARE_H
#define TNFS_REPLAY_BARE_H

#include <stdint.h>

enum { REPLAY_BARE_DOUBLE_MS = 400 };   /* the longest time from a click to the one that makes it a double click */

typedef struct {
    int32_t bare;       /* 1: the controls and the pointer are not drawn */
    int32_t was_down;   /* the button the frame before */
    int32_t armed;      /* a click on the picture waits for its second */
    uint32_t first_ms;  /* when that click came */
} ReplayBare;

/* One frame of a replay: `down` the mouse's select button, `on_controls` the pointer over the controls' strip (a
 * click there is a button's and never counts; bare there is no strip, so every click counts), `now_ms` the clock.
 * 1 when `bare` changed on this frame. */
int32_t replay_bare_frame(ReplayBare *b, int32_t down, int32_t on_controls, uint32_t now_ms);

#endif
