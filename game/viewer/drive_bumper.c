/* port: the bumper view's extras (drive_bumper.h). */
#include "viewer/drive_bumper.h"

#include "game/gfx/blit.h"

void drive_bumper_start(DriveBumper *b, int mirror, int radar)
{
    *b = (DriveBumper){ mirror != 0, radar != 0, 0, 0 };
}

void drive_bumper_keys(DriveBumper *b, int mirror_key, int radar_key)
{
    if (mirror_key && !b->mirror_down) b->mirror = !b->mirror;
    if (radar_key && !b->radar_down) b->radar = !b->radar;
    b->mirror_down = mirror_key != 0, b->radar_down = radar_key != 0;
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

static void plot(uint8_t *screen, int screen_w, int screen_h, int x, int y, uint8_t c)
{
    if (x >= 0 && x < screen_w && y >= 0 && y < screen_h) screen[(size_t)y * screen_w + x] = c;
}

/* a box `fill` with a one pixel edge, `top_left` on its top and left, `bottom_right` on its bottom and right (the
 * corners where the two meet are the lower one's) */
static void raised(uint8_t *screen, int screen_w, int screen_h, ScreenPoint at, ScreenPoint size, uint8_t fill,
                   uint8_t top_left, uint8_t bottom_right)
{
    for (int y = 0; y < size.y; y++)
        for (int x = 0; x < size.x; x++) {
            uint8_t c = fill;
            if (y == 0 || x == 0) c = top_left;
            if (y == size.y - 1 || x == size.x - 1) c = bottom_right;
            plot(screen, screen_w, screen_h, at.x + x, at.y + y, c);
        }
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
    const int well = DRIVE_BUMPER_RADAR_BEVEL + DRIVE_BUMPER_RADAR_FACE;   /* the well's edge in from the case's */
    raised(screen, screen_w, screen_h, at, size, art->face, art->light, art->dark);
    raised(screen, screen_w, screen_h, (ScreenPoint){ at.x + well, at.y + well },
           (ScreenPoint){ size.x - 2 * well, size.y - 2 * well }, DRIVE_BUMPER_FRAME, art->dark, art->light);
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
