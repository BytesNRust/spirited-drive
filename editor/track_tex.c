/* track_tex.h: the textures of a CD track. */
#include "track_tex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formats/chunk.h"
#include "formats/fsh.h"

/* The loader of game/race/track_art.c for variant 0 of each slot and for the scenery boards, on the core's chunk and fsh readers alone (the Studio shares only the core with the game);
 * tests/test_editor_track_tex.c compares it with the game's own loader on all sixteen tracks. */
enum { IDS = 48, SCENERY = 64 };

struct TrackTexSet {
    uint8_t *fam;      /* the file: the bitmaps point into it */
    uint8_t palette[768];
    TrackTexInfo info[TRACK_TEX_MAX];
    FshEntry entry[TRACK_TEX_MAX];
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

/* the bitmap `name` (upper case first, then lower: 0044c1fc), as the game accepts it */
static int find_bitmap(const Fsh *fsh, const char *name, FshEntry *out)
{
    char upper[4], lower[4];
    for (int i = 0; i < 4; i++) {
        const char c = name[i];
        upper[i] = c >= 'a' && c <= 'z' ? (char)(c - 32) : c;
        lower[i] = c >= 'A' && c <= 'Z' ? (char)(c + 32) : c;
    }
    uint32_t index;
    if (fsh_find(fsh, upper, &index) != FSH_OK && fsh_find(fsh, lower, &index) != FSH_OK) return 0;
    return fsh_entry(fsh, index, out) == FSH_OK && out->type == FSH_TYPE_BITMAP && (size_t)out->width * out->height <= out->payload_size;
}

static void add(TrackTexSet *s, const char *kind, int n, const FshEntry *e)
{
    if (e->width == 0 || e->height == 0 || e->payload == NULL || s->count >= TRACK_TEX_MAX) return;
    snprintf(s->info[s->count].name, TRACK_TEX_NAME, "%s_%02d", kind, n);
    s->info[s->count].width = e->width, s->info[s->count].height = e->height;
    s->entry[s->count++] = *e;
}

static int open_shpi(const Chunk *chunk, Fsh *fsh)
{
    return chunk->kind == CHUNK_KIND_BITMAP && chunk_bitmaps(chunk, fsh) == FSH_OK;
}

static int load_palette(TrackTexSet *s, const Fsh *fsh)
{
    uint32_t index;
    FshEntry pal;
    return fsh_find(fsh, "!pal", &index) == FSH_OK && fsh_entry(fsh, index, &pal) == FSH_OK && fsh_palette_rgb(&pal, s->palette) == FSH_OK;
}

static int load_slots(TrackTexSet *s, const Chunk *terrain)
{
    Chunk first;
    Fsh fsh;
    FshEntry e;
    if (terrain->count == 1) {   /* a circuit: one SHPI, slot id * 3 + letter is "%02d%c0" */
        if (chunk_child(terrain, 0, &first) != CHUNK_OK || !open_shpi(&first, &fsh) || !load_palette(s, &fsh)) return 0;
        for (int id = 0; id < IDS; id++)
            for (int letter = 0; letter < 3; letter++) {
                char name[5];
                snprintf(name, sizeof name, "%02d%c0", id, 'A' + letter);
                if (find_bitmap(&fsh, name, &e)) add(s, "slot", id * 3 + letter, &e);
            }
        return 1;
    }
    int palette = 0;   /* a stage: one SHPI per id, the slots "A000", "B000", "C000" of it; the palette is the first one's */
    for (uint32_t n = 0; n < terrain->count && n < IDS; n++) {
        Chunk child;
        if (chunk_child(terrain, n, &child) != CHUNK_OK || !open_shpi(&child, &fsh)) continue;
        for (int letter = 0; letter < 3; letter++) {
            const char name[4] = { (char)('A' + letter), '0', '0', '0' };
            if (find_bitmap(&fsh, name, &e)) add(s, "slot", (int)n * 3 + letter, &e);
        }
        if (n == 0) palette = load_palette(s, &fsh);
    }
    return palette;
}

static void board(TrackTexSet *s, const Fsh *fsh, int n, const char name[4])
{
    uint32_t index;
    FshEntry e;
    if (fsh_find(fsh, name, &index) == FSH_OK && fsh_entry(fsh, index, &e) == FSH_OK && e.type == FSH_TYPE_BITMAP && (size_t)e.width * e.height <= e.payload_size) add(s, "scenery", n, &e);
}

static void load_scenery(TrackTexSet *s, const Chunk *root)
{
    Chunk scenery, child;
    Fsh fsh;
    if (chunk_child(root, FAM_CHILD_SCENERY, &scenery) != CHUNK_OK || scenery.kind != CHUNK_KIND_HEADER) return;
    if (scenery.count == 1) {   /* a circuit: one SHPI, board id is "%02d00" */
        if (chunk_child(&scenery, 0, &child) != CHUNK_OK || !open_shpi(&child, &fsh)) return;
        for (int id = 0; id < SCENERY; id++) {
            char name[5];
            snprintf(name, sizeof name, "%02d00", id);
            board(s, &fsh, id, name);
        }
        return;
    }
    for (uint32_t n = 0; n < scenery.count && n < SCENERY; n++)   /* a stage: entry "0000" of the n-th SHPI */
        if (chunk_child(&scenery, n, &child) == CHUNK_OK && open_shpi(&child, &fsh)) board(s, &fsh, (int)n, "0000");
}

TrackTexSet *track_tex_open(const char *cd, const char *track, char *why, size_t why_cap)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/SIMDATA/ETRACKFM/%s_001.FAM", cd, track);
    TrackTexSet *s = calloc(1, sizeof *s);
    size_t size = 0;
    Chunk root, terrain;
    if (s == NULL) return fail(why, why_cap, "out of memory", path), NULL;
    s->fam = slurp(path, &size);
    if (s->fam == NULL) return fail(why, why_cap, "cannot read the game's track file", path), free(s), NULL;
    if (chunk_open(&root, s->fam, size) != CHUNK_OK || chunk_child(&root, FAM_CHILD_TERRAIN, &terrain) != CHUNK_OK || terrain.kind != CHUNK_KIND_HEADER || !load_slots(s, &terrain)) {
        fail(why, why_cap, "the game's track file has no textures this reader knows", path);
        free(s->fam), free(s);
        return NULL;
    }
    load_scenery(s, &root);
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
        const FshEntry *e = &set->entry[i];
        const size_t n = (size_t)e->width * e->height;
        uint8_t *out = malloc(n * 4);
        if (out == NULL) return 0;
        for (size_t k = 0; k < n; ++k) {
            const uint8_t v = e->payload[k];
            memcpy(out + 4 * k, set->palette + 3 * v, 3);
            out[4 * k + 3] = v == 255 ? 0 : 255;
        }
        *width = e->width, *height = e->height, *rgba = out;
        return 1;
    }
    return 0;
}
