#include "formats/fsh.h"

#include <string.h>

enum {
    HEADER_SIZE = 16,
    DIR_ENTRY_SIZE = 8,
    ENTRY_HEADER_SIZE = 16,
    PALETTE_BYTES = 768
};

static uint16_t read_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

FshResult fsh_open(Fsh *fsh, const uint8_t *data, size_t size)
{
    if (size < HEADER_SIZE || memcmp(data, "SHPI", 4) != 0) return FSH_ERR_HEADER;
    const uint32_t count = read_u32(data + 8);
    if (count > (size - HEADER_SIZE) / DIR_ENTRY_SIZE) return FSH_ERR_BOUNDS;
    fsh->data = data;
    fsh->size = size;
    fsh->count = count;
    return FSH_OK;
}

FshResult fsh_find(const Fsh *fsh, const char name[4], uint32_t *index)
{
    for (uint32_t i = 0; i < fsh->count; i++) {
        if (memcmp(fsh->data + HEADER_SIZE + (size_t)i * DIR_ENTRY_SIZE, name, 4) == 0) {
            *index = i;
            return FSH_OK;
        }
    }
    return FSH_ERR_NOT_FOUND;
}

FshResult fsh_entry(const Fsh *fsh, uint32_t index, FshEntry *out)
{
    if (index >= fsh->count) return FSH_ERR_BOUNDS;
    const uint8_t *dir = fsh->data + HEADER_SIZE + (size_t)index * DIR_ENTRY_SIZE;
    const uint32_t offset = read_u32(dir + 4);
    if (offset > fsh->size || fsh->size - offset < ENTRY_HEADER_SIZE) return FSH_ERR_BOUNDS;

    const uint8_t *e = fsh->data + offset;
    FshEntry entry;
    memcpy(entry.name, dir, 4);
    entry.type = e[0];
    entry.block_size = (uint32_t)e[1] | ((uint32_t)e[2] << 8) | ((uint32_t)e[3] << 16);
    entry.width = read_u16(e + 4);
    entry.height = read_u16(e + 6);
    for (int i = 0; i < 4; i++) entry.misc[i] = (int16_t)read_u16(e + 8 + 2 * i);
    entry.payload = e + ENTRY_HEADER_SIZE;
    entry.payload_size = fsh->size - offset - ENTRY_HEADER_SIZE;

    if (entry.type == FSH_TYPE_BITMAP && (size_t)entry.width * entry.height > entry.payload_size) {
        return FSH_ERR_BOUNDS;
    }
    *out = entry;
    return FSH_OK;
}

FshResult fsh_palette_rgb(const FshEntry *entry, uint8_t rgb[768])
{
    if (entry->type == FSH_TYPE_BITMAP) return FSH_ERR_TYPE;
    if (entry->payload_size < PALETTE_BYTES) return FSH_ERR_BOUNDS;
    /* 00463e08: type 0x24 is copied, anything else is VGA 6-bit shifted left by 2. */
    const int shift = entry->type == FSH_TYPE_PALETTE_8BIT ? 0 : 2;
    for (int i = 0; i < PALETTE_BYTES; i++) rgb[i] = (uint8_t)(entry->payload[i] << shift);
    return FSH_OK;
}

const char *fsh_result_str(FshResult result)
{
    switch (result) {
    case FSH_OK: return "ok";
    case FSH_ERR_HEADER: return "not an SHPI container";
    case FSH_ERR_BOUNDS: return "entry outside the data";
    case FSH_ERR_NOT_FOUND: return "entry not found";
    case FSH_ERR_TYPE: return "wrong entry type";
    }
    return "unknown error";
}
