#include "formats/pbs.h"

#include "formats/fixed_record.h"
#include "formats/le.h"

int pbs_read(uint8_t out[PBS_SIZE], const uint8_t *file, size_t size)
{
    return fixed_record_read(out, PBS_SIZE, file, size);
}

int32_t pbs_checksum(const uint8_t record[PBS_SIZE])
{
    int32_t sum = 0;
    for (int i = 0; i < PBS_CHECKED_BYTES; ++i) sum += record[i];
    return sum;
}

int pbs_checksum_ok(const uint8_t record[PBS_SIZE])
{
    return (int32_t)le_u32(record + PBS_CHECKSUM_OFFSET) == pbs_checksum(record);
}
