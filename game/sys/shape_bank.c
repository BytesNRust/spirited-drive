/* Shape banks (shape_bank.h). */
#include "game/sys/shape_bank.h"

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
    const int32_t count = (int32_t)le32(bank + BANK_COUNT);
    for (int32_t i = 0; i < count; i++) {
        const uint8_t *entry = bank + BANK_DIRECTORY + BANK_ENTRY * i;
        if (memcmp(entry, name, NAME_BYTES) == 0)
            return bank + le32(entry + NAME_BYTES);
    }
    return NULL;
}
