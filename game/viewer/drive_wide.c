/* port: --drive's wide screen: the 3D view at the screen's width, the 640 frame (the race's 2D) composed over it, and
 * the viewports that go with them (layer_compose.h, screen_layout.h). */
#include "viewer/track_viewer.h"
#include "viewer/drive_session.h"
#include "game/gfx/blit.h"
#include "game/sys/backdrop.h"
#include "viewer/layer_compose.h"
#include "viewer/drive_light.h"
#include "viewer/dash_box.h"
#include "game/sys/bands.h"
#include "game/sys/port_log.h"
#include "game/sys/port_settings.h"
#include "game/race/car_draw.h"
#include "game/race/dash_color.h"
#include "game/race/dash_mask.h"
#include "game/race/wiper.h"
#include "viewer/ported_view.h"
#include "game/race/tach_dim.h"
#include "game/race/wheel_front.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DriveWide drive_wide = { SCREEN_W, NULL, { { 0 } }, 0, 0, 1, NULL };

/* port: the PS1 light's time of day tints the 3D alone, which needs the 3D in a buffer of its own under the 2D layer (the wide
 * path's), whatever the screen's width */
static int tinted_time(void) { return drive_light_time_tinted(); }   /* kept at the race's start: no port.cfg read a frame */

int drive_wide_active(void) { return drive_wide.width > SCREEN_W || drive_wide.scale > 1 || tinted_time(); }

uint8_t *drive_wide_canvas(void) { return drive_wide.scale > 1 ? drive_wide.layer : drive_wide.pixels; }

/* the layer's rows [top, bottom) as blocks over the 3D */
static void finish_band(void *user, int top, int bottom)
{
    (void)user;
    layer_upscale_rows(drive_wide.layer, drive_wide.width, drive_wide.scale, drive_wide.pixels, top, bottom);
}

/* the mirrors drawn at the scale this frame (drive_wide_hires_place) */
typedef struct {
    const uint8_t *shape;
    int sw, sh, x, y, w, h;
} HiresPlace;
static HiresPlace places[2];
static int place_count;

void drive_wide_hires_place(const DriveMirror *m, int x, int y, int w, int h)
{
    if (m->big == NULL || m->zoom <= 0 || w <= 0 || h <= 0 || place_count >= 2) return;
    places[place_count++] = (HiresPlace){ m->big, m->big_w, m->big_h, x, y, w, h };
}

int32_t drive_wide_mirror_zoom(const DriveMirror *m)
{
    return drive_wide.scale > 1 && !m->on_dash ? (int32_t)((int64_t)screen_mirror_scale(drive_wide.width) * drive_wide.scale) : 0;
}

int32_t drive_wide_bumper_zoom(void) { return drive_wide.scale > 1 ? drive_wide.scale << 16 : 0; }

static void put_places(void)
{
    const int s = drive_wide.scale, out_w = drive_wide.width * s, out_h = SCREEN_H * s;
    for (int i = 0; i < place_count; i++) {
        const HiresPlace *p = &places[i];
        const int w = p->w * s, h = p->h * s;
        for (int y = 0; y < h; y++) {
            const int dy = p->y * s + y;
            if (dy < 0 || dy >= out_h) continue;
            const uint8_t *from = p->shape + DRIVE_MIRROR_SHAPE_HEADER + (size_t)((int64_t)y * p->sh / h) * p->sw;
            for (int x = 0; x < w; x++) {
                const int dx = p->x * s + x;
                if (dx >= 0 && dx < out_w) drive_wide.pixels[(size_t)dy * out_w + dx] = from[(int64_t)x * p->sw / w];
            }
        }
    }
    place_count = 0;
}

void drive_wide_finish(void)
{
    if (drive_wide.scale <= 1 || drive_wide.layer == NULL || drive_wide.pixels == NULL) {
        place_count = 0;
        return;
    }
    bands_rows(0, SCREEN_H, finish_band, NULL);
    put_places();
}

ScreenFormat drive_screen_format(void)
{
    const char *v = getenv("TNFS_SCREEN");
    if (v != NULL) return strcmp(v, "21:9") == 0 ? SCREEN_21_9 : strcmp(v, "16:9") == 0 ? SCREEN_16_9 : SCREEN_4_3;
    return port_settings_load().format;
}

/* port: the road's near plane for fields widened by the field of view setting: the smaller of the two axes' */
static int32_t near_scale(int32_t fov_x_exe, int32_t fov_x, int32_t fov_y_exe, int32_t fov_y)
{
    const int32_t x = screen_near_scale(fov_x_exe, fov_x), y = screen_near_scale(fov_y_exe, fov_y);
    return x < y ? x : y;
}

/* The camera in use's layout as the 3D view's place on the frame. */
void drive_wide_set_viewport(const RaceState *race)
{
    const RaceCamera *camera = race->chase.camera;
    const ViewLayout *l = camera != NULL ? camera->layout_row : NULL;
    if (l == NULL || l->width <= 0 || l->height <= 0) return;
    /* port: a row taller than the exe's (the outside view without its strip) keeps its vertical focal length */
    const int32_t row = camera->layout >= 0 && camera->layout < VIEW_LAYOUTS ? camera->layout : 0;
    const int32_t fov = camera->fov + drive_wide.fov_adjust;
    const int32_t exe_height = race_view_layouts[HIGH_SCREEN][row].height;
    const int32_t fov_y = screen_fov_y(camera->fov, exe_height, l->height, drive_wide.fov_adjust);
    const int32_t near = near_scale(camera->fov, fov, screen_fov_y(camera->fov, exe_height, l->height, 0), fov_y);
    const PortedViewport vp = { l->left + l->x, l->top + l->y, l->width, l->height, fov, l->scale,
                                l->left, l->top, l->unknown_20, l->unknown_24, fov_y, near, 1 };
    ported_view_set_viewport(&vp);
}

/* port: the same camera at the wide screen's width: inside the car the 640 view enlarged with the dash, the outside
 * view wider at its focal lengths, the smaller windows centred */
PortedViewport drive_wide_viewport(const RaceState *race)
{
    const RaceCamera *camera = race->chase.camera;
    const int32_t row = camera != NULL && camera->layout >= 0 && camera->layout < VIEW_LAYOUTS ? camera->layout : 0;
    const int32_t base = camera != NULL ? camera->fov : 70;
    const ViewLayout *live = camera != NULL && camera->layout_row != NULL ? camera->layout_row
                                                                          : &race_view_layouts[HIGH_SCREEN][0];
    const ViewLayout l = row == 0 ? screen_cockpit_view(drive_wide.width, live) : drive_wide.rows[row];
    const ViewLayout *orig = &race_view_layouts[HIGH_SCREEN][row];
    /* every row keeps the 640's focal lengths at its width (inside the car too: the dash keeps its size) */
    const int32_t fov = (l.width != orig->width ? screen_fov_x(base, l.width) : base) + drive_wide.fov_adjust;
    const int32_t fov_y = screen_fov_y(base, orig->height, l.height, drive_wide.fov_adjust);
    const int32_t fov_y_exe = screen_fov_y(base, orig->height, l.height, 0);
    return (PortedViewport){ l.left + l.x, l.top + l.y, l.width, l.height, fov, l.scale,
                             l.left, l.top, l.unknown_20, l.unknown_24, fov_y,
                             near_scale(fov - drive_wide.fov_adjust, fov, fov_y_exe, fov_y), 1 };
}

/* port: the 640 view's box (its row's left, top, +0x20, +0x24) LAYER_KEY on the 640 surface the 3D would go to */
void drive_wide_key_box(const RaceState *race, uint8_t *surface)
{
    const ViewLayout *l = race->chase.camera != NULL ? race->chase.camera->layout_row : NULL;
    if (l == NULL) return;
    const int32_t x0 = l->left < 0 ? 0 : l->left, y0 = l->top < 0 ? 0 : l->top;
    const int32_t x1 = l->left + l->unknown_20 > SCREEN_W ? SCREEN_W : l->left + l->unknown_20;
    const int32_t y1 = l->top + l->unknown_24 > SCREEN_H ? SCREEN_H : l->top + l->unknown_24;
    for (int32_t y = y0; y < y1 && x1 > x0; y++) memset(surface + (size_t)y * SCREEN_W + x0, LAYER_KEY, (size_t)(x1 - x0));
}

enum {
    LIGHT_STEP_640 = 130,               /* the start lights' step (hud.c LIGHT_STEP_HIGH) */
    BAR_MIDDLE_640 = 262, BAR_END_640 = 405,  /* the scoreboard's groups (screen_layout.c) */
    GAUGE_X_640 = 10, GAUGE_Y_640 = 289, GAUGE_SIZE = 156,
    MIRROR_FRAME = 40                   /* the most the mirror's frame and stalk reach past its picture */
};

enum { STRIP_BACK_X = 258 };
enum { GRAB_X = 0x208, GRAB_Y = 0x132 };  /* where 00421160 grabs the map's back at 640 */  /* a column of the 640 strip with nothing on it, between its first two groups */

/* port: the strip between the scoreboard's groups, each line as the 640 strip's bare column (black, and the grey
 * edge on its last two lines) */
static void wide_strip_gap(const uint8_t *frame, int x0, int x1)
{
    for (int y = 0; y < HUD_BAR_HEIGHT && x1 > x0; y++)
        memset(drive_wide_canvas() + (size_t)y * drive_wide.width + x0, frame[y * SCREEN_W + STRIP_BACK_X], (size_t)(x1 - x0));
}

/* port: the dash's own art (004218d0's 'dash', its windshield LAYER_KEY), what the map's box covers */
static const uint8_t *dash_art_layer(const Cockpit *c)
{
    static uint8_t layer[SCREEN_W * SCREEN_H];
    static const uint8_t *drawn;
    if (c->dash != drawn) {
        memset(layer, LAYER_KEY, sizeof layer);
        const QuadTarget t = { layer, SCREEN_W, 0, 0, SCREEN_W, SCREEN_H };
        if (c->dash != NULL) blit_shape_at_transparent(&t, c->dash, 0, 0);
        drawn = c->dash;
    }
    return layer;
}

/* port: a box of the 640 dash where it lands on the wide screen (screen_dash_x / _y), drawn from `art` (LAYER_KEY
 * left alone) */
static void wide_art_placed(const LayerRect *r, const uint8_t *art, uint8_t *into, int as_class)
{
    const int *column = screen_dash_columns(drive_wide.width);
    const int width = drive_wide.width < SCREEN_DASH_MOST_WIDTH ? drive_wide.width : SCREEN_DASH_MOST_WIDTH;
    for (int y = 0; y < SCREEN_H; y++) {
        const int sy = screen_dash_y(y);
        if (sy < r->top || sy >= r->bottom) continue;
        for (int x = 0; x < width; x++) {
            const int sx = column[x];
            if (sx < r->left || sx >= r->right) continue;
            const uint8_t p = art[sy * SCREEN_W + sx];
            if (p != LAYER_KEY) into[(size_t)y * drive_wide.width + x] = as_class ? (uint8_t)PSX_CLASS_DASH : p;
        }
    }
}

/* port: 00421160's grab for the map's see-through back from the dash under the map's anchor, not the 640's */
void drive_wide_map_grab(uint8_t *back)
{
    if (drive_wide.width <= SCREEN_W) return;
    const ScreenPoint sizes[7] = { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { DRIVE_MAP_W, DRIVE_MAP_H }, { 0, 0 }, { 0, 0 } };
    const ScreenPoint at = screen_hud_anchors(drive_wide.width, sizes, (ScreenPoint){ 0, 0 }).map;
    layer_dash_grab(dash_art_layer(&drive_hud.cockpit), drive_wide.width, at.x + GRAB_X - DRIVE_MAP_X,
                    at.y + GRAB_Y - DRIVE_MAP_Y, back + DRIVE_VIEW_HEADER, DRIVE_BACK_W, DRIVE_BACK_H);
}

static int line_drawn(const uint8_t *frame, int x0, int y0, int x1, int y1)
{
    if (x0 < 0 || y0 < 0 || x1 > SCREEN_W || y1 > SCREEN_H) return 0;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            if (frame[y * SCREEN_W + x] != LAYER_KEY) return 1;
    return 0;
}

/* port: a piece over the windshield with the dash's art round it (the mirror's frame and stalk, the detector's
 * housing): `r` grown, at most `most` lines a side, while the line past each side still holds something drawn */
static LayerRect grow_piece(const uint8_t *frame, LayerRect r, int most)
{
    int grown[4] = { 0 }, changed = 1;
    while (changed) {
        changed = 0;
        if (grown[0] < most && line_drawn(frame, r.left - 1, r.top, r.left, r.bottom)) r.left--, grown[0]++, changed = 1;
        if (grown[1] < most && line_drawn(frame, r.left, r.top - 1, r.right, r.top)) r.top--, grown[1]++, changed = 1;
        if (grown[2] < most && line_drawn(frame, r.right, r.top, r.right + 1, r.bottom)) r.right++, grown[2]++, changed = 1;
        if (grown[3] < most && line_drawn(frame, r.left, r.bottom, r.right, r.bottom + 1)) r.bottom++, grown[3]++, changed = 1;
    }
    return r;
}

/* port: the widened mirror over its frame's two halves (drive_wide_compose): the frame's middle column drawn across the
 * gap between them, then the whole picture, enlarged as the frame */
static void wide_mirror(const uint8_t *frame, const DriveMirror *m, const LayerRect *r, ScreenPoint at,
                        const CockpitScale *cs)
{
    const int mid = drive_hud.mirror_x + (m->width - m->extra) / 2;
    const int x0 = at.x + (int)((int64_t)(mid - r->left) * cs->scale >> 16);
    const int gap = (int)((int64_t)m->extra * cs->scale >> 16);
    const int rows = (int)((int64_t)(r->bottom - r->top) * cs->scale >> 16);
    for (int y = 0; y < rows; y++) {
        const int sy = r->top + (int)(((int64_t)y << 16) / cs->scale), dy = at.y + y;
        const uint8_t p = frame[sy * SCREEN_W + mid];
        if (p == LAYER_KEY || dy < 0 || dy >= SCREEN_H) continue;
        for (int x = x0; x < x0 + gap && x < drive_wide.width; x++) if (x >= 0) drive_wide_canvas()[(size_t)dy * drive_wide.width + x] = p;
    }
    /* the picture's ends from the piece's origin, as the frame's pieces are placed: each scaled on its own, the last
     * column fell short of the frame's edge and the world showed through it */
    const int left = (int)((int64_t)(drive_hud.mirror_x - r->left) * cs->scale >> 16);
    const int top = (int)((int64_t)(drive_hud.mirror_y - r->top) * cs->scale >> 16);
    const int px = at.x + left, py = at.y + top;
    const int out_w = (int)((int64_t)(drive_hud.mirror_x - r->left + m->width) * cs->scale >> 16) - left;
    const int out_h = (int)((int64_t)(drive_hud.mirror_y - r->top + m->height) * cs->scale >> 16) - top;
    for (int y = 0; y < out_h; y++) {
        int sy = (int)(((int64_t)y << 16) / cs->scale);
        const int dy = py + y;
        if (sy >= m->height) sy = m->height - 1;
        if (dy < 0 || dy >= SCREEN_H) continue;
        for (int x = 0; x < out_w; x++) {
            int sx = (int)(((int64_t)x << 16) / cs->scale);
            const int dx = px + x;
            if (sx >= m->width) sx = m->width - 1;
            if (dx >= 0 && dx < drive_wide.width)
                drive_wide_canvas()[(size_t)dy * drive_wide.width + dx] = m->picture[DRIVE_VIEW_HEADER + sy * m->width + sx];
        }
    }
}

static LayerPiece wide_piece(int left, int top, int w, int h, ScreenPoint at, int scaled)
{
    return (LayerPiece){ { left, top, left + w, top + h }, at.x, at.y, scaled, 0 };
}

/* port: the gear gate art's real rectangle in the 640: the gate shape is the dash round the lever picture, so its box
 * is the rows and columns where it differs from the dash art under it. Kept for as long as the shape is the same. */
static LayerRect gate_rect(const uint8_t *g, const uint8_t *art)
{
    static const uint8_t *seen;
    static LayerRect rect;
    if (g == seen) return rect;
    seen = g;
    const int gw = g[4] | g[5] << 8, gh = g[6] | g[7] << 8, gx = (int16_t)(g[0xc] | g[0xd] << 8),
              gy = (int16_t)(g[0xe] | g[0xf] << 8);
    rect = (LayerRect){ DRIVE_MAP_X, DRIVE_MAP_Y, DRIVE_MAP_X + DRIVE_MAP_W, DRIVE_MAP_Y + DRIVE_MAP_H };
    if (gw <= 0 || gh <= 0 || gx < 0 || gy < 0 || gx + gw > SCREEN_W || gy + gh > SCREEN_H) return rect;
    LayerRect box = { gx + gw, gy + gh, gx, gy };
    for (int y = 0; y < gh; y++)
        for (int x = 0; x < gw; x++) {
            if (g[0x10 + y * gw + x] == art[(gy + y) * SCREEN_W + gx + x]) continue;
            if (gx + x < box.left) box.left = gx + x;
            if (gx + x + 1 > box.right) box.right = gx + x + 1;
            if (gy + y < box.top) box.top = gy + y;
            if (gy + y + 1 > box.bottom) box.bottom = gy + y + 1;
        }
    if (box.left < box.right && box.top < box.bottom) rect = box;
    return rect;
}

/* port: the box as a piece at its anchor: the map's whole rectangle with the map; with the gear gate only the lever
 * picture's own (the rest of the map's rectangle stays the stretched dash) */
static LayerPiece wide_box_piece(int map_on, ScreenPoint map_at)
{
    LayerRect r = { DRIVE_MAP_X, DRIVE_MAP_Y, DRIVE_MAP_X + DRIVE_MAP_W, DRIVE_MAP_Y + DRIVE_MAP_H };
    if (!map_on && drive_hud.cockpit.gate != NULL) r = gate_rect(drive_hud.cockpit.gate, dash_art_layer(&drive_hud.cockpit));
    return wide_piece(r.left, r.top, r.right - r.left, r.bottom - r.top,
                      (ScreenPoint){ map_at.x + r.left - DRIVE_MAP_X, map_at.y + r.top - DRIVE_MAP_Y }, 0);
}

/* port: the 640 frame (the race's 2D, LAYER_KEY where the 3D goes) over the wide 3D view: inside the car the dash
 * at its size with its sides stretched (screen_dash_x / _y), the radar detector on it, the mirror at its anchor; outside it centred with the gauge at the left;
 * the scoreboard's three groups and the map at their anchors in both; the police screen alone centred */
/* port (render bands): the compose's rows on bands (a band's rows are what the whole gives there) */
typedef struct {
    const LayerCompose *c;
    const uint8_t *frame;
} ComposeJob;

static void compose_band(void *user, int top, int bottom)
{
    const ComposeJob *job = user;
    layer_compose_rows(job->c, job->frame, drive_wide_canvas(), top, bottom);
}

static uint8_t *shadow_out;
static void shadow_band(void *user, int top, int bottom)
{
    const ComposeJob *job = user;
    layer_compose_rows(job->c, job->frame, shadow_out, top, bottom);
}

static void compose_banded(const LayerCompose *c, const uint8_t *frame)
{
    const ComposeJob job = { c, frame };
    bands_rows(0, c->layer_h, compose_band, (void *)&job);
}

/* port (the PS1 light's time of day, drive_light.h): which of the 2D layer's pixels are the dash's art (the dash, the wheel, the
 * pillars, the mirror's housing: half the tint) and which the HUD's (the top bar, the map, the gauge, the two dials with their needles
 * and the lights: none). The layer's own frame is classed pixel by pixel, then composed the way the frame is, so each output pixel has
 * its class whatever the piece or the stretch that put it there. */
enum { LIGHTS_PAD_LEFT = 2, LIGHTS_PAD_RIGHT = 14, LIGHTS_PAD_TOP = 3, LIGHTS_PAD_BOTTOM = 11 };   /* round the detector's lights */

/* port (gauge_light): the track's palette, for the lit parts of the gauges (a pixel's brightness); set before the layer is composed */
static const uint8_t *lit_palette;
void drive_wide_set_palette(const uint8_t *palette) { lit_palette = palette; }

/* a gauge's lit part: brighter than this (of 255, the channels' mean) */
enum { LIT_FROM = 96 };
static int lit_pixel(uint8_t v)
{
    return lit_palette != NULL && (lit_palette[3 * v] + lit_palette[3 * v + 1] + lit_palette[3 * v + 2]) / 3 >= LIT_FROM;
}

static int in_rect(int x, int y, int left, int top, int w, int h) { return x >= left && x < left + w && y >= top && y < top + h; }

/* the ellipse inscribed in the rectangle (a round dial's box has the dash's art in its corners) */
static int in_ellipse(int x, int y, int left, int top, int w, int h)
{
    if (w <= 0 || h <= 0) return 0;
    const long dx = 2L * x + 1 - 2L * left - w, dy = 2L * y + 1 - 2L * top - h;
    return dx * dx * h * h + dy * dy * w * w <= (long)w * w * h * h;
}

/* port (gauge_light): the hand-made mask of the dash on screen (dash_mask.h), loaded once per dash; NULL when it has none */
static const uint8_t *dash_mask_now(void)
{
    static uint8_t *mask;
    static char loaded[sizeof drive_hud.dash_stem];
    static int tried;
    if (!tried || strcmp(loaded, drive_hud.dash_stem) != 0) {
        free(mask);
        mask = dash_mask_load(drive_hud.dash_stem);
        snprintf(loaded, sizeof loaded, "%s", drive_hud.dash_stem);
        tried = 1;
    }
    return mask;
}

/* port (gauge_light = 5): the colours painted for the dash on screen (dash_color.h), loaded once per dash; NULL when it paints none */
static const DashColour *dash_colour_now(void)
{
    static DashColour colour;
    static char loaded[sizeof drive_hud.dash_stem];
    static int tried;
    if (!tried || strcmp(loaded, drive_hud.dash_stem) != 0) {
        dash_color_free(&colour);
        (void)dash_color_load(drive_hud.dash_stem, &colour);
        snprintf(loaded, sizeof loaded, "%s", drive_hud.dash_stem);
        tried = 1;
    }
    return colour.slot != NULL && colour.count > 0 ? &colour : NULL;
}

typedef char lit_colour_classes_are_the_dash_slots[DASH_COLOUR_SLOTS == PSX_CLASS_LIT_COLOURS && DASH_COLOUR_SLOTS == PLATFORM_LIT_SLOTS ? 1 : -1];

int drive_wide_lit_colours(uint8_t (*rgb)[3], int max)
{
    const DashColour *c = port_gauge_light() == 5 && drive_light_gauges_lit() ? dash_colour_now() : NULL;
    const int n = c == NULL ? 0 : c->count < max ? c->count : max;
    for (int i = 0; i < n; i++) memcpy(rgb[i], c->rgb[i], 3);
    return n;
}

/* port (gauge_light): inside a dial proper: the gauge's ellipse inset by a twelfth of its size each side (the rim is the dash's art) */
/* the ellipse of either dial's box */
static int in_gauge(const Cockpit *k, int x, int y)
{
    return in_ellipse(x, y, k->tach_x, k->tach_y, k->tach_w, k->tach_h) || in_ellipse(x, y, k->sped_x, k->sped_y, k->sped_w, k->sped_h);
}

static int in_dial(const Cockpit *k, int x, int y)
{
    return in_ellipse(x, y, k->tach_x + k->tach_w / 12, k->tach_y + k->tach_h / 12, k->tach_w - k->tach_w / 6, k->tach_h - k->tach_h / 6) ||
           in_ellipse(x, y, k->sped_x + k->sped_w / 12, k->sped_y + k->sped_h / 12, k->sped_w - k->sped_w / 6, k->sped_h - k->sped_h / 6);
}

/* a 640 x 480 picture of palette indices as <dir>/<stem>.<what>.pgm (dash_dump) */
static void dash_dump_pgm(const char *dir, const char *stem, const char *what, const uint8_t *pixels)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s.%s.pgm", dir, stem, what);
    FILE *f = fopen(path, "wb");
    if (f == NULL) return;
    fprintf(f, "P5\n%d %d\n255\n", SCREEN_W, SCREEN_H);
    fwrite(pixels, 1, (size_t)SCREEN_W * SCREEN_H, f);
    fclose(f);
}

/* port (dev, env only): TNFS_DASH_DUMP=<dir> writes each dash's 2D layer as <dir>/<STEM>.ppm (the dash art in the track's palette, the windshield black, and black too
 * wherever the steering wheel is in some pose: the wheel is drawn over the dash later, so the template does not hold it; wheel_front_zone) and a draft mask
 * <dir>/<STEM>.mask.png (the two gauge ellipses as DIAL), once per dash, for tools/dash_mask_psd.py's drafts */
/* the arrays dash_dump's parts share: the wheel's zone, the dials over it, and the dash with the wheel blanked */
static uint8_t dump_zone[SCREEN_W * SCREEN_H], dump_dials[SCREEN_W * SCREEN_H], dump_out[SCREEN_W * SCREEN_H];

/* <STEM>.ppm: the dash with the wheel blanked and the dials back over it, in the lit palette */
static void dash_dump_picture(const char *path, const Cockpit *k, const uint8_t *art)
{
    for (int y = 0; y < SCREEN_H; y++)
        for (int x = 0; x < SCREEN_W; x++) {
            const int i = y * SCREEN_W + x;
            const int dial = dump_dials[i] != LAYER_KEY && in_gauge(k, x, y);
            dump_out[i] = dump_zone[i] ? (dial ? dump_dials[i] : LAYER_KEY) : art[i];
        }
    FILE *f = fopen(path, "wb");
    if (f != NULL) {
        fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
        for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
            const uint8_t v = art[i];
            static const uint8_t black[3] = { 0, 0, 0 };
            fwrite(dump_out[i] == LAYER_KEY ? black : lit_palette + 3 * dump_out[i], 1, 3, f);
            (void)v;
        }
        fclose(f);
    }
}

/* the straight-ahead wheel, for a guide layer: the stored art where the wheel is in some pose (<STEM>.guide.ppm) and where that is (<STEM>.zone.pgm) */
static void dash_dump_guide(const char *dir, const char *done, const uint8_t *art)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s.guide.ppm", dir, done);
    FILE *f = fopen(path, "wb");
    if (f != NULL) {
        fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
        for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
            static const uint8_t black[3] = { 0, 0, 0 };
            fwrite(dump_zone[i] && art[i] != LAYER_KEY ? lit_palette + 3 * art[i] : black, 1, 3, f);
        }
        fclose(f);
    }
    snprintf(path, sizeof path, "%s/%s.zone.pgm", dir, done);
    if ((f = fopen(path, "wb")) != NULL) {
        fprintf(f, "P5\n%d %d\n255\n", SCREEN_W, SCREEN_H);
        for (int i = 0; i < SCREEN_W * SCREEN_H; i++) fputc(dump_zone[i] && art[i] != LAYER_KEY ? 255 : 0, f);
        fclose(f);
    }
}

/* the inputs of tools/dash_rebuild.py, which gives the layer the dash behind the wheel from the steering frames: the art's palette indices (<STEM>.art.pgm), the 'w' frames'
 * ('w'l and 'w'r, each steering frame: <STEM>.w<row>_<frame>.pgm, 255 where a frame draws nothing), the dials over the wheel (<STEM>.dials.pgm, 255 elsewhere) and the palette (<STEM>.pal) */
static void dash_dump_rebuild_inputs(const char *dir, const char *done, const Cockpit *k, const uint8_t *art)
{
    dash_dump_pgm(dir, done, "art", art);
    static uint8_t dial_only[SCREEN_W * SCREEN_H];
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        const int x = i % SCREEN_W, y = i / SCREEN_W;
        const int dial = dump_dials[i] != LAYER_KEY && in_gauge(k, x, y);
        dial_only[i] = dial ? dump_dials[i] : 255;
    }
    dash_dump_pgm(dir, done, "dials", dial_only);
    for (int row = 0; row < 2; row++)
        for (int frame = 0; frame < COCKPIT_FRAMES; frame++) {
            const uint8_t *shape = k->frames[row][frame];
            if (shape == NULL) continue;
            static uint8_t pic[SCREEN_W * SCREEN_H];
            for (int y = 0; y < SCREEN_H; y++)
                for (int x = 0; x < SCREEN_W; x++) pic[y * SCREEN_W + x] = wheel_front_pixel(shape, x, y) ? shape[0x10 + (y - (int16_t)(shape[0xe] | shape[0xf] << 8)) * (int16_t)(shape[4] | shape[5] << 8) + (x - (int16_t)(shape[0xc] | shape[0xd] << 8))] : 255;
            char what[16];
            snprintf(what, sizeof what, "w%d_%d", row, frame);
            dash_dump_pgm(dir, done, what, pic);
        }
}

/* <STEM>.pal, and <STEM>.mask.png: the draft of a hand-made mask (the dials' marks, else automatic) */
static void dash_dump_palette_and_mask(const char *dir, const char *done, const Cockpit *k)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s.pal", dir, done);
    FILE *f = fopen(path, "wb");
    if (f != NULL) {
        fwrite(lit_palette, 1, 768, f);
        fclose(f);
    }
    static uint8_t draft[SCREEN_W * SCREEN_H];
    for (int y = 0; y < SCREEN_H; y++)
        for (int x = 0; x < SCREEN_W; x++)
            draft[y * SCREEN_W + x] = in_dial(k, x, y) && dump_out[y * SCREEN_W + x] != LAYER_KEY ? DASH_MASK_DIAL : DASH_MASK_AUTO;
    uint8_t *png = NULL;
    size_t len = 0;
    snprintf(path, sizeof path, "%s/%s.mask.png", dir, done);
    f = fopen(path, "wb");
    if (f != NULL && dash_mask_write(draft, SCREEN_W, SCREEN_H, &png, &len)) fwrite(png, 1, len, f);
    if (f != NULL) fclose(f);
    free(png);
}

static void dash_dump(const Cockpit *k, const uint8_t *art)
{
    const char *dir = getenv("TNFS_DASH_DUMP");
    static char done[sizeof drive_hud.dash_stem];
    if (dir == NULL || lit_palette == NULL || strcmp(done, drive_hud.dash_stem) == 0) return;
    snprintf(done, sizeof done, "%s", drive_hud.dash_stem);
    char path[512];
    snprintf(path, sizeof path, "%s/%s.ppm", dir, done);
    wheel_front_zone(k, art, dump_zone);
    /* the dials the game draws over the 'w' frame inside the gauge ellipses ('tach', 'sped'): they come back over the blanked wheel */
    memset(dump_dials, LAYER_KEY, sizeof dump_dials);
    const QuadTarget dt = { dump_dials, SCREEN_W, 0, 0, SCREEN_W, SCREEN_H };
    if (k->tach != NULL) blit_shape_at_transparent(&dt, k->tach, 0, 0);
    if (k->sped != NULL) blit_shape_at_transparent(&dt, k->sped, 0, 0);
    dash_dump_picture(path, k, art);
    dash_dump_guide(dir, done, art);
    dash_dump_rebuild_inputs(dir, done, k, art);
    dash_dump_palette_and_mask(dir, done, k);
}

/* port (compose perf): what the class loop reads, so its rows can run on bands (each row is a function of the frame alone) */
typedef struct {
    const uint8_t *frame, *art;
    const Cockpit *k;
    const uint8_t *dash_mask;
    const DashColour *colour;   /* gauge_light 5: the car's colours, or NULL */
    uint8_t *classes;
    const uint8_t *wheel_t, *wheel_s, *wheel_w;
    int lights_left, lights_top, lights_w, lights_h;
    int in_car, map_on, dim_tach, wheel_gauge, wheel_masked, lit_keyed;
    const uint8_t *dials;   /* gauge_dials: per pixel GAUGE_ELLIPSE (inside a dial's box's ellipse), GAUGE_DIAL (inside the smaller dial) */
} ClassJob;

/* port (frame budget): the ellipses of the two dials are a function of the cockpit alone, so each pixel's two tests (four ellipse sums a pixel) are made once per cockpit, not per frame */
enum { GAUGE_ELLIPSE = 1, GAUGE_DIAL = 2 };

static const uint8_t *gauge_dials(const Cockpit *k)
{
    static uint8_t flags[SCREEN_W * SCREEN_H];
    static int key[8], made;
    const int now[8] = { k->tach_x, k->tach_y, k->tach_w, k->tach_h, k->sped_x, k->sped_y, k->sped_w, k->sped_h };
    if (made && memcmp(key, now, sizeof key) == 0) return flags;
    for (int y = 0; y < SCREEN_H; y++)
        for (int x = 0; x < SCREEN_W; x++)
            flags[y * SCREEN_W + x] = (uint8_t)((in_gauge(k, x, y) ? GAUGE_ELLIPSE : 0) |
                                                (in_dial(k, x, y) ? GAUGE_DIAL : 0));
    memcpy(key, now, sizeof key), made = 1;
    return flags;
}

static void class_band(void *user, int top, int bottom)
{
    const ClassJob *j = user;
    const uint8_t *const frame = j->frame, *const art = j->art, *const dash_mask = j->dash_mask;
    const DashColour *const colour = j->colour;
    const uint8_t *const wheel_t = j->wheel_t, *const wheel_s = j->wheel_s, *const wheel_w = j->wheel_w;
    uint8_t *const classes = j->classes;
    const int lights_left = j->lights_left, lights_top = j->lights_top, lights_w = j->lights_w, lights_h = j->lights_h;
    const uint8_t *const dials = j->dials;
    const int in_car = j->in_car, map_on = j->map_on, dim_tach = j->dim_tach, wheel_gauge = j->wheel_gauge,
              wheel_masked = j->wheel_masked, lit_keyed = j->lit_keyed;
    for (int y = top; y < bottom; y++)
        for (int x = 0; x < SCREEN_W; x++) {
            const uint8_t v = frame[y * SCREEN_W + x];
            if (v == LAYER_KEY) {   /* nothing drawn: the 3D's, whatever the rest says (every test below is on a drawn pixel) */
                classes[y * SCREEN_W + x] = LAYER_KEY;
                continue;
            }
            int hud = !in_car || y < HUD_BAR_HEIGHT || (map_on && in_rect(x, y, DRIVE_MAP_X, DRIVE_MAP_Y, DRIVE_MAP_W, DRIVE_MAP_H));
            hud = hud || (in_car && v != art[y * SCREEN_W + x] && in_rect(x, y, lights_left, lights_top, lights_w, lights_h));
            int gauge = dials[y * SCREEN_W + x] & GAUGE_ELLIPSE;
            if (gauge && wheel_gauge && (wheel_front_pixel(wheel_t, x, y) || wheel_front_pixel(wheel_s, x, y))) gauge = 0;   /* port (gauge_wheel): the wheel in front of the dials is the dash's */
            hud = hud || gauge;
            if (hud && dim_tach && tach_dim_face(drive_hud.art.tach, x, y, v, (uint8_t)drive_hud.needle_colour, LAYER_KEY)) hud = 0;   /* port (tach_dim): the dial's face is half tinted */
            uint8_t class = v == LAYER_KEY ? LAYER_KEY : hud ? PSX_CLASS_HUD : PSX_CLASS_DASH;
            /* port (gauge_light): in the cockpit at night, a dial's bright marks (the numerals, the ticks) are lit: not the needle, not what is the dash's art under the dial (the ellipse's corners) */
            if (wheel_masked && v != LAYER_KEY && wheel_front_now(gauge ? NULL : wheel_w, wheel_t, wheel_s, art, x, y)) class = PSX_CLASS_DASH;   /* the wheel: the dash's half tint, whatever the mask says (in a gauge the dial is drawn over the 'w' frame: only the 't' / 's' frames are the wheel there) */
            else if (lit_keyed && v != LAYER_KEY && v != (uint8_t)drive_hud.needle_colour) {
                if (dash_mask != NULL) {   /* a hand-made mask decides alone: the automatic marks are off */
                    const uint8_t m = dash_mask[y * SCREEN_W + x];
                    if (m == DASH_MASK_DASH) class = PSX_CLASS_DASH;
                    else if (m == DASH_MASK_LIT || (m == DASH_MASK_DIAL && lit_pixel(v))) class = PSX_CLASS_LIT;
                } else if (class == PSX_CLASS_HUD && gauge) {
                    if (!(dials[y * SCREEN_W + x] & GAUGE_DIAL)) class = PSX_CLASS_DASH;   /* the ellipse's rim holds the dash's art (the speedo's arc): the dash's, not the dial's */
                    else if (lit_pixel(v)) class = PSX_CLASS_LIT;
                }
            }
            if (class == PSX_CLASS_LIT && colour != NULL && colour->slot[y * SCREEN_W + x] != 0)
                class = (uint8_t)(PSX_CLASS_LIT_COLOUR + colour->slot[y * SCREEN_W + x] - 1);   /* painted: its own colour; else the orange of gauge_light 3 */
            classes[y * SCREEN_W + x] = class;
        }
}

/* `placed`: the 640 box where the map was and the dash is drawn instead (wide_art_placed), NULL when the map is there or it is not the dash;
 * `map_on`: the map is in its box (off, the box holds the dash's art and is the dash's like the rest of it) */
static void compose_classes(const LayerCompose *c, const uint8_t *frame, int in_car, const LayerRect *placed, int map_on)
{
    static uint8_t classes[SCREEN_W * SCREEN_H], *shadow;
    static size_t shadow_size;
    const size_t out = (size_t)drive_wide.width * SCREEN_H;
    if (shadow_size != out) {
        uint8_t *p = realloc(shadow, out);
        if (p == NULL) return;
        shadow = p, shadow_size = out;
    }
    const Cockpit *k = &drive_hud.cockpit;
    const uint8_t *art = dash_art_layer(k);
    if (in_car) dash_dump(k, art);
    /* the radar detector's lights (cockpit_lights: the big one at dash point 2, the five small at 3..7): they are lights, untinted; what in
     * their box the lights drew over the dash's art (the case round them is the art's: half) */
    int lights_left = k->dash_points[2].x, lights_right = lights_left, lights_top = k->dash_points[2].y;
    for (int i = 3; i < 8; i++) {
        if (k->dash_points[i].x < lights_left) lights_left = k->dash_points[i].x;
        if (k->dash_points[i].x > lights_right) lights_right = k->dash_points[i].x;
    }
    lights_left -= LIGHTS_PAD_LEFT, lights_top -= LIGHTS_PAD_TOP;
    const int lights_w = lights_right - lights_left + LIGHTS_PAD_RIGHT, lights_h = LIGHTS_PAD_TOP + LIGHTS_PAD_BOTTOM;
    const int lit_keyed = in_car && drive_light_gauges_lit() && port_gauge_light() > 0;   /* port (gauge_light) */
    const uint8_t *const dash_mask = lit_keyed ? dash_mask_now() : NULL;
    const DashColour *const colour = lit_keyed && port_gauge_light() == 5 ? dash_colour_now() : NULL;
    const int dim_tach = !in_car && drive_hud.police.screen == NULL && port_tach_dim();   /* port (tach_dim) */
    /* port (gauge_wheel): the 't' and 's' frames of this steering frame, the wheel drawn over the dials */
    const Car *const driven = drive_sim.race.player >= 0 && drive_sim.race.player < RACE_CARS ? drive_sim.race.cars[drive_sim.race.player] : NULL;
    const int wheel_gauge = in_car && driven != NULL && port_gauge_wheel();
    const int wheel_masked = lit_keyed && driven != NULL && dash_mask != NULL;   /* the wheel is drawn after the dash and turns: the mask never classes it */
    const int steer_negative = driven != NULL && driven->steer < 0;
    const int wheel_any = wheel_gauge || wheel_masked;
    const uint8_t *const wheel_t = wheel_any ? wheel_front_shape(k, 0, drive_hud.hud.unknown_4f9a20[0], steer_negative, drive_sim.race.mirrored) : NULL;
    const uint8_t *const wheel_s = wheel_any ? wheel_front_shape(k, 1, drive_hud.hud.unknown_4f9a20[0], steer_negative, drive_sim.race.mirrored) : NULL;
    const uint8_t *const wheel_w = wheel_masked ? wheel_front_shape(k, 2, drive_hud.hud.unknown_4f9a20[0], steer_negative, drive_sim.race.mirrored) : NULL;
    const ClassJob job = { frame, art, k, dash_mask, colour, classes, wheel_t, wheel_s, wheel_w, lights_left, lights_top, lights_w, lights_h,
                           in_car, map_on, dim_tach, wheel_gauge, wheel_masked, lit_keyed, gauge_dials(k) };
    bands_rows(0, SCREEN_H, class_band, (void *)&job);
    memset(shadow, LAYER_KEY, out);
    shadow_out = shadow;
    if (placed != NULL) wide_art_placed(placed, art, shadow, 1);   /* the dash placed where the map was is the dash's: half the tint, not the 3D's */
    const ComposeJob cj = { c, classes };
    bands_rows(0, c->layer_h, shadow_band, (void *)&cj);
    drive_light_world_class(shadow, drive_wide.width, SCREEN_H, drive_wide.scale);
}

/* what drive_wide_compose decides about this frame and hands to its parts */
typedef struct {
    const DriveMirror *m;
    int police, in_car, mirror_on, lights, map_on, box_on;
    LayerRect mirror;
    int32_t mirror_scale;
    int mirror_w;
    HudAnchors a;
} WideFrame;

/* the frame's layout: who is drawn (the police screen, the cockpit, the mirror) and where the HUD's pieces go on the wide screen */
static WideFrame wide_frame_start(const RaceState *race, const uint8_t *frame)
{
    WideFrame w;
    memset(&w, 0, sizeof w);
    w.m = &drive_hud.mirror;
    const RaceCamera *camera = race->chase.camera;
    w.police = drive_hud.police.screen != NULL;
    w.in_car = !w.police && camera != NULL && camera->layout == 0;
    const DriveMirror *m = w.m;
    w.mirror_on = w.in_car && m->picture != NULL && m->state != 0 && !m->on_dash;  /* on the dash: with it */
    const LayerRect picture = { drive_hud.mirror_x, drive_hud.mirror_y, drive_hud.mirror_x + m->width - m->extra,
                                drive_hud.mirror_y + m->height };  /* as the frame holds it: a widened one's middle */
    w.mirror = w.mirror_on ? grow_piece(frame, picture, MIRROR_FRAME) : picture;
    w.mirror_scale = screen_mirror_scale(drive_wide.width);
    w.mirror_w = (int)((int64_t)(w.mirror.right - w.mirror.left + (w.mirror_on ? m->extra : 0)) * w.mirror_scale >> 16);
    const int mirror_h = (int)((int64_t)(w.mirror.bottom - w.mirror.top) * w.mirror_scale >> 16);
    const ScreenPoint sizes[7] = { { BAR_MIDDLE_640, HUD_BAR_HEIGHT }, { BAR_END_640 - BAR_MIDDLE_640, HUD_BAR_HEIGHT },
                                   { SCREEN_W - BAR_END_640, HUD_BAR_HEIGHT }, { w.mirror_w, mirror_h },
                                   { DRIVE_MAP_W, DRIVE_MAP_H }, { 0, 0 },   /* the radar detector stays on the dash */
                                   { GAUGE_SIZE, GAUGE_SIZE } };
    w.a = screen_hud_anchors(drive_wide.width, sizes, (ScreenPoint){ w.mirror.left, w.mirror.top });
    /* the box where the exe's 640 frame has it (the anchor is the dash's grab, 2 px on) when the wide path is on only for the dawn / dusk tint:
     * that frame is the day's; the other reasons keep the wide path's own place */
    if (drive_wide.width <= SCREEN_W && drive_light_time_tinted()) w.a.map = (ScreenPoint){ DRIVE_MAP_X, DRIVE_MAP_Y };
    return w;
}

/* the pieces of the layer, in order: the start lights or the bar's three groups with the map's box, the mirror (whole, or in two apart), or the gauge; returns how many */
static int wide_pieces(const WideFrame *w, LayerPiece pieces[12])
{
    const DriveMirror *m = w->m;
    const HudAnchors *a = &w->a;
    int n = 0;
    if (w->lights) {  /* the start lights whole, each at its width spread across the screen, not cut with the groups */
        for (int i = 0; i < LIGHT_COUNT; i++)
            pieces[n++] = wide_piece(i * LIGHT_STEP_640, 0, LIGHT_WIDTH, HUD_BAR_HEIGHT,
                                     (ScreenPoint){ screen_light_x(drive_wide.width, i), 0 }, 0);
        if (w->box_on) pieces[n++] = wide_box_piece(w->map_on, a->map);
    } else if (!w->police) {
        pieces[n++] = wide_piece(0, 0, BAR_MIDDLE_640, HUD_BAR_HEIGHT, a->bar_start, 0);
        pieces[n++] = wide_piece(BAR_MIDDLE_640, 0, BAR_END_640 - BAR_MIDDLE_640, HUD_BAR_HEIGHT, a->bar_middle, 0);
        pieces[n++] = wide_piece(BAR_END_640, 0, SCREEN_W - BAR_END_640, HUD_BAR_HEIGHT, a->bar_end, 0);
        if (w->box_on) pieces[n++] = wide_box_piece(w->map_on, a->map);
    }
    if (w->in_car) {
        if (w->mirror_on && m->extra == 0) pieces[n++] = (LayerPiece){ w->mirror, a->mirror.x, a->mirror.y, 1, 0 };
        if (w->mirror_on && m->extra != 0) {  /* the frame in two, either side of the picture's middle, apart by the extra */
            const int mid = drive_hud.mirror_x + (m->width - m->extra) / 2;
            const int apart = (int)((int64_t)(mid - w->mirror.left + m->extra) * w->mirror_scale >> 16);
            pieces[n++] = (LayerPiece){ { w->mirror.left, w->mirror.top, mid, w->mirror.bottom }, a->mirror.x, a->mirror.y, 1, 0 };
            pieces[n++] = (LayerPiece){ { mid, w->mirror.top, w->mirror.right, w->mirror.bottom }, a->mirror.x + apart, a->mirror.y, 1,
                                        w->mirror_w - apart };   /* to the mirror's end, as anchored */
        }
    } else if (!w->police) {
        pieces[n++] = wide_piece(GAUGE_X_640, GAUGE_Y_640, GAUGE_SIZE, GAUGE_SIZE, a->gauge, 0);
    }
    return n;
}

/* the mirror's picture where the layer puts it (and the PS1 light's tint reaching it), the widened mirror's middle, and the strips between the lights or the bar's groups */
static void wide_mirror_and_strips(const WideFrame *w, const uint8_t *frame)
{
    const DriveMirror *m = w->m;
    const HudAnchors *a = &w->a;
    const CockpitScale mirror_cs = { w->mirror_scale, screen_cockpit_scale(drive_wide.width).crop_top };
    if (w->mirror_on) {  /* the picture's place in the layer, as wide_mirror puts it */
        const int left = (int)((int64_t)(drive_hud.mirror_x - w->mirror.left) * w->mirror_scale >> 16);
        const int top = (int)((int64_t)(drive_hud.mirror_y - w->mirror.top) * w->mirror_scale >> 16);
        const int out_w = (int)((int64_t)(drive_hud.mirror_x - w->mirror.left + m->width) * w->mirror_scale >> 16) - left;
        const int out_h = (int)((int64_t)(drive_hud.mirror_y - w->mirror.top + m->height) * w->mirror_scale >> 16) - top;
        drive_wide_hires_place(m, a->mirror.x + left, a->mirror.y + top, out_w, out_h);
        const int s = drive_wide.scale;   /* the picture shows the 3D: the PS1 light's tint reaches it */
        drive_light_world_rect(drive_wide.width * s, (a->mirror.x + left) * s, (a->mirror.y + top) * s, out_w * s, out_h * s);
    }
    if (w->mirror_on && m->extra != 0) wide_mirror(frame, m, &w->mirror, a->mirror, &mirror_cs);
    if (w->lights) {  /* the strip between the lights */
        for (int i = 0; i + 1 < LIGHT_COUNT; i++)
            wide_strip_gap(frame, screen_light_x(drive_wide.width, i) + LIGHT_WIDTH, screen_light_x(drive_wide.width, i + 1));
    } else if (!w->police) {  /* the strip between the groups */
        wide_strip_gap(frame, a->bar_start.x + BAR_MIDDLE_640, a->bar_middle.x);
        wide_strip_gap(frame, a->bar_middle.x + BAR_END_640 - BAR_MIDDLE_640, a->bar_end.x);
    }
}

/* port (wipers): where the blades are on the dash (layer pixels), for every dash from its art, as a tandem pair on the FULL windscreen: the dash view shows only about 62% of the glass, the rest is past the right edge, so
 * the glass's width is the visible width over WIPER_VISIBLE_SHARE; the left (driver) pivot is low, a gap right of the wheel's left rim, a little below the glass's bottom edge (hidden by the dash: the blade comes out from behind the wheel and the dash top, as in DriveClub), the second near the glass's centre (often past the view), both blades the same length, parked flat to the
 * right of their pivot along the bottom edge's slope and sweeping together up to the left. The glass is raked and seen from the left of its centre, so a blade turns in the glass's plane and the plane's axes put it
 * on the screen: along the edge (u) as it is, up the glass (v) squashed by WIPER_RAKE. The sweep is cut only against the left pillar and the glass's top (the right edge of the view cuts nothing). */
enum { WIPER_PROBE_X = 300, WIPER_SPAN_DEG = 90, WIPER_FREE_X = 540, WIPER_LEFT_PIVOT_X = 333, WIPER_PIVOT_DROP = 8, WIPER_FIT_FROM = 7 };   /* the driver's pivot: a gap right of the wheel's left rim (wheel_left_rim; WIPER_LEFT_PIVOT_X is its fallback), y this far below the edge */
static const float WIPER_VISIBLE_SHARE = 0.62f, WIPER_RAKE = 0.55f, WIPER_LEN_SHARE = 0.45f;

/* the glass (the art's nothing, or what its mask gives to the blades) */
static int wiper_glass(const uint8_t *art, const uint8_t *mask, int x, int y)
{
    const size_t i = (size_t)y * SCREEN_W + (size_t)x;
    return art[i] == LAYER_KEY || (mask != NULL && mask[i] == DASH_MASK_BLADE);
}

static float windscreen_bottom(const uint8_t *art, const uint8_t *mask, int x)
{
    int y = SCREEN_H - 40;   /* up from the dash: the first pixel of glass is the windscreen's bottom edge there */
    while (y > HUD_BAR_HEIGHT + 10 && !wiper_glass(art, mask, x, y)) y--;
    return (float)y - 2.0f;
}

/* the glass's extent along a row of the art: the first column left and right of `x` that is not glass */
static int glass_edge(const uint8_t *art, const uint8_t *mask, int y, int x, int step)
{
    while (x + step >= 0 && x + step < SCREEN_W && wiper_glass(art, mask, x + step, y)) x += step;
    return x;
}

/* how far (degrees) a blade turns from its park and stays on the glass: the angle before the first at which a point of its outer 40%, is off the art's glass, past the left side or above the
 * view. Points in the right part of the view (WIPER_FREE_X on) are not looked at. */
static float blade_fit(const uint8_t *art, const uint8_t *mask, const WiperBlade *b)
{
    float ok = 0.0f;
    const float rad = 3.14159265f / 180.0f;
    for (float a = 24.0f;   /* the bottom edge is curved, a line is only its slope: the first degrees are the edge itself */ a <= (float)WIPER_SPAN_DEG; a += 2.0f) {
        for (int k = WIPER_FIT_FROM; k <= 10; k++) {   /* the blade's outer part: its root is behind the wheel and the dash */
            float dx, dy;
            wiper_offset(b, b->len * (float)k / 10.0f, b->park + a * rad, &dx, &dy);
            const int px = (int)(b->x + dx), py = (int)(b->y + dy);
            if (px >= WIPER_FREE_X) continue;   /* the right part of the view: the mirror is drawn over it and the right pillar is off the view */
            if (px < 0 || py < 0 || py >= SCREEN_H || !wiper_glass(art, mask, px, py)) return ok;
        }
        ok = a;
    }
    return ok;
}

/* the windscreen's glass alone: the art's glass (nothing, or the mask's blade pixels) connected to the point (x, y), by 4 neighbours, so the holes of the wheel and the rest of the cockpit's art are not in it; cached per dash art */
static const uint8_t *wiper_glass_flags(const uint8_t *art, const uint8_t *mask, int x, int y)
{
    static uint8_t flags[SCREEN_W * SCREEN_H];
    static int stack[SCREEN_W * SCREEN_H];
    static const uint8_t *made_for;
    if (made_for == drive_hud.cockpit.dash) return flags;
    memset(flags, 0, sizeof flags);
    size_t top = 0;
    if (wiper_glass(art, mask, x, y)) stack[top++] = y * SCREEN_W + x, flags[y * SCREEN_W + x] = 1;
    while (top > 0) {
        const int at = stack[--top], ax = at % SCREEN_W, ay = at / SCREEN_W;
        const int nx[4] = { ax - 1, ax + 1, ax, ax }, ny[4] = { ay, ay, ay - 1, ay + 1 };
        for (int k = 0; k < 4; k++) {
            if (nx[k] < 0 || nx[k] >= SCREEN_W || ny[k] < 0 || ny[k] >= SCREEN_H) continue;
            const int n = ny[k] * SCREEN_W + nx[k];
            if (flags[n] == 0 && wiper_glass(art, mask, nx[k], ny[k])) flags[n] = 1, stack[top++] = n;
        }
    }
    made_for = drive_hud.cockpit.dash;
    return flags;
}

/* the x of the steering wheel's left rim (its outer edge), read from the art: where the glass's lower boundary is pushed above the edge's line by the wheel's top, the bump is the arc of a circle, whose chord (c) and height (h)
 * give its radius (c*c/4 + h*h) / 2h and its centre; the rim is the centre less the radius. `fallback` when there is no bump. */
static float wheel_left_rim(const uint8_t *flags, float ya, float tn, int xa, float fallback)
{
    int x1 = -1, x2 = -1;
    float h = 0.0f;
    for (int x = 0; x < SCREEN_W; x++) {
        int y = SCREEN_H - 1;
        while (y > 0 && flags[y * SCREEN_W + x] == 0) y--;   /* the glass's lowest pixel in this column */
        const float up = (ya - tn * (float)(x - xa)) - (float)y;   /* how far above the edge's line */
        if (up > 6.0f) {
            if (x1 < 0) x1 = x;
            x2 = x;
            if (up > h) h = up;
        }
    }
    if (x1 < 0 || x2 - x1 < 40 || h < 8.0f) return fallback;
    const float c = (float)(x2 - x1), r = (c * c / 4.0f + h * h) / (2.0f * h);
    return (float)(x1 + x2) / 2.0f - r;
}

int drive_wide_wiper_scene(const RaceState *race, WiperBlade *out)
{
    ported_view_set_wiper_glass(NULL, 0.0f, 0.0f, 1.0f);
    const RaceCamera *camera = race != NULL ? race->chase.camera : NULL;
    if (!port_wipers() || camera == NULL || camera->layout != 0 || drive_hud.police.screen != NULL || drive_hud.cockpit.dash == NULL) return 0;
    const uint8_t *const art = dash_art_layer(&drive_hud.cockpit), *const mask = dash_mask_now();
    const float PI = 3.14159265f, deg = PI / 180.0f;
    const int row = (int)windscreen_bottom(art, mask, WIPER_PROBE_X) - 10;
    const int gl = glass_edge(art, mask, row, WIPER_PROBE_X, -1), gr = glass_edge(art, mask, row, WIPER_PROBE_X, 1);
    const float seen = (float)(gr - gl), full = seen / WIPER_VISIBLE_SHARE;
    const int xa = gl + (int)(seen * 0.1f), xb = gl + (int)(seen * 0.9f);
    const float ya = windscreen_bottom(art, mask, xa), yb = windscreen_bottom(art, mask, xb);
    const float slope = atan2f(ya - yb, (float)(xb - xa)), tn = tanf(slope);   /* the edge's, up to the right */
    const uint8_t *const flags = wiper_glass_flags(art, mask, WIPER_PROBE_X, row);
    const float rim = wheel_left_rim(flags, ya, tn, xa, (float)WIPER_LEFT_PIVOT_X), gap = (float)gl + full * 0.5f - (float)WIPER_LEFT_PIVOT_X, len = full * WIPER_LEN_SHARE;
    const float lx = rim + gap, rx = rim + 2.0f * gap;   /* the driver's pivot a gap right of the wheel's left rim, the second a gap further */
    const float ux = cosf(slope), uy = sinf(slope), vx = -sinf(slope) * WIPER_RAKE, vy = cosf(slope) * WIPER_RAKE;
    WiperBlade layer[WIPER_BLADES] = {
        { lx, ya - tn * (lx - (float)xa) + (float)WIPER_PIVOT_DROP, len, slope, 0.0f, ux, uy, vx, vy, (float)SCREEN_W },
        { rx, ya - tn * (rx - (float)xa), len, slope, 0.0f, ux, uy, vx, vy, (float)SCREEN_W }
    };
    float span = blade_fit(art, mask, &layer[0]);
    const float right = blade_fit(art, mask, &layer[1]);
    if (right < span) span = right;
    if (span < 50.0f) span = 50.0f;
    const int wide = drive_wide_active();   /* the 640x480 path (the exe's) has its dash at the layer's own coordinates */
    const float scale = wide ? (float)drive_wide.scale : 1.0f, x0 = wide ? (float)((drive_wide.width - SCREEN_W) / 2) : 0.0f, y0 = wide && drive_wide.width > SCREEN_W ? (float)DASH_DROP : 0.0f;
    for (int i = 0; i < WIPER_BLADES; i++) {
        WiperBlade b = layer[i];
        b.top = b.park + span * deg;
        b.x = (x0 + b.x) * scale, b.y = (y0 + b.y) * scale, b.len *= scale, b.xmax = (x0 + b.xmax) * scale;
        out[i] = b;
    }
    ported_view_set_wiper_glass(flags, x0 * scale, y0 * scale, scale);
    return WIPER_BLADES;
}

/* port (wipers): `frame` with the pixels the dash's mask marks as a wiper blade (dash_mask.h BLADE) made nothing, as the windscreen is; `frame` itself when the dash has none */
static const uint8_t *blades_hidden(const uint8_t *frame)
{
    static uint8_t copy[SCREEN_W * SCREEN_H];
    const uint8_t *const mask = dash_mask_now();
    if (mask == NULL) return frame;
    int any = 0;
    memcpy(copy, frame, sizeof copy);
    for (size_t i = 0; i < sizeof copy; i++)
        if (mask[i] == DASH_MASK_BLADE) copy[i] = LAYER_KEY, any = 1;
    return any ? copy : frame;
}

/* port (wipers): on the 640x480 path the 3D (`view`) reaches the screen through the windscreen's run lists only, so the art's own blades stay: where the dash's mask marks a blade, the 3D's pixel goes on the screen
 * (the sweeping blades are in it). The frame's mirror, gauges and the rest are drawn after, as ever. */
void drive_wide_wipers_reveal(uint8_t *screen, const uint8_t *view)
{
    const uint8_t *const mask = dash_mask_now();
    if (mask == NULL || drive_wide_active() || !ported_view_wipers_active()) return;
    for (size_t i = 0; i < (size_t)SCREEN_W * SCREEN_H; i++)
        if (mask[i] == DASH_MASK_BLADE) screen[i] = view[i];
}

void drive_wide_compose(const RaceState *race, const uint8_t *frame)
{
    place_count = 0;
    WideFrame w = wide_frame_start(race, frame);
    drive_light_world_rect(drive_wide.width * drive_wide.scale, 0, 0, 0, 0);
    LayerPiece pieces[12];
    w.lights = !w.police && hud_start_lights_drawn(&drive_hud.hud, race->tick);
    /* 004f9b3c: off, the 640's map box holds the dash, and for a moment the gear gate (0041f5bc's state not 0). The box
     * is a piece at its size and anchor with the map, or with the gate; with neither it is the continuous dash. */
    w.map_on = drive_hud.keys.use_parts != 0;
    w.box_on = dash_box_shown((DashBox)drive_hud.dash_box, drive_hud.keys.use_parts, drive_hud.gate_state, w.in_car);
    const int n = wide_pieces(&w, pieces);
    if (w.police && drive_hud.rgb != NULL) {  /* 004214e4's screen over its darkened, enlarged copy, as the menus */
        static uint8_t screen[SCREEN_W * SCREEN_H];
        uint8_t shade[256];
        for (size_t i = 0; i < sizeof screen; i++) screen[i] = frame[i] == LAYER_KEY ? 0 : frame[i];  /* none on black */
        backdrop_shade_table(drive_hud.rgb, shade);
        backdrop_compose(screen, drive_wide_canvas(), drive_wide.width, shade);
        drive_wide.ready = 1;
        return;
    }
    if (w.police) memset(drive_wide_canvas(), 0, (size_t)drive_wide.width * SCREEN_H);  /* 004214e4's screen on black */
    else if (drive_wide.scale > 1) memset(drive_wide.layer, LAYER_KEY, (size_t)drive_wide.width * SCREEN_H);  /* the 3D shows through */
    const LayerRect map = { DRIVE_MAP_X, DRIVE_MAP_Y, DRIVE_MAP_X + DRIVE_MAP_W, DRIVE_MAP_Y + DRIVE_MAP_H };
    if (w.in_car && w.box_on) wide_art_placed(&map, dash_art_layer(&drive_hud.cockpit), drive_wide_canvas(), 0);  /* the dash where the map was */
    const LayerCompose c = { SCREEN_W, SCREEN_H, drive_wide.width, w.mirror_scale, w.in_car, pieces, n };
    const uint8_t *const shown = w.in_car && ported_view_wipers_active() ? blades_hidden(frame) : frame;   /* port (wipers): the dash art's own blades are not there when ours sweep */
    compose_banded(&c, shown);
    if (drive_light_active()) compose_classes(&c, shown, w.in_car, w.in_car && w.box_on ? &map : NULL, w.map_on);
    wide_mirror_and_strips(&w, frame);
    drive_wide.ready = 1;
}

/* port: a 640 screen of its own (the link race's wait) centred on black on the wide screen */
void drive_wide_centre(const uint8_t *frame)
{
    if (!drive_wide_active()) return;
    memset(drive_wide_canvas(), 0, (size_t)drive_wide.width * SCREEN_H);
    const LayerCompose c = { SCREEN_W, SCREEN_H, drive_wide.width, 1 << 16, 0, NULL, 0 };
    layer_compose(&c, frame, drive_wide_canvas());
    drive_wide_finish();
}

int drive_screen_width(void)
{
    return drive_wide.width > SCREEN_W ? drive_wide.width : SCREEN_W;
}

/* port: what the frame shows: the wide screen's when there is one (width * scale x 480 * scale) */
const uint8_t *drive_output(const uint8_t *pixels, int *width)
{
    *width = drive_wide_active() ? drive_wide.width * drive_wide.scale : SCREEN_W;
    return drive_wide_active() ? drive_wide.pixels : pixels;
}

int drive_output_height(void)
{
    return drive_wide_active() ? SCREEN_H * drive_wide.scale : SCREEN_H;
}

/* port: the race drawn `width` wide (the headless frames at the format's width, the window at its platform's), and at
 * port.cfg's render scale (TNFS_RENDER_SCALE) times that each way */
static int scale_off;
void drive_wide_scale_off(void) { scale_off = 1; }

/* automatic at a render scale of 2 or 3; TNFS_CAR_DRAW_DISTANCE=0 (dev only) keeps the exe's distances */
static int car_distance_wanted(void)
{
    const char *const v = getenv("TNFS_CAR_DRAW_DISTANCE");
    return !(v != NULL && v[0] == '0' && v[1] == '\0');
}

void drive_wide_size(int width)
{
    drive_wide.width = width;
    drive_wide.scale = scale_off ? 1 : port_render_scale();
    car_draw_set_distance_scale(car_distance_wanted() ? drive_wide.scale : 1);   /* port: cars drawn as far as the scale shows them */
    drive_mirror_widen(&drive_hud.mirror, width > SCREEN_W);
    free(drive_wide.layer);
    drive_wide.layer = NULL;
    if (!drive_wide_active()) return;
    screen_layouts(drive_wide.width > SCREEN_W ? drive_wide.width : SCREEN_W, drive_wide.rows);
    const size_t out = (size_t)drive_wide.width * drive_wide.scale * SCREEN_H * drive_wide.scale;
    uint8_t *p = realloc(drive_wide.pixels, out);
    uint8_t *layer = drive_wide.scale > 1 ? malloc((size_t)drive_wide.width * SCREEN_H) : NULL;
    if (p == NULL || (drive_wide.scale > 1 && layer == NULL)) {
        port_log("track_viewer: no wide screen buffer, 640 x 480\n");
        drive_wide.pixels = p != NULL ? p : drive_wide.pixels;
        free(layer);
        drive_wide.width = SCREEN_W, drive_wide.scale = 1;
        return;
    }
    drive_wide.pixels = p;
    drive_wide.layer = layer;
    memset(p, 0, out);
}
