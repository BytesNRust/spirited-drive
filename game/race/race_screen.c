/* The race's screen size (race_screen.h). */
#include "game/race/race_screen.h"

enum { LOW_W = 0x140, LOW_H = 0xc8, HIGH_W = 0x280, HIGH_H = 0x1e0 };

void race_screen_setup(RaceScreen *s, int32_t mode, int32_t a, int32_t b, int32_t c, int32_t d, int32_t next_kind)
{
    s->mode = mode;
    s->width = s->clip_right = mode == 0 ? LOW_W : HIGH_W;
    s->height = s->clip_bottom = mode == 0 ? LOW_H : HIGH_H;
    s->clip_left = s->clip_top = 0;
    s->unknown_51200c = a;
    s->unknown_511ffc = b;
    s->unknown_512014 = d;
    s->unknown_512000 = c;
    s->next_kind = next_kind;
}
