#include "card_central.h"

#include "car_export.h"
#include "formats/qfs.h"
#include "formats/shape_bank.h"
#include "raster/picture_palette.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { POINTER_COLOUR = 0xfe, SEE_THROUGH = 0xff, PALETTE_RAW = 0x24, SHAPE_HEADER = 0x10, PALETTE_BYTES = 768 };

static int fail(char *why, size_t cap, const char *what)
{
    if (why != NULL && cap > 0) snprintf(why, cap, "%s", what);
    return 0;
}

int card_central_palette(const char *cd, uint8_t palette[768], char *why, size_t why_cap)
{
    uint8_t *file = NULL;
    size_t size = 0;
    if (cd == NULL || !car_export_game_file(cd, "FRONTEND/ART/CONTROL/CENTRAL.QFS", &file, &size, why, why_cap)) return 0;
    uint8_t *bank = file;
    size_t bank_size = size;
    if (qfs_is_compressed(file, size)) {   /* the game's own order: unpacked, then searched as a shape bank */
        size_t unpacked = 0;
        bank = NULL;
        if (qfs_decompressed_size(file, size, &unpacked) != QFS_OK || unpacked == 0 || (bank = malloc(unpacked)) == NULL || qfs_decompress(file, size, bank, unpacked) != QFS_OK) {
            free(bank);
            free(file);
            return fail(why, why_cap, "Control Central's art cannot be unpacked");
        }
        bank_size = unpacked;
    }
    int ok = 0;
    if (!shape_bank_valid(bank, bank_size)) {
        fail(why, why_cap, "Control Central's art is not a picture bank the game reads");
    } else {
        const uint8_t name[4] = { '!', 'p', 'a', 'l' };
        const uint8_t *shape = shape_bank_find(bank, name);
        if (shape == NULL) {
            fail(why, why_cap, "Control Central's art has no palette");
        } else {   /* palette_put: the type byte says 8 bits (0x24); any other is 6 bits, shifted */
            for (int i = 0; i < PALETTE_BYTES; ++i) palette[i] = shape[0] == PALETTE_RAW ? shape[SHAPE_HEADER + i] : (uint8_t)(shape[SHAPE_HEADER + i] << 2);
            ok = 1;
        }
    }
    if (bank != file) free(bank);
    free(file);
    return ok;
}

void card_central_as_drawn(const uint8_t *rgba, int w, int h, const uint8_t palette[768], uint8_t *out)
{
    const size_t n = (size_t)w * (size_t)h;
    uint8_t usable[256], *index = malloc(n);
    memset(usable, 1, sizeof usable);
    usable[POINTER_COLOUR] = usable[SEE_THROUGH] = 0;   /* the screen's own */
    if (index == NULL) {
        memcpy(out, rgba, n * 4);
        return;
    }
    picture_palette_dither(rgba, w, h, palette, usable, index);
    for (size_t i = 0; i < n; ++i) {
        memcpy(out + 4 * i, palette + 3 * (size_t)index[i], 3);
        out[4 * i + 3] = 255;
    }
    free(index);
}
