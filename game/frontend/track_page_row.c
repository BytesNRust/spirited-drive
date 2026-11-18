/* port (dep59 step 5, T7d): see track_page_row.h. */
#include "game/frontend/track_page_row.h"

#include "formats/fsh.h"
#include "formats/qfs.h"
#include "game/frontend/gallery_layout.h"
#include "game/frontend/track_page.h"

#include <stdlib.h>
#include <string.h>

enum { MASKED = 0xff };

void track_row_clear(TrackRow *row) { memset(row, 0, sizeof *row); }

/* the colour's number in the row's (1..), added when there is room, else the nearest kept */
static int row_colour(TrackRow *row, const uint8_t *c)
{
    int best = 0, best_d = 1 << 30;
    for (int k = 0; k < row->colours; k++) {
        const int dr = c[0] - row->rgb[k][0], dg = c[1] - row->rgb[k][1], db = c[2] - row->rgb[k][2], d = dr * dr + dg * dg + db * db;
        if (d == 0) return k + 1;
        if (d < best_d) best = k, best_d = d;
    }
    if (row->colours < TR_COLOURS) {
        memcpy(row->rgb[row->colours], c, 3);
        return ++row->colours;
    }
    return best + 1;
}

void track_row_put(TrackRow *row, int look, const uint8_t *pixels, int w, int h, int x0, int y0, const uint8_t pal[768])
{
    if (look < 0 || look >= TR_LOOKS) return;
    for (int y = 0; y < h; y++) {
        const int ry = y0 + y - TR_Y;
        if (ry < 0 || ry >= TR_H) continue;
        for (int x = 0; x < w; x++) {
            const int rx = x0 + x;
            const uint8_t texel = pixels[(size_t)y * w + x];
            if (rx < 0 || rx >= TR_W || texel == MASKED) continue;
            const uint8_t *c = pal + 3 * texel;
            row->face[look][ry * TR_W + rx] = gallery_row_is_face(c) ? (uint8_t)row_colour(row, c) : 0;
        }
    }
}

/* the bank's shape `tag` into the look, at its own place; 0 when it has none */
static int put_shape(TrackRow *row, int look, const Fsh *fsh, const char tag[4], const uint8_t pal[768])
{
    uint32_t index;
    FshEntry e;
    if (fsh_find(fsh, tag, &index) != FSH_OK || fsh_entry(fsh, index, &e) != FSH_OK || e.type != FSH_TYPE_BITMAP ||
        e.payload_size < (size_t)e.width * e.height)
        return 0;
    track_row_put(row, look, e.payload, e.width, e.height, e.misc[2], e.misc[3], pal);
    return 1;
}

int track_row_cut(TrackRow *row, const uint8_t *file, size_t size)
{
    track_row_clear(row);
    uint8_t *data = NULL;
    size_t data_size = size;
    if (qfs_is_compressed(file, size)) {
        if (qfs_decompressed_size(file, size, &data_size) != QFS_OK || (data = malloc(data_size)) == NULL ||
            qfs_decompress(file, size, data, data_size) != QFS_OK) {
            free(data);
            return 0;
        }
    }
    Fsh fsh;
    uint32_t index;
    FshEntry e;
    uint8_t pal[768];
    int ok = fsh_open(&fsh, data != NULL ? data : file, data_size) == FSH_OK && fsh_find(&fsh, "!pal", &index) == FSH_OK &&
             fsh_entry(&fsh, index, &e) == FSH_OK && fsh_palette_rgb(&e, pal) == FSH_OK;
    static const char *const LIT[TR_LOOKS - 1] = { "donl", "prel", "nexl", "besl" };
    for (int look = 0; ok && look < TR_LOOKS; look++) {   /* the background has them unlit, as the CD's page first shows them */
        ok = put_shape(row, look, &fsh, "bgnd", pal);
        if (ok && look > 0) ok = put_shape(row, look, &fsh, LIT[look - 1], pal);
    }
    free(data);
    if (!ok) {
        track_row_clear(row);
        return 0;
    }
    for (int look = 0; look < TR_LOOKS; look++)
        for (int i = 0; i < TR_H * TR_W; i++)
            if (row->face[look][i] != 0) row->count[row->face[look][i] - 1]++;
    return 1;
}

int track_row_look(int item)
{
    return item == TP_DONE ? TR_DONE : item == TP_PREVIOUS ? TR_PREVIOUS : item == TP_NEXT ? TR_NEXT : item == TP_BEST ? TR_BEST : TR_PLAIN;
}

void track_row_draw(const TrackRow *row, int look, uint8_t *to, int tw, int th, const uint8_t map[TR_COLOURS], int shadow)
{
    if (look < 0 || look >= TR_LOOKS) return;
    const uint8_t *face = row->face[look];
    for (int pass = 0; pass < 2; pass++)
        for (int y = 0; y < TR_H; y++)
            for (int x = 0; x < TR_W; x++) {
                const int k = face[y * TR_W + x], d = pass == 0 ? TR_SHADOW : 0, ty = TR_Y + y + d, tx = x + d;
                if (k == 0 || tx >= tw || ty >= th) continue;
                to[ty * tw + tx] = pass == 0 ? (uint8_t)shadow : map[k - 1];
            }
}

/* --- the words of the bank's art --- */

void track_words_clear(TrackWords *w) { memset(w, 0, sizeof *w); }

static int words_colour(TrackWords *w, const uint8_t *c)
{
    int best = 0, best_d = 1 << 30;
    for (int k = 0; k < w->colours; k++) {
        const int dr = c[0] - w->rgb[k][0], dg = c[1] - w->rgb[k][1], db = c[2] - w->rgb[k][2], d = dr * dr + dg * dg + db * db;
        if (d == 0) return k + 1;
        if (d < best_d) best = k, best_d = d;
    }
    if (w->colours < TR_COLOURS) {
        memcpy(w->rgb[w->colours], c, 3);
        return ++w->colours;
    }
    return best + 1;
}

void track_words_put(TrackWords *w, int layer, const uint8_t *pixels, int pw, int ph, int x0, int y0, const uint8_t pal[768], int last_row)
{
    if (layer < 0 || layer >= TW_LAYERS) return;
    for (int y = 0; y < ph; y++) {
        const int ry = y0 + y - TW_Y;
        if (ry < 0 || ry >= TW_H || y0 + y > last_row) continue;
        for (int x = 0; x < pw; x++) {
            const int rx = x0 + x - TW_X;
            const uint8_t texel = pixels[(size_t)y * pw + x];
            if (rx < 0 || rx >= TW_W || texel == MASKED) continue;
            const uint8_t *c = pal + 3 * texel;
            w->face[layer][ry * TW_W + rx] = gallery_row_is_face(c) ? (uint8_t)words_colour(w, c) : 0;
        }
    }
}

static int words_shape(TrackWords *w, int layer, const Fsh *fsh, const char tag[4], const uint8_t pal[768], int last_row)
{
    uint32_t index;
    FshEntry e;
    if (fsh_find(fsh, tag, &index) != FSH_OK || fsh_entry(fsh, index, &e) != FSH_OK || e.type != FSH_TYPE_BITMAP ||
        e.payload_size < (size_t)e.width * e.height)
        return 0;
    track_words_put(w, layer, e.payload, e.width, e.height, e.misc[2], e.misc[3], pal, last_row);
    return 1;
}

int track_words_cut(TrackWords *w, const uint8_t *file, size_t size)
{
    track_words_clear(w);
    uint8_t *data = NULL;
    size_t data_size = size;
    if (qfs_is_compressed(file, size)) {
        if (qfs_decompressed_size(file, size, &data_size) != QFS_OK || (data = malloc(data_size)) == NULL ||
            qfs_decompress(file, size, data, data_size) != QFS_OK) {
            free(data);
            return 0;
        }
    }
    Fsh fsh;
    uint32_t index;
    FshEntry e;
    uint8_t pal[768];
    int ok = fsh_open(&fsh, data != NULL ? data : file, data_size) == FSH_OK && fsh_find(&fsh, "!pal", &index) == FSH_OK &&
             fsh_entry(&fsh, index, &e) == FSH_OK && fsh_palette_rgb(&e, pal) == FSH_OK;
    static const struct { int layer; const char *tag; } SHAPES[] = {
        { TW_TIME_DIM, "timd" }, { TW_TIME_LIT, "timl" }, { TW_MORNING_DIM, "mord" }, { TW_MIDDAY_DIM, "midd" }, { TW_EVENING_DIM, "eved" },
        { TW_MORNING_LIT, "morl" }, { TW_MIDDAY_LIT, "midl" }, { TW_EVENING_LIT, "evel" } };
    ok = ok && words_shape(w, TW_LENGTH, &fsh, "bgnd", pal, TW_LENGTH_Y1);
    for (size_t i = 0; ok && i < sizeof SHAPES / sizeof SHAPES[0]; i++) ok = words_shape(w, SHAPES[i].layer, &fsh, SHAPES[i].tag, pal, TW_Y + TW_H - 1);
    free(data);
    if (!ok) track_words_clear(w);
    return ok;
}

void track_words_draw(const TrackWords *w, int layer, uint8_t *to, int tw, int th, const uint8_t map[TR_COLOURS], int shadow)
{
    if (layer < 0 || layer >= TW_LAYERS) return;
    for (int pass = 0; pass < 2; pass++)
        for (int y = 0; y < TW_H; y++)
            for (int x = 0; x < TW_W; x++) {
                const int k = w->face[layer][y * TW_W + x], d = pass == 0 ? TR_SHADOW : 0, ty = TW_Y + y + d, tx = TW_X + x + d;
                if (k == 0 || tx >= tw || ty >= th) continue;
                to[ty * tw + tx] = pass == 0 ? (uint8_t)shadow : map[k - 1];
            }
}

int track_words_time_layer(int light_time, int lit)
{
    if (light_time == 1) return lit ? TW_MORNING_LIT : TW_MORNING_DIM;   /* LIGHT_TIME_DAWN */
    if (light_time == 2) return lit ? TW_EVENING_LIT : TW_EVENING_DIM;   /* LIGHT_TIME_DUSK */
    if (light_time == 0) return lit ? TW_MIDDAY_LIT : TW_MIDDAY_DIM;     /* LIGHT_TIME_DAY */
    return -1;
}
