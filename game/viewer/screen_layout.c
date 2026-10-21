/* The wide screen's calculations (screen_layout.h). Port glue: nothing here is the original's. */
#include "game/viewer/screen_layout.h"

#include <math.h>

enum {
    HIGH_MODE = 1,              /* race_view_layouts[1]: the 640 x 480 mode */
    OUTSIDE_ROW = 3,
    OUTSIDE_HEIGHT = SCREEN_HEIGHT - HUD_BAR_HEIGHT,
    MIRROR_ROW = 12,
    WIDTH_16_9 = 854,
    WIDTH_21_9 = 1120,
    MIRROR_TOP = 38,            /* the mirror against the right edge, as the original's runs off it */
    MAP_MARGIN = 6,
    /* 640 positions of the pieces that do not move with the car */
    MIDDLE_X_640 = 262,         /* the speed's first digit (hud.c TOP_ROW_HIGH), where the middle group begins */
    END_X_640 = 405,
    MAP_X_640 = 0x208, MAP_Y_640 = 0x132,   /* cockpit_redraw.c's grab of the map's box */
    RADAR_X_640 = 487, RADAR_Y_640 = 452,   /* 'copd' / 'copl' of maskhi.fsh */
    GAUGE_X = 10, GAUGE_Y = 289             /* 'tach' of maskhi.fsh */
};

static const double PI = 3.14159265358979323846;

int screen_width(ScreenFormat f)
{
    switch (f) {
    case SCREEN_16_9: return WIDTH_16_9;
    case SCREEN_21_9: return WIDTH_21_9;
    default: return ORIGINAL_WIDTH;
    }
}

int screen_menu_left(int width)
{
    return (width - ORIGINAL_WIDTH) / 2;
}

/* The field of a viewport `size` across that keeps the focal length `focal` (pixels). */
static int field(double focal, double size)
{
    return (int)lround(2.0 * atan(size / 2.0 / focal) * 180.0 / PI);
}

int screen_fov_x(int fov_x_640, int width)
{
    const double focal = ORIGINAL_WIDTH / 2.0 / tan(fov_x_640 * PI / 360.0);
    return field(focal, width);
}

int screen_fov_y(int fov_y_orig, int height_orig, int height, int fov_adjust)
{
    const double focal = height_orig / 2.0 / tan(fov_y_orig * PI / 360.0);
    return field(focal, height) + fov_adjust;
}

int32_t screen_near_scale(int fov_y_exe, int fov_y)
{
    if (fov_y <= fov_y_exe) return 65536;
    return (int32_t)lround(tan(fov_y_exe * PI / 360.0) / tan(fov_y * PI / 360.0) * 65536.0);
}

int32_t screen_horizon_scale(int32_t scale, int width)
{
    if (width <= ORIGINAL_WIDTH) return scale;
    const int64_t num = (int64_t)scale * ORIGINAL_WIDTH, den = width;
    return (int32_t)((num + den / 2) / den);
}

void screen_layouts(int width, ViewLayout out[VIEW_LAYOUTS])
{
    const int left = screen_menu_left(width);
    for (int r = 0; r < VIEW_LAYOUTS; r++) {
        out[r] = race_view_layouts[HIGH_MODE][r];
        if (r == MIRROR_ROW || out[r].width == 0) continue;
        if (r == 0 || r == OUTSIDE_ROW) {
            out[r].width = width;
            out[r].unknown_20 = width;
        } else {
            out[r].left += left;
        }
    }
    /* the clip box only: the projection (its centre, its focal length) stays, so the picture above the old strip is
     * the same and the lines below it show more of the road */
    out[OUTSIDE_ROW].unknown_24 = OUTSIDE_HEIGHT;
}

CockpitScale screen_cockpit_scale(int width)
{
    CockpitScale s;
    s.scale = (int32_t)(((int64_t)width << 16) / ORIGINAL_WIDTH);
    s.crop_top = (int)(((int64_t)SCREEN_HEIGHT * s.scale >> 16) - SCREEN_HEIGHT) / 2;
    return s;
}

int screen_dash_x(int width, int x)
{
    if (width <= ORIGINAL_WIDTH) return x;
    const double half = ORIGINAL_WIDTH / 2.0, from_centre = x + 0.5 - width / 2.0;
    const double a = fabs(from_centre), edge = width / 2.0;
    const double in_dash = a <= DASH_MIDDLE ? a : DASH_MIDDLE + (a - DASH_MIDDLE) * (half - DASH_MIDDLE) / (edge - DASH_MIDDLE);
    const int sx = (int)floor(half + (from_centre < 0 ? -in_dash : in_dash));
    return sx < 0 ? 0 : sx >= ORIGINAL_WIDTH ? ORIGINAL_WIDTH - 1 : sx;
}

int screen_light_x(int width, int i)
{
    return i * (width - LIGHT_WIDTH) / (LIGHT_COUNT - 1);
}

int screen_dash_y(int y)
{
    return y < DASH_DROP + HUD_BAR_HEIGHT ? HUD_BAR_HEIGHT : y - DASH_DROP;
}

ViewLayout screen_cockpit_view(int width, const ViewLayout *row)
{
    ViewLayout v = *row;
    if (width <= ORIGINAL_WIDTH) return v;
    const int box = v.unknown_24 + DASH_DROP;
    v.width = width;   /* the centre in the middle; the field widened to keep the focal length (screen_fov_x) */
    v.y += DASH_DROP;
    v.unknown_20 = width;
    v.unknown_24 = box < SCREEN_HEIGHT ? box : SCREEN_HEIGHT;
    return v;
}

HudAnchors screen_hud_anchors(int width, const ScreenPoint sizes[7], ScreenPoint mirror_original)
{
    enum { START, MIDDLE, END, MIRROR, MAP, RADAR, GAUGE };
    if (width <= ORIGINAL_WIDTH)
        return (HudAnchors){ { 0, 0 }, { MIDDLE_X_640, 0 }, { END_X_640, 0 }, mirror_original,
                             { MAP_X_640, MAP_Y_640 }, { RADAR_X_640, RADAR_Y_640 }, { GAUGE_X, GAUGE_Y } };
    return (HudAnchors){
        { 0, 0 },
        { MIDDLE_X_640 + screen_menu_left(width), 0 },
        { width - sizes[END].x, 0 },
        { width - sizes[MIRROR].x, MIRROR_TOP },
        { width - sizes[MAP].x - MAP_MARGIN, SCREEN_HEIGHT - sizes[MAP].y - MAP_MARGIN },
        { sizes[START].x - sizes[RADAR].x / 2, HUD_BAR_HEIGHT },
        { GAUGE_X, GAUGE_Y }
    };
}

ScreenFormat screen_format_nearest(int desktop_w, int desktop_h)
{
    const double ratio = (double)desktop_w / (double)desktop_h;
    ScreenFormat best = SCREEN_4_3;
    double best_gap = 1e9;
    for (ScreenFormat f = SCREEN_4_3; f <= SCREEN_21_9; f++) {
        const double gap = fabs(ratio - (double)screen_width(f) / SCREEN_HEIGHT);
        if (gap < best_gap) best = f, best_gap = gap;
    }
    return best;
}
