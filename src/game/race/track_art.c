#include "game/race/track_art.h"

#include <stdio.h>
#include <string.h>

/* 0044c1fc: exact name, upper case first, then lower case. */
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
    return fsh_entry(fsh, index, out) == FSH_OK && out->type == FSH_TYPE_BITMAP
        && (size_t)out->width * out->height <= out->payload_size;
}

/* 0044c290: any 0xFF pixel makes the bitmap transparent. */
static int has_transparency(const FshEntry *e)
{
    return memchr(e->payload, 0xFF, (size_t)e->width * e->height) != NULL;
}

/* 0044c318: variants '0'..'7' in the last character; a missing one reuses the
 * previous, except the first. Slots already filled are kept. */
static void fill_slot(TrackTexture slot[TRACK_TEXTURE_VARIANTS], const Fsh *fsh, const char base[4])
{
    if (slot[0].present) return;
    char name[4];
    memcpy(name, base, 4);
    for (int v = 0; v < TRACK_TEXTURE_VARIANTS; v++, name[3]++) {
        TrackTexture t = { 0 };
        if (find_bitmap(fsh, name, &t.bitmap)) {
            t.present = 1;
            t.transparent = has_transparency(&t.bitmap);
        } else if (v > 0) {
            t = slot[v - 1];
        }
        slot[v] = t;
    }
}

static int open_shpi(const Chunk *chunk, Fsh *fsh)
{
    return chunk->kind == CHUNK_KIND_BITMAP && chunk_bitmaps(chunk, fsh) == FSH_OK;
}

static void load_palette(TrackArt *art, const Fsh *fsh)
{
    uint32_t index;
    FshEntry pal;
    art->has_palette = fsh_find(fsh, "!pal", &index) == FSH_OK && fsh_entry(fsh, index, &pal) == FSH_OK
        && fsh_palette_rgb(&pal, art->palette) == FSH_OK;
}

static void load_circuit(TrackArt *art, const Fsh *fsh)
{
    for (int id = 0; id < TRACK_TEXTURE_IDS; id++) {
        for (int letter = 0; letter < 3; letter++) {
            char name[5];
            snprintf(name, sizeof name, "%02d%c0", id, 'A' + letter);
            fill_slot(art->slots[id * 3 + letter], fsh, name);
        }
    }
    /* TODO(0044c36c): the "ga%d0" bitmaps fill 3 extra rows of slots from the
     * TRI header field at +0x28 / 3; animated water/lights on circuits. */
}

static void load_stage(TrackArt *art, const Chunk *terrain)
{
    for (uint32_t n = 0; n < terrain->count && n < TRACK_TEXTURE_IDS; n++) {
        Chunk child;
        Fsh fsh;
        if (chunk_child(terrain, n, &child) != CHUNK_OK || !open_shpi(&child, &fsh)) continue;
        for (int letter = 0; letter < 3; letter++) {
            const char name[4] = { (char)('A' + letter), '0', '0', '0' };
            fill_slot(art->slots[n * 3 + letter], &fsh, name);
        }
        if (n == 0) load_palette(art, &fsh);
    }
}

static void set_scenery(TrackTexture *t, const Fsh *fsh, const char name[4])
{
    uint32_t index;
    if (fsh_find(fsh, name, &index) != FSH_OK || fsh_entry(fsh, index, &t->bitmap) != FSH_OK
        || t->bitmap.type != FSH_TYPE_BITMAP || (size_t)t->bitmap.width * t->bitmap.height > t->bitmap.payload_size)
        return;
    t->present = 1;
    t->transparent = has_transparency(&t->bitmap);
}

/* 0044c36c, scenery part. */
static void load_scenery(TrackArt *art, const Chunk *root)
{
    Chunk scenery, child;
    Fsh fsh;
    if (chunk_child(root, FAM_CHILD_SCENERY, &scenery) != CHUNK_OK || scenery.kind != CHUNK_KIND_HEADER) return;
    if (scenery.count == 1) {
        if (chunk_child(&scenery, 0, &child) != CHUNK_OK || !open_shpi(&child, &fsh)) return;
        for (int id = 0; id < TRACK_SCENERY_TEXTURES; id++) {
            char name[5];
            snprintf(name, sizeof name, "%02d00", id);
            set_scenery(&art->scenery[id], &fsh, name);
        }
        return;
    }
    for (uint32_t n = 0; n < scenery.count && n < TRACK_SCENERY_TEXTURES; n++) {
        if (chunk_child(&scenery, n, &child) == CHUNK_OK && open_shpi(&child, &fsh))
            set_scenery(&art->scenery[n], &fsh, "0000");
    }
}

int track_art_load(TrackArt *art, const Chunk *root)
{
    memset(art, 0, sizeof *art);
    Chunk terrain, horizon, first;
    Fsh fsh;
    if (chunk_child(root, FAM_CHILD_TERRAIN, &terrain) != CHUNK_OK || terrain.kind != CHUNK_KIND_HEADER) return 0;
    if (terrain.count == 1) {
        if (chunk_child(&terrain, 0, &first) != CHUNK_OK || !open_shpi(&first, &fsh)) return 0;
        load_palette(art, &fsh);
        load_circuit(art, &fsh);
    } else {
        load_stage(art, &terrain);
    }
    if (chunk_child(root, FAM_CHILD_HORIZON, &horizon) == CHUNK_OK && open_shpi(&horizon, &fsh)) {
        uint32_t index;
        art->has_horizon = fsh_find(&fsh, "horz", &index) == FSH_OK && fsh_entry(&fsh, index, &art->horizon) == FSH_OK;
    }
    load_scenery(art, root);
    /* TODO(0044c36c): the palette setup of 004471dc / 00446f90 / 00446f58. */
    return 1;
}
