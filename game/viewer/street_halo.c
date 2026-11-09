#include "game/viewer/street_halo.h"

#include "game/race/lamp_flare.h"
#include "game/sys/palette.h"
#include "game/sys/port_log.h"
#include "game/sys/port_settings.h"

#include <math.h>
#include <string.h>

/* port (lamp_halos): the screen rectangles of the lamp boards' sprites drawn this frame; a halo goes only to a blob inside one (a traffic sign the reflectors light is painted in the same entries) */
enum { STREET_RECT_MARGIN = 3 };
static LampRects street_rects;
static const FshEntry *street_bitmap[3];
static int street_count;
static uint8_t street_entry[3], window_entry;

void street_halo_boards(const FshEntry *const bitmaps[3], int count, const uint8_t entry[3], uint8_t window)
{
    street_count = bitmaps != NULL && entry != NULL && count > 0 ? (count < 3 ? count : 3) : 0;
    for (int i = 0; i < 3; i++) street_bitmap[i] = i < street_count ? bitmaps[i] : NULL;
    if (street_count > 0) memcpy(street_entry, entry, sizeof street_entry);
    window_entry = window;
}

void street_halo_frame_begin(void) { lamp_rects_reset(&street_rects); }

static int street_board(const FshEntry *texture)
{
    for (int i = 0; i < street_count; i++)
        if (street_bitmap[i] != NULL && texture == street_bitmap[i]) return 1;
    return 0;
}

static void street_push(int x0, int y0, int x1, int y1)
{
    lamp_rects_push(&street_rects, x0, y0, x1, y1);
}

/* the exe's mappers: the polygon's screen corners */
void street_halo_note(const FshEntry *texture, const int32_t screen[8])
{
    if (!port_lamp_halos() || !street_board(texture)) return;
    int x0 = screen[0], y0 = screen[1], x1 = screen[0], y1 = screen[1];
    for (int k = 1; k < 4; k++) {
        x0 = screen[2 * k] < x0 ? screen[2 * k] : x0, x1 = screen[2 * k] > x1 ? screen[2 * k] : x1;
        y0 = screen[2 * k + 1] < y0 ? screen[2 * k + 1] : y0, y1 = screen[2 * k + 1] > y1 ? screen[2 * k + 1] : y1;
    }
    street_push(x0, y0, x1, y1);
}

/* Modern 3D: the corners in camera space, projected as modern_road.c does; a board the near plane clips takes the whole view (it is that close) */
void street_halo_note_cam(const FshEntry *texture, const View *view, const int32_t *const cam[4])
{
    if (!port_lamp_halos() || view == NULL || !street_board(texture)) return;
    const float focal = (float)view->focal_x;
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    for (int i = 0; i < 4; i++) {
        const float z = (float)cam[i][2];
        if (!(z >= (float)view->near_z) || !(z > 0.0f)) {
            street_push(-100000, -100000, 100000, 100000);
            return;
        }
        const float x = view->centre_x + (float)cam[i][0] * focal / z, y = view->centre_y - (float)cam[i][1] * focal / z;
        if (i == 0 || x < x0) x0 = x;
        if (i == 0 || x > x1) x1 = x;
        if (i == 0 || y < y0) y0 = y;
        if (i == 0 || y > y1) y1 = y;
    }
    street_push((int)floorf(x0), (int)floorf(y0), (int)ceilf(x1), (int)ceilf(y1));
}

/* port (lamp_halos): the street lamps' halo at night. The lamp board's three entries (street_entry) are emissive, so a lamp on screen after the 3D is a blob of them; the window entry is never
 * a member. The nearest (largest) STREET_HALO_MAX blobs get the halo, in the brightest of the three entries' colour; the tables are made again only when the palette or the colour changes */
enum { STREET_HALO_MAX = 24, STREET_HALO_SCAN = 96, STREET_HALO_REACH = 200 };   /* STREET_HALO_REACH: percent of the usual radius: a lamp is far and small, its halo has to read */

void street_halo_draw(const QuadTarget *t, const uint8_t palette[768], int night)
{
    if (!port_lamp_halos() || street_count <= 0 || !night || t == NULL || t->pixels == NULL) return;
    uint8_t member[256] = { 0 };
    for (int k = 0; k < 3; k++) member[street_entry[k]] = 1;
    member[window_entry] = 0;
    LampBlob blobs[STREET_HALO_SCAN];   /* more than are drawn: the signs' blobs, large, are dropped after the scan */
    int count = lamp_blobs_frame(t->pixels, t->pitch, t->clip_left, t->clip_top, t->clip_right - 1, t->clip_bottom - 1, member, blobs, STREET_HALO_SCAN);
    static int logged_drops;
    if (street_rects.dropped > 0 && !logged_drops++) port_log("street_halo: %d lamp-board rectangles past %d were dropped; the blobs in them get no halo\n", street_rects.dropped, LAMP_RECTS_MAX);
    int kept = 0;
    for (int i = 0; i < count; i++)   /* only a lamp's own sprite: not a sign the reflectors light in the same entries */
        if (lamp_blob_in_rects(&blobs[i], (const int (*)[4])street_rects.rects, street_rects.count, STREET_RECT_MARGIN)) blobs[kept++] = blobs[i];
    count = kept < STREET_HALO_MAX ? kept : STREET_HALO_MAX;
    if (count <= 0) return;
    static uint8_t maps[3][LAMP_FLARE_LEVELS][256], made_palette[768];
    static int made_window = -1;
    static int made[3];
    if (made_window != window_entry || memcmp(made_palette, palette, 768) != 0) {   /* the tables are made again only when the palette changes */
        memset(made, 0, sizeof made);
        memcpy(made_palette, palette, 768), made_window = window_entry;
    }
    static const int PERCENT[LAMP_FLARE_LEVELS] = { 30, 60, 100 };
    const uint8_t *const usable = palette_usable();
    for (int i = 0; i < count; i++) {
        int k = 0;   /* the halo is the colour of the blob's own entry: a lamp's orange stays orange */
        while (k < 2 && street_entry[k] != blobs[i].value) k++;
        if (!made[k]) {
            const int glow = street_entry[k];
            for (int l = 0; l < LAMP_FLARE_LEVELS; l++)
                for (int d = 0; d < 256; d++) {
                    int rgb[3];
                    for (int c = 0; c < 3; c++) {
                        rgb[c] = palette[3 * d + c] + palette[3 * glow + c] * PERCENT[l] / 100;
                        if (rgb[c] > 255) rgb[c] = 255;
                    }
                    maps[k][l][d] = member[d] || d == window_entry || !usable[d] ? (uint8_t)d : (uint8_t)palette_nearest(palette, usable, rgb[0], rgb[1], rgb[2]);
                }
            made[k] = 1;
        }
        const uint8_t *const map[LAMP_FLARE_LEVELS] = { maps[k][0], maps[k][1], maps[k][2] };
        lamp_halo_draw(t->pixels, t->pitch, t->clip_left, t->clip_top, t->clip_right - 1, t->clip_bottom - 1, blobs[i].cx, blobs[i].cy,
                       lamp_flare_radius_scaled(blobs[i].area, STREET_HALO_REACH), map);
    }
}
