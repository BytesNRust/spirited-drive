/* A shape bank as the game's art loaders take it (shape_bank_valid) and search it (shape_bank_find): the menus', the HUD's, the
 * cockpit's, the smoke's and the cars' and tracks' banks a mod can replace */
#include "fuzz/fuzz.h"
#include "game/sys/shape_bank.h"

#include <stdlib.h>
#include <string.h>

enum { HEADER = 16, ENTRY = 8, SHAPE_HEADER = 16, PALETTE = 768, BITMAP = 0x7b };

static uint32_t le32(const uint8_t *b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }

/* what the game reads of a shape it found: its header, then a bitmap's pixels or a palette's 768 bytes */
static void read_shape(const uint8_t *s)
{
    fuzz_touch(s, SHAPE_HEADER);
    const size_t w = (size_t)(s[4] | s[5] << 8), h = (size_t)(s[6] | s[7] << 8);
    fuzz_touch(s + SHAPE_HEADER, s[0] == BITMAP ? w * h : PALETTE);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    uint8_t *bank = malloc(size ? size : 1);   /* the search hands back writable shapes: a copy */
    if (bank == NULL) return 0;
    if (size) memcpy(bank, data, size);
    if (shape_bank_valid(bank, size)) {
        const uint32_t count = le32(bank + 8);
        for (uint32_t i = 0; i < count; i++) {
            const uint8_t *s = shape_bank_find(bank, bank + HEADER + ENTRY * i);
            if (s != NULL) read_shape(s);
        }
        const uint8_t *pal = shape_bank_find(bank, (const uint8_t *)"!pal");
        if (pal != NULL) fuzz_touch(pal, SHAPE_HEADER + PALETTE);
        (void)shape_bank_find(bank, (const uint8_t *)"none");
    }
    free(bank);
    return 0;
}
