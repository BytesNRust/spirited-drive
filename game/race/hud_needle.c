/* The tachometer's needle (hud_needle.h). */
#include "game/race/hud_needle.h"

#include "game/gfx/blit.h"
#include "game/gfx/clip.h"
#include "game/gfx/flat.h"
#include "game/sys/fixed_math.h"

enum {
    RPM_OVER = 0x3d4,       /* past the redline the needle stops here */
    SWEEP = 600,            /* of 1024 a turn, 0 to the redline */
    START_ANGLE = 0x1a9,
    MARGIN = 2              /* the erase boxes reach this far past the old needle */
};

static int32_t min32(int32_t a, int32_t b)
{
    return a < b ? a : b;
}

static int32_t max32(int32_t a, int32_t b)
{
    return a > b ? a : b;
}

/* The tach redrawn over the box from (ax, ay) to (bx, by), MARGIN out. */
static void erase(QuadTarget *t, const DashArt *art, int32_t ax, int32_t ay, int32_t bx, int32_t by,
                  int32_t surface_w, int32_t surface_h)
{
    clip_set(t, min32(ax, bx) - MARGIN, max32(ax, bx) + MARGIN, min32(ay, by) - MARGIN, max32(ay, by) + MARGIN,
             surface_w, surface_h);
    blit_shape_placed_transparent(t, art->tach);
}

void hud_draw_needle(QuadTarget *t, HudNeedle *needle, const DashArt *art, int32_t rpm, int32_t redline,
                     int32_t surface_w, int32_t surface_h, int32_t screen_w, int32_t screen_h, int32_t high_res,
                     uint32_t colour)
{
    const int32_t top = (int32_t)((uint32_t)redline + RPM_OVER);
    if (rpm > top) rpm = top;
    const int32_t sweep = (int32_t)((uint32_t)rpm * SWEEP);
    if (redline == 0 || (redline == -1 && sweep == INT32_MIN)) return;
    const uint32_t angle = (uint32_t)(sweep / redline) + START_ANGLE;
    const int32_t tip_x = art->centre_x + fx_mul_round(art->radius_x, fx_cos(angle));
    const int32_t tip_y = art->centre_y + fx_mul_round(art->radius_y, fx_sin(angle));
    const int32_t inner_x = (art->centre_x + (art->centre_x + tip_x) / 2) / 2;
    const int32_t inner_y = (art->centre_y + (art->centre_y + tip_y) / 2) / 2;
    const int32_t mid_x = (needle->tip_x + needle->inner_x) / 2, mid_y = (needle->tip_y + needle->inner_y) / 2;
    erase(t, art, mid_x, mid_y, needle->tip_x, needle->tip_y, surface_w, surface_h);
    erase(t, art, mid_x, mid_y, needle->inner_x, needle->inner_y, surface_w, surface_h);
    clip_set(t, 0, screen_w, 0, screen_h, surface_w, surface_h);
    if (high_res != 0) {
        flat_line(t, tip_x, tip_y, inner_x, inner_y, colour);
        flat_line(t, tip_x + 1, tip_y, inner_x + 1, inner_y, colour);
        flat_line(t, tip_x, tip_y + 1, inner_x, inner_y + 1, colour);
    }
    *needle = (HudNeedle){ tip_x, tip_y, inner_x, inner_y };
}
