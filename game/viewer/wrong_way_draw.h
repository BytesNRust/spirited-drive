/* port (wrong_way): the warning drawn on a copy of the shown frame, as race_stats_draw.c draws the stats: the words (wrong_way_text) centred in the
 * upper third, red, in the menus' face (graver18) scaled to the frame's height. The frame is left as it is when the warning is not visible. */
#ifndef TNFS_WRONG_WAY_DRAW_H
#define TNFS_WRONG_WAY_DRAW_H

#include "game/race/wrong_way.h"

#include <stdint.h>

const uint8_t *wrong_way_compose(const WrongWay *w, const uint8_t *frame, int width, int height, const uint8_t palette[768], int german);

#endif
