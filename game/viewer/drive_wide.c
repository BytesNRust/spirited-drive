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

static int in_rect(int x, int y, int left, int top, int w, int h) { return x >= left && x < left + w && y >= top && y < top + h; }

/* the ellipse inscribed in the rectangle (a round dial's box has the dash's art in its corners) */
static int in_ellipse(int x, int y, int left, int top, int w, int h)
{
    if (w <= 0 || h <= 0) return 0;
    const long dx = 2L * x + 1 - 2L * left - w, dy = 2L * y + 1 - 2L * top - h;
    return dx * dx * h * h + dy * dy * w * w <= (long)w * w * h * h;
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
    /* the radar detector's lights (cockpit_lights: the big one at dash point 2, the five small at 3..7): they are lights, untinted; what in
     * their box the lights drew over the dash's art (the case round them is the art's: half) */
    int lights_left = k->dash_points[2].x, lights_right = lights_left, lights_top = k->dash_points[2].y;
    for (int i = 3; i < 8; i++) {
        if (k->dash_points[i].x < lights_left) lights_left = k->dash_points[i].x;
        if (k->dash_points[i].x > lights_right) lights_right = k->dash_points[i].x;
    }
    lights_left -= LIGHTS_PAD_LEFT, lights_top -= LIGHTS_PAD_TOP;
    const int lights_w = lights_right - lights_left + LIGHTS_PAD_RIGHT, lights_h = LIGHTS_PAD_TOP + LIGHTS_PAD_BOTTOM;
    for (int y = 0; y < SCREEN_H; y++)
        for (int x = 0; x < SCREEN_W; x++) {
            const uint8_t v = frame[y * SCREEN_W + x];
            int hud = !in_car || y < HUD_BAR_HEIGHT || (map_on && in_rect(x, y, DRIVE_MAP_X, DRIVE_MAP_Y, DRIVE_MAP_W, DRIVE_MAP_H));
            hud = hud || (in_car && v != art[y * SCREEN_W + x] && in_rect(x, y, lights_left, lights_top, lights_w, lights_h));
            hud = hud || in_ellipse(x, y, k->tach_x, k->tach_y, k->tach_w, k->tach_h) || in_ellipse(x, y, k->sped_x, k->sped_y, k->sped_w, k->sped_h);
            classes[y * SCREEN_W + x] = v == LAYER_KEY ? LAYER_KEY : hud ? PSX_CLASS_HUD : PSX_CLASS_DASH;
        }
    memset(shadow, LAYER_KEY, out);
    if (placed != NULL) wide_art_placed(placed, art, shadow, 1);   /* the dash placed where the map was is the dash's: half the tint, not the 3D's */
    layer_compose(c, classes, shadow);
    drive_light_world_class(shadow, drive_wide.width, SCREEN_H, drive_wide.scale);
}

void drive_wide_compose(const RaceState *race, const uint8_t *frame)
{
    const RaceCamera *camera = race->chase.camera;
    place_count = 0;
    const int police = drive_hud.police.screen != NULL;
    const int in_car = !police && camera != NULL && camera->layout == 0;
    const CockpitScale cs = screen_cockpit_scale(drive_wide.width);
    const DriveMirror *m = &drive_hud.mirror;
    const int mirror_on = in_car && m->picture != NULL && m->state != 0 && !m->on_dash;  /* on the dash: with it */
    const LayerRect picture = { drive_hud.mirror_x, drive_hud.mirror_y, drive_hud.mirror_x + m->width - m->extra,
                                drive_hud.mirror_y + m->height };  /* as the frame holds it: a widened one's middle */
    const LayerRect mirror = mirror_on ? grow_piece(frame, picture, MIRROR_FRAME) : picture;
    const int32_t mirror_scale = screen_mirror_scale(drive_wide.width);
    const int mirror_w = (int)((int64_t)(mirror.right - mirror.left + (mirror_on ? m->extra : 0)) * mirror_scale >> 16);
    const int mirror_h = (int)((int64_t)(mirror.bottom - mirror.top) * mirror_scale >> 16);
    const ScreenPoint sizes[7] = { { BAR_MIDDLE_640, HUD_BAR_HEIGHT }, { BAR_END_640 - BAR_MIDDLE_640, HUD_BAR_HEIGHT },
                                   { SCREEN_W - BAR_END_640, HUD_BAR_HEIGHT }, { mirror_w, mirror_h },
                                   { DRIVE_MAP_W, DRIVE_MAP_H }, { 0, 0 },   /* the radar detector stays on the dash */
                                   { GAUGE_SIZE, GAUGE_SIZE } };
    HudAnchors a = screen_hud_anchors(drive_wide.width, sizes, (ScreenPoint){ mirror.left, mirror.top });
    /* the box where the exe's 640 frame has it (the anchor is the dash's grab, 2 px on) when the wide path is on only for the dawn / dusk tint:
     * that frame is the day's; the other reasons keep the wide path's own place */
    if (drive_wide.width <= SCREEN_W && drive_light_time_tinted()) a.map = (ScreenPoint){ DRIVE_MAP_X, DRIVE_MAP_Y };
    drive_light_world_rect(drive_wide.width * drive_wide.scale, 0, 0, 0, 0);
    LayerPiece pieces[12];
    int n = 0;
    const int lights = !police && hud_start_lights_drawn(&drive_hud.hud, race->tick);
    /* 004f9b3c: off, the 640's map box holds the dash, and for a moment the gear gate (0041f5bc's state not 0). The box
     * is a piece at its size and anchor with the map, or with the gate; with neither it is the continuous dash. */
    const int map_on = drive_hud.keys.use_parts != 0;
    const int box_on = dash_box_shown((DashBox)drive_hud.dash_box, drive_hud.keys.use_parts, drive_hud.gate_state, in_car);
    if (lights) {  /* the start lights whole, each at its width spread across the screen, not cut with the groups */
        for (int i = 0; i < LIGHT_COUNT; i++)
            pieces[n++] = wide_piece(i * LIGHT_STEP_640, 0, LIGHT_WIDTH, HUD_BAR_HEIGHT,
                                     (ScreenPoint){ screen_light_x(drive_wide.width, i), 0 }, 0);
        if (box_on) pieces[n++] = wide_box_piece(map_on, a.map);
    } else if (!police) {
        pieces[n++] = wide_piece(0, 0, BAR_MIDDLE_640, HUD_BAR_HEIGHT, a.bar_start, 0);
        pieces[n++] = wide_piece(BAR_MIDDLE_640, 0, BAR_END_640 - BAR_MIDDLE_640, HUD_BAR_HEIGHT, a.bar_middle, 0);
        pieces[n++] = wide_piece(BAR_END_640, 0, SCREEN_W - BAR_END_640, HUD_BAR_HEIGHT, a.bar_end, 0);
        if (box_on) pieces[n++] = wide_box_piece(map_on, a.map);
    }
    if (in_car) {
        if (mirror_on && m->extra == 0) pieces[n++] = (LayerPiece){ mirror, a.mirror.x, a.mirror.y, 1, 0 };
        if (mirror_on && m->extra != 0) {  /* the frame in two, either side of the picture's middle, apart by the extra */
            const int mid = drive_hud.mirror_x + (m->width - m->extra) / 2;
            const int apart = (int)((int64_t)(mid - mirror.left + m->extra) * mirror_scale >> 16);
            pieces[n++] = (LayerPiece){ { mirror.left, mirror.top, mid, mirror.bottom }, a.mirror.x, a.mirror.y, 1, 0 };
            pieces[n++] = (LayerPiece){ { mid, mirror.top, mirror.right, mirror.bottom }, a.mirror.x + apart, a.mirror.y, 1,
                                        mirror_w - apart };   /* to the mirror's end, as anchored */
        }
    } else if (!police) {
        pieces[n++] = wide_piece(GAUGE_X_640, GAUGE_Y_640, GAUGE_SIZE, GAUGE_SIZE, a.gauge, 0);
    }
    if (police && drive_hud.rgb != NULL) {  /* 004214e4's screen over its darkened, enlarged copy, as the menus */
        static uint8_t screen[SCREEN_W * SCREEN_H];
        uint8_t shade[256];
        for (size_t i = 0; i < sizeof screen; i++) screen[i] = frame[i] == LAYER_KEY ? 0 : frame[i];  /* none on black */
        backdrop_shade_table(drive_hud.rgb, shade);
        backdrop_compose(screen, drive_wide_canvas(), drive_wide.width, shade);
        drive_wide.ready = 1;
        return;
    }
    if (police) memset(drive_wide_canvas(), 0, (size_t)drive_wide.width * SCREEN_H);  /* 004214e4's screen on black */
    else if (drive_wide.scale > 1) memset(drive_wide.layer, LAYER_KEY, (size_t)drive_wide.width * SCREEN_H);  /* the 3D shows through */
    const LayerRect map = { DRIVE_MAP_X, DRIVE_MAP_Y, DRIVE_MAP_X + DRIVE_MAP_W, DRIVE_MAP_Y + DRIVE_MAP_H };
    if (in_car && box_on) wide_art_placed(&map, dash_art_layer(&drive_hud.cockpit), drive_wide_canvas(), 0);  /* the dash where the map was */
    const LayerCompose c = { SCREEN_W, SCREEN_H, drive_wide.width, mirror_scale, in_car, pieces, n };
    compose_banded(&c, frame);
    if (drive_light_active()) compose_classes(&c, frame, in_car, in_car && box_on ? &map : NULL, map_on);
    const CockpitScale mirror_cs = { mirror_scale, cs.crop_top };
    if (mirror_on) {  /* the picture's place in the layer, as wide_mirror puts it */
        const int left = (int)((int64_t)(drive_hud.mirror_x - mirror.left) * mirror_scale >> 16);
        const int top = (int)((int64_t)(drive_hud.mirror_y - mirror.top) * mirror_scale >> 16);
        const int out_w = (int)((int64_t)(drive_hud.mirror_x - mirror.left + m->width) * mirror_scale >> 16) - left;
        const int out_h = (int)((int64_t)(drive_hud.mirror_y - mirror.top + m->height) * mirror_scale >> 16) - top;
        drive_wide_hires_place(m, a.mirror.x + left, a.mirror.y + top, out_w, out_h);
        const int s = drive_wide.scale;   /* the picture shows the 3D: the PS1 light's tint reaches it */
        drive_light_world_rect(drive_wide.width * s, (a.mirror.x + left) * s, (a.mirror.y + top) * s, out_w * s, out_h * s);
    }
    if (mirror_on && m->extra != 0) wide_mirror(frame, m, &mirror, a.mirror, &mirror_cs);
    if (lights) {  /* the strip between the lights */
        for (int i = 0; i + 1 < LIGHT_COUNT; i++)
            wide_strip_gap(frame, screen_light_x(drive_wide.width, i) + LIGHT_WIDTH, screen_light_x(drive_wide.width, i + 1));
    } else if (!police) {  /* the strip between the groups */
        wide_strip_gap(frame, a.bar_start.x + BAR_MIDDLE_640, a.bar_middle.x);
        wide_strip_gap(frame, a.bar_middle.x + BAR_END_640 - BAR_MIDDLE_640, a.bar_end.x);
    }
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

void drive_wide_size(int width)
{
    drive_wide.width = width;
    drive_wide.scale = scale_off ? 1 : port_render_scale();
    car_draw_set_distance_scale(port_settings_load().car_draw_distance ? drive_wide.scale : 1);   /* port: cars drawn as far as the scale shows them */
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
