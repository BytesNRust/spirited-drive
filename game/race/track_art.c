#include "game/race/track_art.h"

#include <stdio.h>
#include <stdlib.h>
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

/* variants '0'..'7' in the last character; a missing one reuses the previous,
 * except the first */
static void fill_variants(TrackTexture slot[TRACK_TEXTURE_VARIANTS], const Fsh *fsh, const char base[4])
{
    char name[4];
    memcpy(name, base, 4);
    for (int v = 0; v < TRACK_TEXTURE_VARIANTS; v++, name[3]++) {
        TrackTexture t = { 0 };
        if (find_bitmap(fsh, name, &t.bitmap)) {
            t.present = 1;
            t.transparent = has_transparency(&t.bitmap);
            if (t.transparent) t.bitmap.type = TRACK_TEXTURE_TRANSPARENT_TYPE;
        } else if (v > 0) {
            t = slot[v - 1];
        }
        slot[v] = t;
    }
}

/* 0044c318: fill_variants; slots already filled are kept. */
static void fill_slot(TrackTexture slot[TRACK_TEXTURE_VARIANTS], const Fsh *fsh, const char base[4])
{
    if (!slot[0].present) fill_variants(slot, fsh, base);
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
    if (t->transparent) t->bitmap.type = TRACK_TEXTURE_TRANSPARENT_TYPE;
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

/* the palette index a texel of index `i` is painted with: a green one becomes the snow-white grey (the lightest, a little lighter the brighter the green was), others stay */
static void snow_road_table(const uint8_t *pal, uint8_t lut[256])
{
    int luma[256], grey[256];
    for (int i = 0; i < 256; i++) {
        const int r = pal[i * 3], g = pal[i * 3 + 1], b = pal[i * 3 + 2];
        const int mx = r > g ? (r > b ? r : b) : (g > b ? g : b), mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
        luma[i] = (r * 77 + g * 150 + b * 29) >> 8;
        grey[i] = i != 0xFF && mx - mn <= 10 && luma[i] > 0;   /* only greys, and not the black the unused entries are */
    }
    for (int i = 0; i < 256; i++) {
        const int r = pal[i * 3], g = pal[i * 3 + 1], b = pal[i * 3 + 2];
        lut[i] = (uint8_t)i;
        if (i == 0xFF || g < r + 6 || g < b + 6) continue;   /* the transparent index and what is not green */
        const int want = 215 + (luma[i] >> 3);
        int best = -1, best_d = 1 << 30;
        for (int k = 0; k < 256; k++) {
            const int d = abs(luma[k] - want);
            if (grey[k] && d < best_d) best_d = d, best = k;
        }
        if (best >= 0) lut[i] = (uint8_t)best;
    }
}

int track_art_remap_slots(TrackArt *art, const uint8_t map[TRACK_TEXTURE_SLOTS])
{
    int taken = 0;
    if (art == NULL || map == NULL) return 0;
    TrackTexture *const before = malloc(sizeof art->slots);   /* every `to` and `from` is read as it was, so a chain of pairs does not depend on their order */
    if (before == NULL) return 0;
    memcpy(before, art->slots, sizeof art->slots);
    uint8_t lut[256];
    if (art->has_palette) snow_road_table(art->palette, lut);
    for (int from = 0; from < TRACK_TEXTURE_SLOTS; from++) {
        const int to = (int)map[from] - 1;
        if (to < 0 || to >= TRACK_TEXTURE_SLOTS || to == from || !before[to * TRACK_TEXTURE_VARIANTS].present) continue;
        for (int v = 0; v < TRACK_TEXTURE_VARIANTS; v++) {
            const TrackTexture *own = &before[from * TRACK_TEXTURE_VARIANTS + v], *snow = &before[to * TRACK_TEXTURE_VARIANTS + v];
            TrackTexture t = *snow;
            const size_t sw = snow->bitmap.width, sh = snow->bitmap.height;
            const size_t ow = own->bitmap.width, oh = own->bitmap.height;
            /* the snow is laid on the road's own size, texel for texel and repeated: the road's polygons map their whole bitmap, so the snow slot's 80 x 80 (far 80 x 40) on a road of 128 x 256 (far 32 x 96) made the texels two to three times coarser and of another shape, and the grain changed from one distance to the next */
            if (art->has_palette && own->present && snow->present && own->bitmap.payload != snow->bitmap.payload && sw > 0 && sh > 0 && ow > 0 && oh > 0
                && ow * oh <= own->bitmap.payload_size && sw * sh <= snow->bitmap.payload_size) {
                uint8_t *dst = (uint8_t *)(uintptr_t)own->bitmap.payload;   /* the track's bank: a file the loader owns and the palette remap rewrites in place */
                for (size_t y = 0; y < oh; y++)
                    for (size_t x = 0; x < ow; x++) dst[y * ow + x] = lut[snow->bitmap.payload[(y % sh) * sw + x % sw]];
                t.bitmap.payload = dst, t.bitmap.payload_size = own->bitmap.payload_size;
                t.bitmap.width = own->bitmap.width, t.bitmap.height = own->bitmap.height;
            }
            art->slots[from][v] = t;
        }
        taken++;
    }
    free(before);
    return taken;
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
    /* port: 0044c36c's palette setup (004471dc / 00446f90 / 00446f58 on the '!pal' made 6-bit) is the race's,
     * drive_palette.c */
    return 1;
}

void track_art_load_animated(TrackArt *art, const Chunk *root, const uint8_t *tri_header)
{
    enum { TRI_ANIMATED_AT = 0x28 };
    Chunk terrain, first;
    Fsh fsh;
    if (chunk_child(root, FAM_CHILD_TERRAIN, &terrain) != CHUNK_OK || terrain.kind != CHUNK_KIND_HEADER
        || terrain.count != 1 || chunk_child(&terrain, 0, &first) != CHUNK_OK || !open_shpi(&first, &fsh))
        return;
    const int16_t field = (int16_t)(uint16_t)(tri_header[TRI_ANIMATED_AT] | tri_header[TRI_ANIMATED_AT + 1] << 8);
    const int id = field / 3;
    for (int k = 0; k < 3; k++) {
        const int slot = id * 3 + k;
        char name[5];
        snprintf(name, sizeof name, "ga%d0", k);
        FshEntry e;
        /* port guard: the original writes past 0050eec4's 0x90 slots for a field of 0x90 or more */
        if (slot < 0 || slot >= TRACK_TEXTURE_SLOTS || !find_bitmap(&fsh, name, &e)) continue;
        fill_variants(art->slots[slot], &fsh, name);
    }
}
