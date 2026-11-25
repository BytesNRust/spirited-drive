/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game/platform/vsync_plan.h"

enum { MOST_VBLANKS = 4 };

int vsync_plan_interval(double display_hz, int target_hz)
{
    if (!(display_hz > 0.0) || target_hz <= 0) return 0;
    for (int n = 1; n <= MOST_VBLANKS; n++) {
        const double shown = display_hz / n, off = shown > target_hz ? shown - target_hz : target_hz - shown;
        if (off <= 0.03 * target_hz) return n;
    }
    return 0;
}
