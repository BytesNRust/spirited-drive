#include "formats/bnk.h"

#include "formats/le.h"

#include <string.h>

enum {
    EACS_OFFSET = 0x28,
    BIT_DEPTH_OFFSET = 0x30,
    CHANNELS_OFFSET = 0x31
};

BnkResult bnk_open(Bnk *bnk, const uint8_t *data, size_t size)
{
    if (size < BNK_TABLE_SIZE) return BNK_ERR_TRUNCATED;
    bnk->data = data;
    bnk->size = size;
    return BNK_OK;
}

static uint32_t slot_offset(const Bnk *bnk, int slot)
{
    return le_u32(bnk->data + 4 * slot);
}

BnkResult bnk_sample(const Bnk *bnk, int slot, BnkSample *out)
{
    if (slot < 0 || slot >= BNK_SLOTS) return BNK_ERR_SLOT;
    const uint32_t at = slot_offset(bnk, slot);
    if (at == 0) return BNK_ABSENT;
    if (at > bnk->size || bnk->size - at < BNK_HEADER_SIZE) return BNK_ERR_BOUNDS;
    const uint8_t *h = bnk->data + at;
    if (memcmp(h + EACS_OFFSET, "EACS", 4) != 0 || le_u32(h + 4) != at + EACS_OFFSET) return BNK_ERR_HEADER;
    if ((h[BIT_DEPTH_OFFSET] != 1 && h[BIT_DEPTH_OFFSET] != 2) || (h[CHANNELS_OFFSET] != 1 && h[CHANNELS_OFFSET] != 2))
        return BNK_ERR_HEADER;
    memcpy(out->header, h, BNK_HEADER_SIZE);
    out->header_offset = at;
    out->flags = le_u32(h);
    out->rate = le_u32(h + 0x2c);
    out->bits = 8 * h[BIT_DEPTH_OFFSET];
    out->channels = h[CHANNELS_OFFSET];
    out->loop_start = le_u32(h + 0x34);
    out->loop_length = le_u32(h + 0x38);
    out->length = le_u32(h + 0x3c);
    out->data_offset = le_u32(h + 0x40);
    const uint64_t end = (uint64_t)out->data_offset + (uint64_t)out->length * (uint64_t)bnk_frame_bytes(out);
    if (out->data_offset > bnk->size || end > bnk->size) return BNK_ERR_DATA;
    return BNK_OK;
}

int bnk_frame_bytes(const BnkSample *sample)
{
    return sample->bits / 8 * sample->channels;
}

size_t bnk_data_extent(const Bnk *bnk, const BnkSample *sample)
{
    size_t next = bnk->size;
    for (int slot = 0; slot < BNK_SLOTS; ++slot) {
        BnkSample other;
        const BnkResult r = bnk_sample(bnk, slot, &other);
        if (r != BNK_OK && r != BNK_ERR_DATA) continue;
        if (other.data_offset > sample->data_offset && other.data_offset < next) next = other.data_offset;
    }
    return next - sample->data_offset;
}

const char *bnk_result_str(BnkResult result)
{
    switch (result) {
    case BNK_OK: return "ok";
    case BNK_ABSENT: return "no sample in this slot";
    case BNK_ERR_TRUNCATED: return "file shorter than the 512-byte offset table";
    case BNK_ERR_SLOT: return "slot outside 0..127";
    case BNK_ERR_BOUNDS: return "sample header outside the file";
    case BNK_ERR_HEADER: return "bad sample header (EACS marker, +4 field, bit depth or channels)";
    case BNK_ERR_DATA: return "sample data runs past the end of the file";
    }
    return "unknown";
}
