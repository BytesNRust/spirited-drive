#include "formats/pdn.h"

#include "formats/fixed_record.h"
#include "formats/le.h"

int pdn_read(uint8_t out[PDN_SIZE], const uint8_t *file, size_t size)
{
    return fixed_record_read(out, PDN_SIZE, file, size);
}

int32_t pdn_i32(const uint8_t record[PDN_SIZE], int offset)
{
    return (int32_t)le_u32(record + offset);
}

QfsResult pdn_pack(const uint8_t record[PDN_SIZE], uint8_t *dst, size_t cap, size_t *len)
{
    return fixed_record_pack(record, PDN_SIZE, dst, cap, len);
}

/* Words the loaders copy are named (0040fa3c, 004407b8; car_struct.h); the rest are numbered. */
static const RecordField pdn_fields[] = {
    { 0x000, REC_I32, 3, "unknown_000" },
    { 0x00c, REC_I32, 1, "body_0c" },
    { 0x010, REC_I32, 1, "body_10" },
    { PDN_MASS, REC_I32, 1, "mass" },
    { 0x018, REC_I32, 1, "unknown_018" },
    { PDN_AI_SPEEDS, REC_I32, PDN_AI_SPEED_COUNT, "ai_speed" },
    { PDN_FACTORS, REC_I32, 6, "factor" },
    { 0x1c4, REC_I32, 1, "unknown_1c4" },
    { 0x1c8, REC_I32, 1, "unknown_1c8" }
};

static const char pdn_file[] = "pdn.tsv";

int pdn_export(const uint8_t record[PDN_SIZE], FileSet *out, ExchangeError *err)
{
    return record_export(record, PDN_SIZE, pdn_fields, sizeof pdn_fields / sizeof pdn_fields[0],
                         "PDN body and AI record (CARFAMS/*.PDN)", out, pdn_file, err);
}

int pdn_import(uint8_t record[PDN_SIZE], const FileSet *in, ExchangeError *err)
{
    return record_import(record, PDN_SIZE, pdn_fields, sizeof pdn_fields / sizeof pdn_fields[0], in, pdn_file, err);
}
