#include "game/viewer/drive_livery.h"

#include "formats/chunk.h"
#include "formats/fsh.h"
#include "formats/psx_bigf.h"
#include "game/race/car_livery.h"
#include "game/platform/platform.h"
#include "game/sys/port_log.h"
#include "game/sys/shape_bank.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { LIVERY_PATH = 512, LIVERY_STEM = 16, SHAPE_DATA = 0x10, LEVELS = 2 };

static const uint8_t PAL_NAME[4] = { '!', 'P', 'A', 'L' }, OWN_NAME[4] = { '!', 'x', 'x', 'x' };

/* The PC palette entries (indices into the car's '!PAL') that are body colour the PS1 textures do not show as body:
 * the colours both liveries paint, which the count alone leaves unmapped. Found from the PC car's textures: the entries
 * at least four fifths of whose pixels are in the body textures (topv, frnt, rsid, bott, wing), not already mapped, and
 * for TSUPRA red (r > g + 24, r > b + 24). Palette indices only: no colour is copied from the CD. The ANSX's body is
 * a ramp of greys that the glass and trim share, so its set is read from the CFM's textures each time
 * (car_livery_body_set) and not listed. */
static const uint8_t TSUPRA_BODY[] = {
    20, 34, 35, 37, 38, 40, 42, 43, 68, 69, 70, 72, 79, 84, 90, 94, 100, 101, 102, 109, 110, 111, 115, 117, 123, 124,
    129, 130, 133, 134, 135, 136, 138, 139, 150, 153, 154, 155, 156, 157, 159, 160, 164, 165, 169, 171, 172, 173, 176,
    177, 190, 191, 252
};
/* The table for a car's stem (upper case), and its length; 0 entries for a car without one. */
static const uint8_t *forced_table(const char *upper, int *count)
{
    *count = 0;
    if (strcmp(upper, "TSUPRA") == 0) {
        *count = (int)sizeof TSUPRA_BODY;
        return TSUPRA_BODY;
    }
    return NULL;
}

/* The counts of the colours a car's Z<STEM>.<ext> file uses (its texture directory is the lower-case stem .psh), or
 * NULL when the file is missing or does not parse. */
static uint32_t *colours_of(const char *psx_dir, const char *upper, const char *lower, const char *ext)
{
    char path[LIVERY_PATH], member[LIVERY_STEM + 4];
    snprintf(path, sizeof path, "%s/Z%s.%s", psx_dir, upper, ext);
    snprintf(member, sizeof member, "%s.psh", lower);
    size_t size = 0;
    uint8_t *file = platform_read_file(path, &size);
    if (file == NULL) return NULL;
    const uint8_t *texts = NULL;
    size_t length = 0;
    uint32_t *counts = calloc(PSX_COLOURS, sizeof *counts);
    if (counts != NULL && !(psx_bigf_member(file, size, member, &texts, &length) && psx_shpp_colours(texts, length, counts))) {
        free(counts);
        counts = NULL;
    }
    platform_free_file(file);
    return counts;
}

/* The '!PAL' of the CFM's high detail bank: its shape's type and its 256 entries. 0 when the CFM does not parse. */
static int cfm_palette(uint8_t *cfm, size_t size, int *type, uint8_t **palette)
{
    Chunk file, shpi;
    Fsh bank;
    if (chunk_open(&file, cfm, size) != CHUNK_OK) return 0;
    if (chunk_child(&file, 1, &shpi) != CHUNK_OK || shpi.kind != CHUNK_KIND_BITMAP) return 0;
    if (fsh_open(&bank, shpi.data, shpi.size) != FSH_OK) return 0;
    uint8_t *shape = shape_bank_find((uint8_t *)(uintptr_t)bank.data, PAL_NAME);
    if (shape == NULL) return 0;
    *type = shape[0];
    *palette = shape + SHAPE_DATA;
    return 1;
}

/* The counts of each palette index in the CFM's high detail textures: all of them, and the body ones (topv, frnt, rsid,
 * bott, wing). 0 when the CFM does not parse. */
static int texture_counts(uint8_t *cfm, size_t size, uint32_t total[256], uint32_t body[256])
{
    static const char *const BODY_TEXTURES[] = { "topv", "frnt", "rsid", "bott", "wing" };
    Chunk file, shpi;
    Fsh bank;
    if (chunk_open(&file, cfm, size) != CHUNK_OK) return 0;
    if (chunk_child(&file, 1, &shpi) != CHUNK_OK || shpi.kind != CHUNK_KIND_BITMAP) return 0;
    if (fsh_open(&bank, shpi.data, shpi.size) != FSH_OK) return 0;
    for (uint32_t i = 0; i < bank.count; i++) {
        FshEntry e;
        if (fsh_entry(&bank, i, &e) != FSH_OK || e.type != FSH_TYPE_BITMAP) continue;
        int is_body = 0;
        for (size_t k = 0; k < sizeof BODY_TEXTURES / sizeof *BODY_TEXTURES; k++)
            is_body |= memcmp(e.name, BODY_TEXTURES[k], 4) == 0;
        size_t pixels = (size_t)e.width * e.height;
        if (pixels > e.payload_size) pixels = e.payload_size;
        for (size_t p = 0; p < pixels; p++) {
            total[e.payload[p]]++;
            body[e.payload[p]] += (uint32_t)is_body;
        }
    }
    return 1;
}

/* The forced colours of the car: the table's palette entries, or the textures' paint entries for the ANSX, read from the
 * CFM's own palette. */
static int forced_colours(uint8_t *cfm, size_t size, const char *upper, uint16_t out[256])
{
    int count = 0, type = 0;
    uint8_t *palette = NULL;
    const uint8_t *table = forced_table(upper, &count);
    if (!cfm_palette(cfm, size, &type, &palette)) return 0;
    uint8_t entries[256];
    if (table == NULL && strcmp(upper, "ANSX") == 0) {
        uint32_t total[256] = { 0 }, body[256] = { 0 };
        if (!texture_counts(cfm, size, total, body)) return 0;
        count = car_livery_body_set(total, body, palette, type, entries);
        table = entries;
    }
    if (table == NULL) return 0;
    for (int k = 0; k < count; k++) out[k] = car_livery_colour(palette + 3 * table[k], type);
    return count;
}

/* One shape of a bank, remapped: its palette is the 768 bytes after the shape's header. */
static void remap_shape(uint8_t *bank, const uint8_t name[4], const CarLiveryPair *pairs, int count)
{
    uint8_t *shape = shape_bank_find(bank, name);
    if (shape != NULL) car_livery_remap(shape + SHAPE_DATA, shape[0], pairs, count);
}

/* The CFM's palettes (the high and low detail banks' '!PAL' and the '!xxx' colours) remapped in place. */
static void apply(uint8_t *cfm, size_t size, const CarLiveryPair *pairs, int count)
{
    Chunk file;
    if (chunk_open(&file, cfm, size) != CHUNK_OK) return;
    for (uint32_t level = 0; level < LEVELS; level++) {
        Chunk shpi;
        Fsh bank;
        if (chunk_child(&file, 2 * level + 1, &shpi) != CHUNK_OK || shpi.kind != CHUNK_KIND_BITMAP) continue;
        if (fsh_open(&bank, shpi.data, shpi.size) != FSH_OK) continue;
        uint8_t *data = (uint8_t *)(uintptr_t)bank.data;
        remap_shape(data, PAL_NAME, pairs, count);
        remap_shape(data, OWN_NAME, pairs, count);
    }
}

int drive_livery_swatch(const char *psx_dir, const char *stem, uint16_t colours[2])
{
    char upper[LIVERY_STEM], lower[LIVERY_STEM];
    size_t i = 0;
    for (; stem[i] != '\0' && i < LIVERY_STEM - 1; i++) {
        upper[i] = (char)toupper((unsigned char)stem[i]);
        lower[i] = (char)tolower((unsigned char)stem[i]);
    }
    upper[i] = lower[i] = '\0';
    uint32_t *normal = colours_of(psx_dir, upper, lower, "VSM"), *alternate = colours_of(psx_dir, upper, lower, "WSM");
    CarLiveryPair pairs[CAR_LIVERY_PAIRS];
    int found = 0;
    if (normal != NULL && alternate != NULL) {
        const int count = car_livery_map(normal, alternate, NULL, 0, pairs);
        found = count > 0 && car_livery_main(pairs, count, alternate, &colours[1]) &&
                car_livery_main_from(pairs, count, normal, &colours[0]);
    }
    free(normal);
    free(alternate);
    return found;
}

int drive_livery_car(const char *psx_dir, const char *stem, uint8_t *cfm, size_t size)
{
    char upper[LIVERY_STEM], lower[LIVERY_STEM];
    size_t i = 0;
    for (; stem[i] != '\0' && i < LIVERY_STEM - 1; i++) {
        upper[i] = (char)toupper((unsigned char)stem[i]);
        lower[i] = (char)tolower((unsigned char)stem[i]);
    }
    upper[i] = lower[i] = '\0';
    uint32_t *normal = colours_of(psx_dir, upper, lower, "VSM"), *alternate = colours_of(psx_dir, upper, lower, "WSM");
    CarLiveryPair pairs[CAR_LIVERY_PAIRS];
    uint16_t forced[256];
    int count = 0;
    if (normal != NULL && alternate != NULL) {
        const int nforced = forced_colours(cfm, size, upper, forced);
        count = car_livery_map(normal, alternate, forced, nforced, pairs);
    }
    free(normal);
    free(alternate);
    if (count == 0) {
        port_log("psx_liveries: no alternate livery for %s in %s\n", stem, psx_dir);
        return 0;
    }
    apply(cfm, size, pairs, count);
    return count;
}
