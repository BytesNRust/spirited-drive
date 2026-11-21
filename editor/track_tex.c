/* track_tex.h: the textures of a CD track. */
#include "track_tex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formats/chunk.h"
#include "game/race/track_art.h"

struct TrackTexSet {
    uint8_t *fam;      /* the file: the art's bitmaps point into it */
    TrackArt art;      /* big: why the set is on the heap */
    TrackTexInfo info[TRACK_TEX_MAX];
    const TrackTexture *tex[TRACK_TEX_MAX];
    int count;
};

static uint8_t *slurp(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    rewind(f);
    uint8_t *d = n > 0 ? malloc((size_t)n) : NULL;
    if (d != NULL && fread(d, 1, (size_t)n, f) != (size_t)n) free(d), d = NULL;
    fclose(f);
    *size = d != NULL ? (size_t)n : 0;
    return d;
}

static void fail(char *why, size_t cap, const char *what, const char *path)
{
    if (why != NULL && cap > 0) snprintf(why, cap, "%s: %s", what, path);
}

static void add(TrackTexSet *s, const char *kind, int n, const TrackTexture *t)
{
    if (!t->present || t->bitmap.width == 0 || t->bitmap.height == 0 || t->bitmap.payload == NULL || s->count >= TRACK_TEX_MAX) return;
    snprintf(s->info[s->count].name, TRACK_TEX_NAME, "%s_%02d", kind, n);
    s->info[s->count].width = t->bitmap.width, s->info[s->count].height = t->bitmap.height;
    s->tex[s->count++] = t;
}

TrackTexSet *track_tex_open(const char *cd, const char *track, char *why, size_t why_cap)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/SIMDATA/ETRACKFM/%s_001.FAM", cd, track);
    TrackTexSet *s = calloc(1, sizeof *s);
    size_t size = 0;
    Chunk root;
    if (s == NULL) return fail(why, why_cap, "out of memory", path), NULL;
    s->fam = slurp(path, &size);
    if (s->fam == NULL) return fail(why, why_cap, "cannot read the game's track file", path), free(s), NULL;
    if (chunk_open(&root, s->fam, size) != CHUNK_OK || !track_art_load(&s->art, &root) || !s->art.has_palette) {
        fail(why, why_cap, "the game's track file has no textures this reader knows", path);
        free(s->fam), free(s);
        return NULL;
    }
    for (int i = 0; i < TRACK_TEXTURE_SLOTS; ++i) add(s, "slot", i, &s->art.slots[i][0]);
    for (int i = 0; i < TRACK_SCENERY_TEXTURES; ++i) add(s, "scenery", i, &s->art.scenery[i]);
    return s;
}

void track_tex_close(TrackTexSet *set)
{
    if (set == NULL) return;
    free(set->fam);
    free(set);
}

int track_tex_count(const TrackTexSet *set) { return set != NULL ? set->count : 0; }

const TrackTexInfo *track_tex_info(const TrackTexSet *set, int index)
{
    return set != NULL && index >= 0 && index < set->count ? &set->info[index] : NULL;
}

int track_tex_rgba(const TrackTexSet *set, const char *name, int *width, int *height, uint8_t **rgba)
{
    for (int i = 0; set != NULL && i < set->count; ++i) {
        if (strcmp(set->info[i].name, name) != 0) continue;
        const TrackTexture *t = set->tex[i];
        const size_t n = (size_t)t->bitmap.width * t->bitmap.height;
        uint8_t *out = malloc(n * 4);
        if (out == NULL) return 0;
        for (size_t k = 0; k < n; ++k) {
            const uint8_t v = t->bitmap.payload[k];
            memcpy(out + 4 * k, set->art.palette + 3 * v, 3);
            out[4 * k + 3] = v == 255 ? 0 : 255;
        }
        *width = t->bitmap.width, *height = t->bitmap.height, *rgba = out;
        return 1;
    }
    return 0;
}
