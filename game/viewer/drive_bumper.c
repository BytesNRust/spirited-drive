/* port: the bumper view's extras (drive_bumper.h). */
#include "viewer/drive_bumper.h"

void drive_bumper_start(DriveBumper *b, int mirror, int radar)
{
    *b = (DriveBumper){ mirror != 0, radar != 0, 0, 0 };
}

void drive_bumper_keys(DriveBumper *b, int f7, int f8)
{
    if (f7 && !b->f7_down) b->mirror = !b->mirror;
    if (f8 && !b->f8_down) b->radar = !b->radar;
    b->f7_down = f7 != 0, b->f8_down = f8 != 0;
}

int drive_bumper_view(int32_t chase_kind)
{
    return chase_kind == DRIVE_BUMPER_KIND;
}

ScreenPoint drive_bumper_mirror_at(int width, int mirror_w)
{
    return (ScreenPoint){ width - mirror_w - DRIVE_BUMPER_MARGIN, HUD_BAR_HEIGHT + DRIVE_BUMPER_MARGIN };
}

static void put(uint8_t *screen, int screen_w, int screen_h, int x, int y, uint8_t c)
{
    if (x >= 0 && x < screen_w && y >= 0 && y < screen_h) screen[(size_t)y * screen_w + x] = c;
}

void drive_bumper_blit(uint8_t *screen, int screen_w, int screen_h, const uint8_t *picture, int w, int h, ScreenPoint at)
{
    for (int y = -1; y <= h; y++)
        for (int x = -1; x <= w; x++) {
            const int inside = x >= 0 && x < w && y >= 0 && y < h;
            put(screen, screen_w, screen_h, at.x + x, at.y + y, inside ? picture[(size_t)y * w + x] : DRIVE_BUMPER_FRAME);
        }
}
