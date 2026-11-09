/* port (the tunnel lamps, psx_lamps.h; psx_flares): the lamp textures of the frame, their tables and the halos drawn round them, moved out of
 * ported_view.c. The state is the lamps the track file names, the bitmaps they are drawn on and the halos noted while the frame was drawn. */
#include "game/viewer/view_lamps.h"

#include "game/gfx/quad.h"
#include "game/race/lamp_flare.h"
#include "game/sys/palette.h"
#include "game/sys/port_log.h"

#include <math.h>
#include <string.h>

/* port (the tunnel lamps, psx_lamps.h): the lamp textures of the frame, by their bitmaps, and the darkening tables that let their fitting's
 * colours through; remade with `lit_tables` */
enum { LAMPS_MAX = 4, LAMP_VARIANTS = 8 };
static PsxLamp lamp_spec[LAMPS_MAX];
static int lamp_count;
static const FshEntry *lamp_bitmap[LAMPS_MAX][LAMP_VARIANTS];
static uint8_t lamp_tables[LAMPS_MAX][MODERN_FOG_LEVELS][256];

void ported_view_set_psx_lamps(const PsxLamp *lamps, int count)
{
    lamp_count = lamps != NULL && count > 0 ? (count < LAMPS_MAX ? count : LAMPS_MAX) : 0;
    for (int i = 0; i < lamp_count; i++) lamp_spec[i] = lamps[i];
}

/* port (psx_flares): the tunnel lamps' glow. Each lamp texture's polygon drawn notes where the lamp's rectangle lies on the screen (the rectangle's centre
 * across the polygon's corners); once the 3D is done a halo is drawn at each that still shows one of the lamp's own colours at that pixel (nothing nearer
 * covers it; the colours stay unchanged through the lamp tables, shared entries or not), by day and by night. */
static int flare_on;   /* port (psx_flares): a soft halo round a lit brake light and the tunnel lamps (lamp_flare.h) */
enum { TUNNEL_FLARES_MAX = 128 };
typedef struct { int lamp, x, y, size; } TunnelFlare;
static TunnelFlare tunnel_flares[TUNNEL_FLARES_MAX];
static int tunnel_flare_count;
static uint8_t lamp_exempt_set[LAMPS_MAX][256];
static uint8_t lamp_glow[LAMPS_MAX];   /* each lamp's brightest colour: the halo's */

void view_lamps_note(const FshEntry *texture, const int32_t uv[8], const int32_t screen[8])
{
    if (!flare_on || tunnel_flare_count >= TUNNEL_FLARES_MAX) return;
    const int lamp = view_lamps_of(texture);
    if (lamp < 0) return;
    const PsxLamp *const l = &lamp_spec[lamp];
    const int tw = (int16_t)texture->width, th = (int16_t)texture->height;
    const int x1 = l->rect[2] < tw ? l->rect[2] + 1 : tw, y1 = l->rect[3] < th ? l->rect[3] + 1 : th;
    const double cu = 65536.0 * (l->rect[0] + x1) / 2, cv = 65536.0 * (l->rect[1] + y1) / 2;
    /* the polygon's uv is affine on the corners 0, 1 and 3 (any rotation or mirroring of the texture): solve uv0 + a (uv1 - uv0) + b (uv3 - uv0) = (cu, cv) */
    const double e1u = (double)uv[2] - uv[0], e1v = (double)uv[3] - uv[1], e2u = (double)uv[6] - uv[0], e2v = (double)uv[7] - uv[1];
    const double det = e1u * e2v - e1v * e2u;
    if (fabs(det) < 1) return;
    const double ru = cu - uv[0], rv = cv - uv[1];
    const double a = (ru * e2v - rv * e2u) / det, b = (e1u * rv - e1v * ru) / det;
    if (a < 0 || a > 1 || b < 0 || b > 1) return;
    double p[2];
    for (int k = 0; k < 2; k++) {
        const double top = screen[k] + (screen[2 + k] - screen[k]) * a, bottom = screen[6 + k] + (screen[4 + k] - screen[6 + k]) * a;
        p[k] = top + (bottom - top) * b;
    }
    const double quad_w = hypot((double)screen[2] - screen[0], (double)screen[3] - screen[1]);
    const double rect_w = (double)(x1 - l->rect[0]) * 65536.0, edge_u = hypot(e1u, e1v);
    tunnel_flares[tunnel_flare_count++] = (TunnelFlare){ lamp, (int)lrint(p[0]), (int)lrint(p[1]), (int)lrint(quad_w * rect_w / edge_u) };
}

void view_lamps_note_full(const FshEntry *texture, const int32_t screen[8])
{
    const int32_t right = ((int32_t)(int16_t)texture->width << 16) - 1, bottom = ((int32_t)(int16_t)texture->height << 16) - 1;
    const int32_t uv[8] = { 0, 0, right, 0, right, bottom, 0, bottom };
    view_lamps_note(texture, uv, screen);
}

/* the lamp `texture` is, or -1 */
int view_lamps_of(const FshEntry *texture)
{
    for (int i = 0; i < lamp_count; i++)
        for (int v = 0; v < LAMP_VARIANTS; v++)
            if (lamp_bitmap[i][v] == texture) return i;
    return -1;
}

/* port (psx_flares): the lamp textures and their colours when the light's tables are not made (the light is off but the lamps are set) */
void view_lamps_textures(const TrackArt *art)
{
    for (int i = 0; i < lamp_count; i++) {
        for (int v = 0; v < LAMP_VARIANTS; v++) {
            const TrackTexture *t = lamp_spec[i].slot >= 0 && lamp_spec[i].slot < TRACK_TEXTURE_SLOTS && v < TRACK_TEXTURE_VARIANTS
                                        ? &art->slots[lamp_spec[i].slot][v] : NULL;
            lamp_bitmap[i][v] = t != NULL && t->present ? &t->bitmap : NULL;
        }
        const FshEntry *t = lamp_bitmap[i][0];
        psx_lamp_exempt(&lamp_spec[i], t != NULL ? t->payload : NULL, t != NULL ? t->width : 0, t != NULL ? t->height : 0, lamp_exempt_set[i]);
        int best = -1, best_sum = -1;
        for (int c = 0; c < 256; c++) {
            const uint8_t *const q = art->palette + 3 * c;
            const int hi = q[0] > q[1] ? (q[0] > q[2] ? q[0] : q[2]) : (q[1] > q[2] ? q[1] : q[2]), lo = q[0] < q[1] ? (q[0] < q[2] ? q[0] : q[2]) : (q[1] < q[2] ? q[1] : q[2]);
            const int sum = lamp_exempt_set[i][c] ? q[0] + q[1] + q[2] + 3 * (hi - lo) : -1;   /* bright, and the colour rather than the white glare */
            if (sum > best_sum) best = c, best_sum = sum;
        }
        lamp_glow[i] = (uint8_t)(best >= 0 ? best : 0);
    }
}

/* port: the lamps' textures of this track in each of their variants, and their tables from the world's tunnel tables and the colours each shows
 * (cheap: a texture and 16 tables) */
void view_lamps_make_tables(const TrackArt *art, const uint8_t (*tunnel_tables)[256])
{
    for (int i = 0; i < lamp_count; i++)   /* the lamp textures of this track, in each of their variants */
        for (int v = 0; v < LAMP_VARIANTS; v++) {
            const TrackTexture *t = lamp_spec[i].slot >= 0 && lamp_spec[i].slot < TRACK_TEXTURE_SLOTS && v < TRACK_TEXTURE_VARIANTS
                                        ? &art->slots[lamp_spec[i].slot][v] : NULL;
            lamp_bitmap[i][v] = t != NULL && t->present ? &t->bitmap : NULL;
        }
    for (int i = 0; i < lamp_count; i++) {   /* the lamps' tables from the world's and the colours each shows (cheap: a texture and 16 tables) */
        const FshEntry *t = lamp_bitmap[i][0];
        uint8_t exempt[256];
        psx_lamp_exempt(&lamp_spec[i], t != NULL ? t->payload : NULL, t != NULL ? t->width : 0, t != NULL ? t->height : 0, exempt);
        psx_lamp_tables(exempt, (const uint8_t (*)[256])tunnel_tables, MODERN_FOG_LEVELS, lamp_tables[i]);   /* lamps hang in tunnels: their dim parts are cast too */
        memcpy(lamp_exempt_set[i], exempt, sizeof exempt);
        int best = -1, best_sum = -1;
        for (int c = 0; c < 256; c++) {
            const uint8_t *const q = art->palette + 3 * c;
        const int hi = q[0] > q[1] ? (q[0] > q[2] ? q[0] : q[2]) : (q[1] > q[2] ? q[1] : q[2]), lo = q[0] < q[1] ? (q[0] < q[2] ? q[0] : q[2]) : (q[1] < q[2] ? q[1] : q[2]);
        const int sum = exempt[c] ? q[0] + q[1] + q[2] + 3 * (hi - lo) : -1;
            if (sum > best_sum) best = c, best_sum = sum;
        }
        lamp_glow[i] = (uint8_t)(best >= 0 ? best : 0);
    }
}

/* port (psx_flares): the halos of the tunnel lamps noted this frame (tunnel_flare_note), over the finished 3D */
void view_lamps_draw(const QuadTarget *t, const uint8_t palette[768])
{
    if (!flare_on || tunnel_flare_count <= 0 || t == NULL || t->pixels == NULL) return;
    static const int PERCENT[LAMP_FLARE_LEVELS] = { 30, 60, 100 };
    const uint8_t *const usable = palette_usable();
    uint8_t maps[LAMPS_MAX][LAMP_FLARE_LEVELS][256];
    int made[LAMPS_MAX] = { 0 };
    for (int i = 0; i < tunnel_flare_count; i++) {
        const TunnelFlare *f = &tunnel_flares[i];
        if (f->x < t->clip_left || f->x >= t->clip_right || f->y < t->clip_top || f->y >= t->clip_bottom) continue;
        int shows = 0;   /* one of the lamp's colours within two pixels of its centre: else what is nearer covers it */
        for (int dy = -2; dy <= 2 && !shows; dy++)
            for (int dx = -2; dx <= 2 && !shows; dx++) {
                const int x = f->x + dx, y = f->y + dy;
                shows = x >= t->clip_left && x < t->clip_right && y >= t->clip_top && y < t->clip_bottom &&
                        lamp_exempt_set[f->lamp][t->pixels[(size_t)y * (size_t)t->pitch + (size_t)x]];
            }
        if (!shows) continue;
        const uint8_t entry = lamp_glow[f->lamp];
        if (!made[f->lamp]) {
            for (int l = 0; l < LAMP_FLARE_LEVELS; l++)
                for (int d = 0; d < 256; d++) {
                    int rgb[3];
                    for (int k = 0; k < 3; k++) {
                        rgb[k] = palette[3 * d + k] + palette[3 * entry + k] * PERCENT[l] / 100;
                        if (rgb[k] > 255) rgb[k] = 255;
                    }
                    maps[f->lamp][l][d] = lamp_exempt_set[f->lamp][d] || !usable[d] ? (uint8_t)d : (uint8_t)palette_nearest(palette, usable, rgb[0], rgb[1], rgb[2]);
                }
            made[f->lamp] = 1;
        }
        const uint8_t *const map[LAMP_FLARE_LEVELS] = { maps[f->lamp][0], maps[f->lamp][1], maps[f->lamp][2] };
        lamp_halo_draw(t->pixels, t->pitch, t->clip_left, t->clip_top, t->clip_right - 1, t->clip_bottom - 1, f->x, f->y,
                       lamp_flare_radius(f->size * f->size), map);
        static int logged;
        if (logged++ < 4) port_log("tunnel_flare: halo at %d,%d size %d (lamp %d, glow entry %d)\n", f->x, f->y, f->size, f->lamp, entry);
    }
}

void view_lamps_set_flares(int on) { flare_on = on != 0; }
int view_lamps_flares(void) { return flare_on; }
int view_lamps_count(void) { return lamp_count; }
uint8_t (*view_lamps_table(int lamp))[256] { return lamp_tables[lamp]; }
void view_lamps_frame_start(void) { tunnel_flare_count = 0; }
