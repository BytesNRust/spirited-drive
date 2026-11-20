#include "formats/car_bank_walk.h"

#include "formats/chunk.h"
#include "formats/fsh.h"
#include "formats/shape_bank.h"

#include <stdlib.h>
#include <string.h>

enum { SHAPE_DATA = 0x10, LEVELS = 2 };

static const uint8_t PAL_NAME[4] = { '!', 'P', 'A', 'L' }, OWN_NAME[4] = { '!', 'x', 'x', 'x' };

int car_bank_walk(uint8_t *cfm, size_t size, const CarBankHooks *hooks, void *user)
{
    Chunk file;
    int changed = 0;
    if (hooks == NULL || hooks->bank == NULL || chunk_open(&file, cfm, size) != CHUNK_OK) return 0;
    for (uint32_t level = 0; level < LEVELS; level++) {
        Chunk shpi;
        Fsh bank;
        if (chunk_child(&file, 2 * level + 1, &shpi) != CHUNK_OK || shpi.kind != CHUNK_KIND_BITMAP) continue;
        if (fsh_open(&bank, shpi.data, shpi.size) != FSH_OK || !shape_bank_valid(shpi.data, shpi.size)) continue;
        uint8_t *data = (uint8_t *)(uintptr_t)bank.data;
        uint8_t *pal = shape_bank_find(data, PAL_NAME), *own = shape_bank_find(data, OWN_NAME);
        if (pal == NULL) continue;
        RepaintTexture textures[CAR_BANK_MAX_TEXTURES];
        CarBankMeta meta = { (int)level, { { 0 } } };
        int count = 0;
        for (uint32_t i = 0; i < bank.count && count < CAR_BANK_MAX_TEXTURES; i++) {
            FshEntry e;
            if (fsh_entry(&bank, i, &e) != FSH_OK || e.type != FSH_TYPE_BITMAP || e.name[0] == '!') continue;
            size_t pixels = (size_t)e.width * e.height;
            if (pixels > e.payload_size) pixels = e.payload_size;
            RepaintTexture tex = { (uint8_t *)(uintptr_t)e.payload, pixels, car_repaint_body_class(e.name, (int)level), 0, { 0 }, NULL };
            tex.width = (int)e.width;
            if (hooks->texture != NULL) hooks->texture(user, (int)level, e.name, (int)e.width, (int)e.height, &tex);
            memcpy(meta.name[count], e.name, 4);
            textures[count++] = tex;
        }
        const RepaintBank rb = { textures, count, { pal + SHAPE_DATA, own != NULL ? own + SHAPE_DATA : NULL }, 2, pal[0],
                                 own != NULL ? own[0] : 0 };
        const int n = hooks->bank(user, &rb, &meta);
        if (n > 0) changed += n;
        for (int k = 0; k < count; k++) free((void *)(uintptr_t)textures[k].mask);
    }
    return changed;
}
