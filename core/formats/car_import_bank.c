/* The car importer's banks: the donor's palettes first, then the author's textures fitted to them (formats/car_texture.h), then
 * the donor's bitmaps still named (the tyres). See car_import.h. */
#include "formats/car_import_internal.h"
#include "formats/car_texture.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ci_note(const CarImportIn *in, const char *format, ...)
{
    if (in->note == NULL) return;
    char text[300];
    va_list ap;
    va_start(ap, format);
    vsnprintf(text, sizeof text, format, ap);
    va_end(ap);
    in->note(in->note_ctx, text);
}

int ci_grow(void **p, size_t n, size_t size, ExchangeError *err)
{
    void *q = realloc(*p, (n ? n : 1) * size);
    if (q == NULL) { exchange_fail(err, "out of memory"); return 0; }
    *p = q;
    return 1;
}

const ShpiDescEntry *ci_bank_find(const ShpiDescEntry *entries, size_t n, const char name[4])
{
    for (size_t i = 0; i < n; i++)
        if (memcmp(entries[i].name, name, 4) == 0) return &entries[i];
    return NULL;
}

static int add(CarImportBank *bank, ShpiDescEntry e, ExchangeError *err)
{
    if (ci_bank_find(bank->entry, bank->count, e.name) != NULL) return 1;
    if (!ci_grow((void **)&bank->entry, bank->count + 1, sizeof *bank->entry, err)) return 0;
    e.dir_index = (uint32_t)bank->count;
    bank->entry[bank->count++] = e;
    return 1;
}

int ci_bank_keep(CarImportBank *bank, const ShpiDescEntry *entry, ExchangeError *err)
{
    ShpiDescEntry e = *entry;
    e.tail = NULL, e.tail_len = 0, e.lead = NULL, e.lead_len = 0, e.block_size = SHPI_BLOCK_AUTO;
    return add(bank, e, err);
}

int ci_bank_start(CarImportBank *bank, int level, const ShpiDescEntry *donor_bank, size_t donor_count, ExchangeError *err)
{
    memset(bank, 0, sizeof *bank);
    static const char *const PALETTES[2] = { "!PAL", "!xxx" };
    for (int k = 0; k < (level == 0 ? 2 : 1); k++) {
        const ShpiDescEntry *e = ci_bank_find(donor_bank, donor_count, PALETTES[k]);
        if (e == NULL) { exchange_fail(err, "the donor's %s bank has no %s", level == 0 ? "high" : "low", PALETTES[k]); return 0; }
        if (!ci_bank_keep(bank, e, err)) return 0;
    }
    return 1;
}

int ci_bank_png(CarImportBank *bank, const char name[4], const uint8_t *png, size_t len, const CarImportIn *in, ExchangeError *err)
{
    const ShpiDescEntry *pal = ci_bank_find(bank->entry, bank->count, "!PAL");
    if (pal == NULL) { exchange_fail(err, "the bank has no !PAL to fit a texture to"); return 0; }
    uint8_t *px = NULL;
    uint16_t w = 0, h = 0;
    CarTextureStats s;
    if (!car_texture_from_png(png, len, pal->data, pal->type, &px, &w, &h, &s, err)) return 0;
    if (!ci_grow((void **)&bank->owned, bank->owned_count + 1, sizeof *bank->owned, err)) { free(px); return 0; }
    bank->owned[bank->owned_count++] = px;
    if (s.moved > 0) ci_note(in, "texture '%.4s': %u of its %u pixels took the nearest of the car's colours", name, s.moved, s.pixels);
    ShpiDescEntry e;
    memset(&e, 0, sizeof e);
    memcpy(e.name, name, 4);
    e.type = 0x7b, e.width = w, e.height = h, e.data = px, e.block_size = SHPI_BLOCK_AUTO;
    return add(bank, e, err);
}

void car_import_bank_free(CarImportBank *bank)
{
    for (size_t i = 0; i < bank->owned_count; i++) free(bank->owned[i]);
    free(bank->owned);
    free(bank->entry);
    memset(bank, 0, sizeof *bank);
}
