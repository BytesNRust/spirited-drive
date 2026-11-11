#include "game/race/track_mask.h"
#include "game/race/tunnel_lamp_textures.h"
#include "game/sys/data_path.h"
#include "game/sys/port_log.h"

#include "formats/png.h"
#include "game/platform/platform.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_SIDE = 4096, REFLECTIVE_FROM = 128, LAMP_RED_FROM = 128, LAMP_GREEN_FROM = 128, LAMP_BLUE_BELOW = 128, MAX_MASKS = TRACK_TEXTURE_SLOTS + TRACK_SCENERY_TEXTURES, MAX_MARKED = 16 };

typedef struct {
    const TrackTexture *texture;
    uint8_t *mask;
} Entry;

static Entry masks[MAX_MASKS];
static int mask_count;

/* a texel's marks: `reflective` (out) red 128 or more, `lamp` (out, NULL when not asked) the yellow of a lamp (red and green 128 or more, blue below 128), which is not reflective */
static int parse_marks(const uint8_t *png, size_t len, int width, int height, uint8_t *reflective, uint8_t *lamp)
{
    if (png == NULL || reflective == NULL || width <= 0 || height <= 0 || width > MAX_SIDE || height > MAX_SIDE) return 0;
    PngImage img;
    if (png_read(png, len, &img) != PNG_OK) return 0;
    for (int y = 0; y < height; y++) {
        const size_t sy = (size_t)y * img.height / (size_t)height;
        for (int x = 0; x < width; x++) {
            const size_t sx = (size_t)x * img.width / (size_t)width, at = sy * img.width + sx, out = (size_t)y * (size_t)width + (size_t)x;
            const uint8_t *c = img.indexed ? img.palette + 3 * img.pixels[at] : img.pixels + 4 * at;
            const int opaque = img.indexed || c[3] != 0;
            const int yellow = opaque && c[0] >= LAMP_RED_FROM && c[1] >= LAMP_GREEN_FROM && c[2] < LAMP_BLUE_BELOW;
            reflective[out] = (uint8_t)(opaque && c[0] >= REFLECTIVE_FROM && !yellow);
            if (lamp != NULL) lamp[out] = (uint8_t)yellow;
        }
    }
    png_free(img.pixels);
    return 1;
}

int track_mask_parse(const uint8_t *png, size_t len, int width, int height, uint8_t *out) { return parse_marks(png, len, width, height, out, NULL); }

/* the mask file's reflective mask, and with `lamp` not NULL its yellow marks (a malloc'd width x height of 0 / 1, set when a file is found) */
static uint8_t *load_marks(const char *track, const char *name, int width, int height, uint8_t **lamp)
{
    if (track == NULL || name == NULL || width <= 0 || height <= 0) return NULL;
    const char *env = getenv("TNFS_TRACK_MASKS");
    char file_name[256], found[DATA_PATH_CANDIDATES][DATA_PATH_MAX];
    snprintf(file_name, sizeof file_name, "track_masks/%s/%s.png", track, name);
    const int dev = env != NULL && env[0] != '\0';   /* a dev override first: TNFS_TRACK_MASKS=<dir of <TRACK>/<file>.png> */
    (void)data_path_candidates(file_name, found);     /* then a mod's, the program's own data, the working directory, a development build's source tree */
    for (int r = 0; r < 1 + DATA_PATH_CANDIDATES; r++) {
        char path[DATA_PATH_MAX];
        if (r == 0) {
            if (!dev) continue;
            snprintf(path, sizeof path, "%s/%s/%s.png", env, track, name);
        } else {
            if (found[r - 1][0] == '\0') continue;
            snprintf(path, sizeof path, "%s", found[r - 1]);
        }
        if (r > 0 && !data_path_exists(path)) continue;   /* masks are optional: a data folder without this one is passed over, no "cannot read" line (i191); the dev override is read as named */
        size_t size = 0;
        uint8_t *file = platform_read_file(path, &size);
        if (file == NULL) continue;
        uint8_t *mask = malloc((size_t)width * (size_t)height), *marks = lamp != NULL ? malloc((size_t)width * (size_t)height) : NULL;
        const int ok = mask != NULL && (lamp == NULL || marks != NULL) && parse_marks(file, size, width, height, mask, marks);
        platform_free_file(file);
        if (ok) {
            if (r <= 1) port_log("track mask: %s\n", path);   /* a dev override's or a mod's: what a bug report needs */
            else port_log_detail("track mask: %s\n", path);   /* the port's own data: a detail */
            if (lamp != NULL) *lamp = marks;
            return mask;
        }
        port_log("track mask: cannot read %s: not a %dx%d mask PNG\n", path, width, height);   /* there, but not a mask: said, then the next folder */
        free(mask);
        free(marks);
    }
    return NULL;
}

uint8_t *track_mask_load(const char *track, const char *name, int width, int height) { return load_marks(track, name, width, height, NULL); }

/* port (the lamps' marks): the lamps the yellow of the last track's slot masks name, one for each slot that has any */
static TunnelLampTexture marked[MAX_MARKED];
static uint8_t marked_set[MAX_MARKED][256];   /* the colours under each lamp's yellow (TunnelLampTexture.set) */
static int marked_count;
static char marked_track[16];

static const TrackTexture *marked_texture[MAX_MARKED];   /* the texture each lamp was marked on, and its yellow (width x height bytes, owned): track_lamp_marks_refresh reads it again */
static uint8_t *marked_yellow[MAX_MARKED];

static void drop_marks(void)
{
    for (int i = 0; i < marked_count; i++) free(marked_yellow[i]), marked_yellow[i] = NULL, marked_texture[i] = NULL;
    marked_count = 0;
    marked_track[0] = '\0';
}

/* lamp `i`'s rectangle and colours from its texture's texels as they are now: the bounding box of the yellow, the palette entries under it (all of them are `set`; idx has the
 * most used, at most TUNNEL_LAMP_TEXTURE_COLOURS: the transparent and shadow entries are none). 0 when the yellow covers no texel. */
static int read_marked(int i)
{
    const TrackTexture *const t = marked_texture[i];
    const uint8_t *const lamp = marked_yellow[i];
    const int w = t->bitmap.width, h = t->bitmap.height;
    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    unsigned count[256] = { 0 };
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            if (!lamp[y * w + x]) continue;
            x0 = x < x0 ? x : x0, y0 = y < y0 ? y : y0, x1 = x > x1 ? x : x1, y1 = y > y1 ? y : y1;
            const uint8_t v = t->bitmap.payload[y * w + x];
            if (v < 0xFE) count[v]++;
        }
    if (x1 < 0) return 0;
    uint8_t *const set = marked_set[i];
    for (int c = 0; c < 256; c++) set[c] = count[c] > 0;   /* every colour the yellow shows stays bright (the table of before listed them and the ones only its rectangle showed) */
    TunnelLampTexture *const l = &marked[i];
    *l = (TunnelLampTexture){ .slot = l->slot, .rect = { x0, y0, x1, y1 }, .set = set };
    while (l->count < TUNNEL_LAMP_TEXTURE_COLOURS) {
        int best = -1;
        for (int c = 0; c < 256; c++)
            if (count[c] > 0 && (best < 0 || count[c] > count[best])) best = c;
        if (best < 0) break;
        l->idx[l->count++] = (uint8_t)best;
        count[best] = 0;
    }
    return 1;
}

/* the lamp of road texture slot `slot`: kept with its yellow (`lamp`, taken) and read (read_marked) */
static void mark_lamp(int slot, const TrackTexture *t, uint8_t *lamp)
{
    if (marked_count >= MAX_MARKED || t->bitmap.payload == NULL) {
        free(lamp);
        return;
    }
    const int i = marked_count;
    marked[i].slot = slot, marked_texture[i] = t, marked_yellow[i] = lamp;
    if (!read_marked(i)) {
        free(lamp);
        marked_yellow[i] = NULL, marked_texture[i] = NULL;
        return;
    }
    marked_count++;
    const TunnelLampTexture *const l = &marked[i];
    port_log_detail("lamp marks: slot %d, %d colours, rect %d,%d to %d,%d\n", slot, l->count, l->rect[0], l->rect[1], l->rect[2], l->rect[3]);
}

void track_lamp_marks_refresh(void)
{
    for (int i = 0; i < marked_count; i++)
        if (marked_texture[i]->bitmap.payload != NULL && read_marked(i))
            port_log_detail("lamp marks: slot %d read again (the race's palette): %d colours, first %d\n", marked[i].slot, marked[i].count, marked[i].idx[0]);
}

/* `slot` >= 0: a road texture slot (its yellow makes a lamp); `reflective` 0 keeps no reflective mask (a rally's road) */
static void keep(const char *track, const char *name, const TrackTexture *t, int slot, int reflective)
{
    if (!t->present || t->bitmap.width <= 0 || t->bitmap.height <= 0) return;
    uint8_t *lamp = NULL;
    uint8_t *mask = load_marks(track, name, t->bitmap.width, t->bitmap.height, slot >= 0 ? &lamp : NULL);
    if (mask != NULL && reflective && mask_count < MAX_MASKS) masks[mask_count++] = (Entry){ t, mask };
    else free(mask);
    if (lamp != NULL) mark_lamp(slot, t, lamp);   /* it keeps the yellow */
}

int track_masks_set_race(const char *track, const TrackArt *art, int rally)
{
    for (int i = 0; i < mask_count; i++) free(masks[i].mask);
    mask_count = 0;
    drop_marks();
    if (track == NULL || art == NULL) return 0;
    snprintf(marked_track, sizeof marked_track, "%s", track);
    char name[32];
    for (int s = 0; s < TRACK_TEXTURE_SLOTS; s++) {   /* a rally's road has no cat's eyes (no reflective mask), but its lamps are the track's all the same */
        snprintf(name, sizeof name, "slot_%02d", s);
        keep(track, name, &art->slots[s][0], s, !rally);
    }
    for (int s = 0; s < TRACK_SCENERY_TEXTURES; s++) {
        snprintf(name, sizeof name, "scenery_%02d", s);
        keep(track, name, &art->scenery[s], -1, 1);
    }
    return mask_count;
}

int track_masks_set(const char *track, const TrackArt *art) { return track_masks_set_race(track, art, 0); }

const uint8_t *track_mask_for(const TrackTexture *t)
{
    for (int i = 0; t != NULL && i < mask_count; i++)
        if (masks[i].texture == t) return masks[i].mask;
    return NULL;
}

static int same_track(const char *a, const char *b)
{
    for (; *a != '\0' && *b != '\0'; a++, b++)
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) return 0;
    return *a == *b;
}

int track_lamp_marks(const char *track, const TunnelLampTexture **out)
{
    if (track == NULL || marked_count == 0 || !same_track(track, marked_track)) return 0;
    if (out != NULL) *out = marked;
    return marked_count;
}

int track_lamps_for(const char *track, const TunnelLampTexture **out)
{
    const int n = track_lamp_marks(track, out);
    return n > 0 ? n : tunnel_lamp_textures_for(track, out);
}
