#include "game/race/track_mask.h"
#include "game/sys/data_path.h"
#include "game/sys/port_log.h"

#include "formats/png.h"
#include "game/platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_SIDE = 4096, REFLECTIVE_FROM = 128, MAX_MASKS = TRACK_TEXTURE_SLOTS + TRACK_SCENERY_TEXTURES };

typedef struct {
    const TrackTexture *texture;
    uint8_t *mask;
} Entry;

static Entry masks[MAX_MASKS];
static int mask_count;

int track_mask_parse(const uint8_t *png, size_t len, int width, int height, uint8_t *out)
{
    if (png == NULL || out == NULL || width <= 0 || height <= 0 || width > MAX_SIDE || height > MAX_SIDE) return 0;
    PngImage img;
    if (png_read(png, len, &img) != PNG_OK) return 0;
    for (int y = 0; y < height; y++) {
        const size_t sy = (size_t)y * img.height / (size_t)height;
        for (int x = 0; x < width; x++) {
            const size_t sx = (size_t)x * img.width / (size_t)width, at = sy * img.width + sx;
            const uint8_t *c = img.indexed ? img.palette + 3 * img.pixels[at] : img.pixels + 4 * at;
            const int opaque = img.indexed || c[3] != 0;
            out[(size_t)y * (size_t)width + (size_t)x] = (uint8_t)(opaque && c[0] >= REFLECTIVE_FROM);
        }
    }
    png_free(img.pixels);
    return 1;
}

uint8_t *track_mask_load(const char *track, const char *name, int width, int height)
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
        size_t size = 0;
        uint8_t *file = platform_read_file(path, &size);
        if (file == NULL) continue;
        uint8_t *mask = malloc((size_t)width * (size_t)height);
        const int ok = mask != NULL && track_mask_parse(file, size, width, height, mask);
        platform_free_file(file);
        if (ok) {
            port_log("track mask: %s\n", path);   /* the log says where a mask came from */
            return mask;
        }
        free(mask);
    }
    return NULL;
}

static void keep(const char *track, const char *name, const TrackTexture *t)
{
    if (!t->present || t->bitmap.width <= 0 || t->bitmap.height <= 0 || mask_count >= MAX_MASKS) return;
    uint8_t *mask = track_mask_load(track, name, t->bitmap.width, t->bitmap.height);
    if (mask != NULL) masks[mask_count++] = (Entry){ t, mask };
}

int track_masks_set(const char *track, const TrackArt *art)
{
    for (int i = 0; i < mask_count; i++) free(masks[i].mask);
    mask_count = 0;
    if (track == NULL || art == NULL) return 0;
    char name[32];
    for (int s = 0; s < TRACK_TEXTURE_SLOTS; s++) {
        snprintf(name, sizeof name, "slot_%02d", s);
        keep(track, name, &art->slots[s][0]);
    }
    for (int s = 0; s < TRACK_SCENERY_TEXTURES; s++) {
        snprintf(name, sizeof name, "scenery_%02d", s);
        keep(track, name, &art->scenery[s]);
    }
    return mask_count;
}

const uint8_t *track_mask_for(const TrackTexture *t)
{
    for (int i = 0; t != NULL && i < mask_count; i++)
        if (masks[i].texture == t) return masks[i].mask;
    return NULL;
}
