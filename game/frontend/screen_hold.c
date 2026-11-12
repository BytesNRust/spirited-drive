/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: see screen_hold.h. */
#include "game/frontend/screen_hold.h"

ScreenHold screen_hold_start(uint32_t now, uint32_t ticks)
{
    return (ScreenHold){ now, ticks, 0 };
}

ScreenHold screen_hold_start_armed(uint32_t now, uint32_t ticks)
{
    return (ScreenHold){ now, ticks, (1u << HOLD_SOURCES) - 1 };
}

HoldStep screen_hold_step(ScreenHold *h, uint32_t now, const HoldSource src[HOLD_SOURCES])
{
    for (int k = 0; k < HOLD_SOURCES; k++) {
        if (!src[k].down || src[k].press == HOLD_STAY) {   /* up, or held on something that does nothing here */
            h->armed |= 1u << k;
            continue;
        }
        if (h->armed & (1u << k)) return src[k].press;
    }
    return now - h->start >= h->ticks ? HOLD_NEXT : HOLD_STAY;
}

int screen_hold_all_up(const HoldSource src[HOLD_SOURCES])
{
    for (int k = 0; k < HOLD_SOURCES; k++)
        if (src[k].down) return 0;
    return 1;
}
