/* Shape banks (shape_bank.h). */
#include "game/sys/shape_bank.h"

#include "core/formats/fsh.h"

#include <stddef.h>
#include <string.h>

enum {
    BANK_COUNT = 8,
    BANK_DIRECTORY = 0x10,
    BANK_ENTRY = 8,
    NAME_BYTES = 4
};

static uint32_t le32(const uint8_t *b)
{
    return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}

uint8_t *shape_bank_find(uint8_t *bank, const uint8_t name[4])
{
    if (bank == NULL) return NULL;   /* port: a bank missing or refused (shape_bank_valid) has no shapes */
    const int32_t count = (int32_t)le32(bank + BANK_COUNT);
    for (int32_t i = 0; i < count; i++) {
        const uint8_t *entry = bank + BANK_DIRECTORY + BANK_ENTRY * i;
        if (memcmp(entry, name, NAME_BYTES) == 0)
            return bank + le32(entry + NAME_BYTES);
    }
    return NULL;
}

enum { PALETTE_BYTES = 256 * 3 };

/* "!pal" in either case: the name the game reads 768 palette bytes from, whatever the shape's type */
static int palette_name(const char name[4])
{
    return name[0] == '!' && (name[1] | 0x20) == 'p' && (name[2] | 0x20) == 'a' && (name[3] | 0x20) == 'l';
}

/* one shape within its bank: a bitmap's pixels, a palette's 768 bytes (a palette type, or a shape named "!pal"), and no other
 * type. A palette is named '!' as all the CD's are, so a shape found by any other name is a bitmap whose pixels are there. */
static int shape_fits(const FshEntry *e)
{
    const int palette = e->type == FSH_TYPE_PALETTE_6BIT || e->type == FSH_TYPE_PALETTE_8BIT;
    if (!palette && e->type != FSH_TYPE_BITMAP) return 0;
    if (palette && e->name[0] != '!') return 0;
    if (e->type == FSH_TYPE_BITMAP && (uint64_t)e->width * e->height > e->payload_size) return 0;
    return (palette || palette_name(e->name)) ? e->payload_size >= PALETTE_BYTES : 1;
}

int shape_bank_valid(const uint8_t *bank, size_t size)
{
    Fsh fsh;
    if (bank == NULL || fsh_open(&fsh, bank, size) != FSH_OK || fsh.count > INT32_MAX) return 0;
    for (uint32_t i = 0; i < fsh.count; i++) {
        FshEntry e;
        if (fsh_entry(&fsh, i, &e) != FSH_OK || !shape_fits(&e)) return 0;
    }
    return 1;
}
