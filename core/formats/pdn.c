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
