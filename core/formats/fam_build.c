/* See fam_build.h. The variants' sizes are the CD's (AL1's terrain), the file's layout is the one chunk.h reads, and each bank goes
 * through shpi_build, so its directory and entries are the ones the CD's banks have. */
#include "formats/fam_build.h"
#include "formats/fam_spec.h"

#include "formats/chunk.h"
#include "formats/limits.h"
#include "formats/shpi_build.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { FAM_CHILDREN = 4, TABLE_HEAD = 8, TABLE_ENTRY = 4, CHUNK_ALIGN = 4, BITMAP_TYPE = 0x7b, PALETTE_TYPE = 0x22, BANK_PALETTE = 0 };

/* width and height of variants 000..007: the CD's AL1, measured with tnfs-studio unpack (A004 is the one not in a plain halving) */
static const uint16_t LADDER[FAM_VARIANTS][2] = {
    { 128, 256 }, { 64, 128 }, { 32, 64 }, { 16, 32 }, { 32, 96 }, { 16, 48 }, { 8, 24 }, { 4, 12 }
};

static const char LETTERS[FAM_LETTERS] = { 'A', 'B', 'C' };

/* one resampled texture: its variants' pictures and their palette indices */
typedef struct {
    uint32_t width[FAM_VARIANTS], height[FAM_VARIANTS];
    uint8_t *rgba[FAM_VARIANTS];
    uint8_t *index[FAM_VARIANTS];
    unsigned letters;   /* FAM_LETTER_ bits of the letters the bank holds */
} Terrain;

/* a chunk of a file being made: malloc'd bytes */
typedef struct {
    uint8_t *data;
    size_t len;
} Blob;

static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24); }
static size_t padded(size_t n) { return (n + CHUNK_ALIGN - 1) & ~(size_t)(CHUNK_ALIGN - 1); }

static void blob_free(Blob *b)
{
    free(b->data);
    b->data = NULL;
    b->len = 0;
}

static void terrain_free(Terrain *t, size_t n)
{
    for (size_t i = 0; i < n; i++)
        for (int v = 0; v < FAM_VARIANTS; v++) free(t[i].rgba[v]), free(t[i].index[v]);
    free(t);
}

/* ---- the limits (formats/limits.h) */

static size_t limit_of(const char *id) { return (size_t)limit_find(id)->max; }

/* a picture's sides go into the 16-bit fields of its entry: 1 to SHPI_MAX_SIDE */
static int picture_fits(const FamPicture *p, const char *what, ExchangeError *err)
{
    if (p->width >= 1 && p->width <= SHPI_MAX_SIDE && p->height >= 1 && p->height <= SHPI_MAX_SIDE) return 1;
    exchange_fail(err, "%s is %u x %u: a side is 1 to %d", what, p->width, p->height, SHPI_MAX_SIDE);
    return 0;
}

static int check_pictures(const FamSource *src, ExchangeError *err)
{
    if (src->horizon == NULL) { exchange_fail(err, "a stage needs a horizon picture"); return 0; }
    if (!picture_fits(src->horizon, "the horizon", err)) return 0;
    for (size_t i = 0; i < src->scenery_count; i++)
        if (!picture_fits(&src->scenery[i], "a scenery picture", err)) return 0;
    for (size_t i = 0; i < src->terrain_count; i++)
        if (!picture_fits(&src->terrain[i], "a terrain picture", err)) return 0;
    return 1;
}

static int check_counts(const FamSource *src, ExchangeError *err)
{
    if (src->terrain_count < 2) {
        exchange_fail(err, "%zu terrain textures: a stage needs at least 2 (with one the game takes the stage for a circuit)", src->terrain_count);
        return 0;
    }
    if (src->terrain_count > limit_of("FAM.BANKS_BACKGROUND")) {
        exchange_fail(err, "%zu terrain textures: the game holds at most %zu", src->terrain_count, limit_of("FAM.BANKS_BACKGROUND"));
        return 0;
    }
    if (src->scenery_count > limit_of("FAM.BANKS_FOREGROUND")) {
        exchange_fail(err, "%zu scenery textures: the game holds at most %zu", src->scenery_count, limit_of("FAM.BANKS_FOREGROUND"));
        return 0;
    }
    if (src->terrain_count + src->scenery_count > limit_of("FAM.BANKS_TOTAL")) {
        exchange_fail(err, "%zu terrain and %zu scenery textures: the game holds at most %zu together", src->terrain_count, src->scenery_count,
                      limit_of("FAM.BANKS_TOTAL"));
        return 0;
    }
    return 1;
}

/* the counts first: the pictures are only walked once they are known to be in the game's limits */
static int check_source(const FamSource *src, ExchangeError *err)
{
    return check_counts(src, err) && check_pictures(src, err);
}

/* ---- the wwww table */

/* a "wwww" table of the `n` chunks in `child`, each padded to 4 bytes; the offsets are from the table's own tag */
static int table_build(const Blob *child, size_t n, Blob *out, ExchangeError *err)
{
    size_t total = TABLE_HEAD + TABLE_ENTRY * n;
    for (size_t i = 0; i < n; i++) total += padded(child[i].len);
    uint8_t *b = calloc(total, 1);
    if (b == NULL) { exchange_fail(err, "out of memory"); return 0; }
    memcpy(b, "wwww", 4), put32(b + 4, (uint32_t)n);
    size_t at = TABLE_HEAD + TABLE_ENTRY * n;
    for (size_t i = 0; i < n; i++) {
        put32(b + TABLE_HEAD + TABLE_ENTRY * i, (uint32_t)at);
        memcpy(b + at, child[i].data, child[i].len);
        at += padded(child[i].len);
    }
    out->data = b;
    out->len = total;
    return 1;
}

/* ---- the banks */

static ShpiDescEntry entry_of(const char name[4], uint8_t type, uint32_t width, uint32_t height, const uint8_t *data, uint32_t place)
{
    ShpiDescEntry e;
    memset(&e, 0, sizeof e);
    memcpy(e.name, name, 4);
    e.type = type;
    e.width = (uint16_t)width;
    e.height = (uint16_t)height;
    e.data = data;
    e.block_size = SHPI_BLOCK_AUTO;
    e.dir_index = place;
    return e;
}

static ShpiDescEntry palette_entry(const uint8_t palette6[FAM_PALETTE_BYTES], uint32_t place)
{
    return entry_of("!pal", PALETTE_TYPE, 0, 0, palette6, place);
}

static int bank_of(ShpiDescEntry *e, size_t n, Blob *out, ExchangeError *err)
{
    return shpi_build(e, n, &out->data, &out->len, err);
}

/* a terrain bank: the palette, then letters A, B, C each with the variants 000..007 */
static int terrain_bank(const Terrain *t, const uint8_t palette6[FAM_PALETTE_BYTES], Blob *out, ExchangeError *err)
{
    enum { ENTRIES = 1 + FAM_LETTERS * FAM_VARIANTS };
    ShpiDescEntry e[ENTRIES];
    e[BANK_PALETTE] = palette_entry(palette6, BANK_PALETTE);
    uint32_t place = 1;
    for (int l = 0; l < FAM_LETTERS; l++) {
        if (!(t->letters & (1u << l))) continue;
        for (int v = 0; v < FAM_VARIANTS; v++, place++) {
            const char name[4] = { LETTERS[l], '0', '0', (char)('0' + v) };
            e[place] = entry_of(name, BITMAP_TYPE, LADDER[v][0], LADDER[v][1], t->index[v], place);
        }
    }
    return bank_of(e, place, out, err);
}

/* a bank of one picture under `name` (scenery "0000", the horizon "horz") */
static int picture_bank(const char name[4], const FamPicture *pic, const uint8_t palette6[FAM_PALETTE_BYTES], Blob *out, ExchangeError *err)
{
    uint8_t *index = fam_indices(pic, palette6);
    if (index == NULL) { exchange_fail(err, "out of memory"); return 0; }
    ShpiDescEntry e[2];
    e[BANK_PALETTE] = palette_entry(palette6, BANK_PALETTE);
    e[1] = entry_of(name, BITMAP_TYPE, pic->width, pic->height, index, 1);
    const int ok = bank_of(e, 2, out, err);
    free(index);
    return ok;
}

/* ---- the palette and the terrain's variants */

static int terrain_resample(const FamPicture *src, Terrain *t, ExchangeError *err)
{
    for (int v = 0; v < FAM_VARIANTS; v++) {
        t->width[v] = LADDER[v][0], t->height[v] = LADDER[v][1];
        t->rgba[v] = fam_resample(src, t->width[v], t->height[v]);
        if (t->rgba[v] == NULL) { exchange_fail(err, "out of memory"); return 0; }
    }
    return 1;
}

static FamPicture variant_picture(const Terrain *t, int v)
{
    const FamPicture p = { t->width[v], t->height[v], t->rgba[v] };
    return p;
}

/* the palette of every picture the stage shows: each terrain variant, the scenery and the horizon */
static int stage_palette(const FamSource *src, const Terrain *t, uint8_t palette6[FAM_PALETTE_BYTES], ExchangeError *err)
{
    const size_t total = src->terrain_count * FAM_VARIANTS + src->scenery_count + 1;
    FamPicture *pics = malloc(total * sizeof *pics);
    if (pics == NULL) { exchange_fail(err, "out of memory"); return 0; }
    size_t k = 0;
    for (size_t i = 0; i < src->terrain_count; i++)
        for (int v = 0; v < FAM_VARIANTS; v++) pics[k++] = variant_picture(&t[i], v);
    for (size_t i = 0; i < src->scenery_count; i++) pics[k++] = src->scenery[i];
    pics[k++] = *src->horizon;
    const int ok = fam_palette_make(pics, total, palette6);
    free(pics);
    if (!ok) exchange_fail(err, "out of memory");
    return ok;
}

static int terrain_index(Terrain *t, const uint8_t palette6[FAM_PALETTE_BYTES], ExchangeError *err)
{
    for (int v = 0; v < FAM_VARIANTS; v++) {
        t->index[v] = fam_indices(&(FamPicture){ t->width[v], t->height[v], t->rgba[v] }, palette6);
        if (t->index[v] == NULL) { exchange_fail(err, "out of memory"); return 0; }
    }
    return 1;
}

/* ---- the file */

static int check_size(size_t len, ExchangeError *err)
{
    if (len <= limit_of("FAM.FILE_SIZE")) return 1;
    exchange_fail(err, "the FAM would be %zu bytes: the game reads at most %zu", len, limit_of("FAM.FILE_SIZE"));
    return 0;
}

/* the banks of the table, each freed after the table is built */
static int bank_table(Blob *bank, size_t n, Blob *out, ExchangeError *err)
{
    const int ok = table_build(bank, n, out, err);
    for (size_t i = 0; i < n; i++) blob_free(&bank[i]);
    return ok;
}

static int terrain_table(const Terrain *t, size_t n, const uint8_t palette6[FAM_PALETTE_BYTES], Blob *out, ExchangeError *err)
{
    Blob *bank = calloc(n, sizeof *bank);
    if (bank == NULL) { exchange_fail(err, "out of memory"); return 0; }
    int ok = 1;
    for (size_t i = 0; ok && i < n; i++) ok = terrain_bank(&t[i], palette6, &bank[i], err);
    ok = ok && bank_table(bank, n, out, err);
    free(bank);
    return ok;
}

static int scenery_table(const FamSource *src, const uint8_t palette6[FAM_PALETTE_BYTES], Blob *out, ExchangeError *err)
{
    Blob *bank = calloc(src->scenery_count ? src->scenery_count : 1, sizeof *bank);
    if (bank == NULL) { exchange_fail(err, "out of memory"); return 0; }
    int ok = 1;
    for (size_t i = 0; ok && i < src->scenery_count; i++) ok = picture_bank("0000", &src->scenery[i], palette6, &bank[i], err);
    ok = ok && bank_table(bank, src->scenery_count, out, err);
    free(bank);
    return ok;
}

/* the root's four children, in chunk.h's order; the objects are an empty table */
static int root_children(const FamSource *src, Terrain *t, Blob child[FAM_CHILDREN], ExchangeError *err)
{
    uint8_t palette6[FAM_PALETTE_BYTES];
    if (!stage_palette(src, t, palette6, err)) return 0;
    for (size_t i = 0; i < src->terrain_count; i++)
        if (!terrain_index(&t[i], palette6, err)) return 0;
    return terrain_table(t, src->terrain_count, palette6, &child[FAM_CHILD_TERRAIN], err)
        && scenery_table(src, palette6, &child[FAM_CHILD_SCENERY], err)
        && picture_bank("horz", src->horizon, palette6, &child[FAM_CHILD_HORIZON], err)
        && table_build(child, 0, &child[FAM_CHILD_OBJECTS], err);
}

static int file_of(const FamSource *src, Terrain *t, uint8_t **out, size_t *out_len, ExchangeError *err)
{
    Blob child[FAM_CHILDREN];
    memset(child, 0, sizeof child);
    int ok = root_children(src, t, child, err);
    Blob root = { NULL, 0 };
    ok = ok && table_build(child, FAM_CHILDREN, &root, err) && check_size(root.len, err);
    for (int i = 0; i < FAM_CHILDREN; i++) blob_free(&child[i]);
    if (ok) *out = root.data, *out_len = root.len;
    else blob_free(&root);
    return ok;
}

int fam_build(const FamSource *src, uint8_t **out, size_t *out_len, ExchangeError *err)
{
    *out = NULL, *out_len = 0;
    if (!check_source(src, err)) return 0;
    Terrain *t = calloc(src->terrain_count, sizeof *t);
    if (t == NULL) { exchange_fail(err, "out of memory"); return 0; }
    int ok = 1;
    for (size_t i = 0; ok && i < src->terrain_count; i++) {
        t[i].letters = src->terrain_letters != NULL ? src->terrain_letters[i] : FAM_LETTERS_ALL;
        if ((t[i].letters & FAM_LETTERS_ALL) == 0 || (t[i].letters & ~(unsigned)FAM_LETTERS_ALL) != 0) {
            exchange_fail(err, "terrain texture %zu: letters must be some of A, B, C", i);
            ok = 0;
            break;
        }
        ok = terrain_resample(&src->terrain[i], &t[i], err);
    }
    ok = ok && file_of(src, t, out, out_len, err);
    terrain_free(t, src->terrain_count);
    return ok;
}
