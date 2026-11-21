/* A track art file's banks read back (fam_banks.h). */
#include "formats/fam_banks.h"

#include "formats/chunk.h"
#include "formats/fsh.h"

#include <stdlib.h>
#include <string.h>

static int terrain_table(const uint8_t *fam, size_t size, Chunk *terrain, ExchangeError *err)
{
    Chunk root;
    if (chunk_open(&root, fam, size) != CHUNK_OK || chunk_child(&root, FAM_CHILD_TERRAIN, terrain) != CHUNK_OK || terrain->kind != CHUNK_KIND_HEADER)
        return exchange_fail(err, "not a track art file (FAM)"), 0;
    return 1;
}

static int bank_open(const Chunk *terrain, uint32_t bank, Fsh *fsh, ExchangeError *err)
{
    Chunk b;
    if (bank >= terrain->count || chunk_child(terrain, bank, &b) != CHUNK_OK || b.kind != CHUNK_KIND_BITMAP || chunk_bitmaps(&b, fsh) != FSH_OK)
        return exchange_fail(err, "terrain bank %u is not in the art (%u banks)", (unsigned)bank, (unsigned)terrain->count), 0;
    return 1;
}

static int first_variant(const Fsh *fsh, char letter, FshEntry *e)
{
    const char name[4] = { letter, '0', '0', '0' };
    uint32_t at;
    return fsh_find(fsh, name, &at) == FSH_OK && fsh_entry(fsh, at, e) == FSH_OK && e->type == FSH_TYPE_BITMAP;
}

int fam_terrain_banks(const uint8_t *fam, size_t size, uint32_t *count, ExchangeError *err)
{
    Chunk terrain;
    if (!terrain_table(fam, size, &terrain, err)) return 0;
    *count = terrain.count;
    return 1;
}

int fam_bank_info(const uint8_t *fam, size_t size, uint32_t bank, FamBankInfo *out, ExchangeError *err)
{
    Chunk terrain;
    Fsh fsh;
    memset(out, 0, sizeof *out);
    if (!terrain_table(fam, size, &terrain, err) || !bank_open(&terrain, bank, &fsh, err)) return 0;
    for (int l = 0; l < 3; ++l) {
        FshEntry e;
        if (!first_variant(&fsh, (char)('A' + l), &e)) continue;
        out->letters |= 1u << l, out->width[l] = e.width, out->height[l] = e.height;
    }
    return 1;
}

int fam_bank_rgba(const uint8_t *fam, size_t size, uint32_t bank, char letter, uint8_t **rgba, int *width, int *height, ExchangeError *err)
{
    Chunk terrain;
    Fsh fsh;
    FshEntry pic, pal;
    uint32_t at;
    uint8_t rgb[768];
    *rgba = NULL;
    if (letter < 'A' || letter > 'C') return exchange_fail(err, "letter %c is none of A, B and C", letter), 0;
    if (!terrain_table(fam, size, &terrain, err) || !bank_open(&terrain, bank, &fsh, err)) return 0;
    if (!first_variant(&fsh, letter, &pic)) return exchange_fail(err, "terrain bank %u has no letter %c", (unsigned)bank, letter), 0;
    if (fsh_find(&fsh, "!pal", &at) != FSH_OK || fsh_entry(&fsh, at, &pal) != FSH_OK || fsh_palette_rgb(&pal, rgb) != FSH_OK)
        return exchange_fail(err, "terrain bank %u has no palette", (unsigned)bank), 0;
    const size_t n = (size_t)pic.width * pic.height;
    uint8_t *out = malloc(n * 4 > 0 ? n * 4 : 1);
    if (out == NULL) return exchange_fail(err, "out of memory"), 0;
    for (size_t i = 0; i < n; ++i) {
        const uint8_t ix = pic.payload[i];
        out[4 * i] = rgb[3 * ix], out[4 * i + 1] = rgb[3 * ix + 1], out[4 * i + 2] = rgb[3 * ix + 2], out[4 * i + 3] = ix == 0xFF ? 0 : 255;
    }
    *rgba = out, *width = pic.width, *height = pic.height;
    return 1;
}
