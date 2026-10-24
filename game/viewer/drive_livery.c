#include "game/viewer/drive_livery.h"

#include "formats/chunk.h"
#include "formats/fsh.h"
#include "formats/psx_bigf.h"
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

/* The counts of the colours a car's Z<STEM>.<ext> file uses (its texture directory is the lower-case stem .psh), or
 * NULL when the file is missing or does not parse. */
static uint32_t *colours_of(const char *psx_dir, const char *stem, const char *ext)
{
    char path[LIVERY_PATH], member[LIVERY_STEM + 4], upper[LIVERY_STEM];
    size_t i = 0;
    for (; stem[i] != '\0' && i < LIVERY_STEM - 1; i++) {
        upper[i] = (char)toupper((unsigned char)stem[i]);
        member[i] = (char)tolower((unsigned char)stem[i]);
    }
    upper[i] = member[i] = '\0';
    snprintf(path, sizeof path, "%s/Z%s.%s", psx_dir, upper, ext);
    snprintf(member + i, sizeof member - i, ".psh");
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

int drive_livery_pairs(const char *psx_dir, const char *stem, CarLiveryPair pairs[CAR_LIVERY_PAIRS])
{
    uint32_t *normal = colours_of(psx_dir, stem, "VSM"), *alternate = colours_of(psx_dir, stem, "WSM");
    int count = 0;
    if (normal != NULL && alternate != NULL) count = car_livery_map(normal, alternate, pairs);
    if (count == 0) port_log("psx_liveries: no alternate livery for %s in %s\n", stem, psx_dir);
    free(normal);
    free(alternate);
    return count;
}

/* One shape of a bank, remapped: its palette is the 768 bytes after the shape's header. */
static void remap_shape(uint8_t *bank, const uint8_t name[4], const CarLiveryPair *pairs, int count)
{
    uint8_t *shape = shape_bank_find(bank, name);
    if (shape != NULL) car_livery_remap(shape + SHAPE_DATA, shape[0], pairs, count);
}

void drive_livery_apply(uint8_t *cfm, size_t size, const CarLiveryPair *pairs, int count)
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
