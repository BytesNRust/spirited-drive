/* port: the bumper view's extras (drive_bumper.h). */
#include "viewer/drive_bumper.h"

#include "game/gfx/blit.h"

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

enum { SHAPE_WIDTH = 4, SHAPE_HEIGHT = 6 };  /* an FSH shape's header (blit.c) */

static int shape_dim(const uint8_t *shape, int at)
{
    return (int16_t)(shape[at] | shape[at + 1] << 8);
}

static int art_whole(const DriveBumperRadarArt *a)
{
    return a->big_lit != NULL && a->big_dark != NULL && a->small_lit != NULL && a->small_dark != NULL;
}

static int radar_height(const DriveBumperRadarArt *a)
{
    const int big = shape_dim(a->big_dark, SHAPE_HEIGHT), small = shape_dim(a->small_dark, SHAPE_HEIGHT);
    return big > small ? big : small;
}

ScreenPoint drive_bumper_radar_size(const DriveBumperRadarArt *art)
{
    if (!art_whole(art)) return (ScreenPoint){ 0, 0 };
    const int w = shape_dim(art->big_dark, SHAPE_WIDTH) + DRIVE_BUMPER_RADAR_SMALL * (DRIVE_BUMPER_RADAR_GAP +
                  shape_dim(art->small_dark, SHAPE_WIDTH));
    return (ScreenPoint){ w + 2 * DRIVE_BUMPER_RADAR_PAD, radar_height(art) + 2 * DRIVE_BUMPER_RADAR_PAD };
}

ScreenPoint drive_bumper_radar_at(int width)
{
    (void)width;
    return (ScreenPoint){ DRIVE_BUMPER_MARGIN, HUD_BAR_HEIGHT + DRIVE_BUMPER_MARGIN };
}

void drive_bumper_radar(uint8_t *screen, int screen_w, int screen_h, const DriveBumperRadarArt *art, int32_t count,
                        int32_t big_lit, ScreenPoint at)
{
    if (!art_whole(art)) return;
    const QuadTarget t = { screen, screen_w, 0, 0, screen_w, screen_h };
    const ScreenPoint size = drive_bumper_radar_size(art);
    for (int y = 0; y < size.y; y++)
        for (int x = 0; x < size.x; x++) {
            const int sx = at.x + x, sy = at.y + y;
            if (sx >= 0 && sx < screen_w && sy >= 0 && sy < screen_h) screen[(size_t)sy * screen_w + sx] = DRIVE_BUMPER_FRAME;
        }
    const int row = radar_height(art), lit = count > DRIVE_BUMPER_RADAR_SMALL ? DRIVE_BUMPER_RADAR_SMALL : count;
    const uint8_t *big = count != 0 && big_lit != 0 ? art->big_lit : art->big_dark;
    int x = at.x + DRIVE_BUMPER_RADAR_PAD;
    const int top = at.y + DRIVE_BUMPER_RADAR_PAD;
    blit_shape(&t, big, x, top + (row - shape_dim(big, SHAPE_HEIGHT)) / 2, BLIT_OPAQUE);
    x += shape_dim(big, SHAPE_WIDTH);
    for (int i = 0; i < DRIVE_BUMPER_RADAR_SMALL; i++) {
        const uint8_t *small = i < lit ? art->small_lit : art->small_dark;
        x += DRIVE_BUMPER_RADAR_GAP;
        blit_shape(&t, small, x, top + (row - shape_dim(small, SHAPE_HEIGHT)) / 2, BLIT_OPAQUE);
        x += shape_dim(small, SHAPE_WIDTH);
    }
}
