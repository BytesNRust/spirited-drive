#include "formats/pbs.h"

#include "formats/fixed_record.h"
#include "formats/le.h"

#include <string.h>

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

void pbs_seal(uint8_t record[PBS_SIZE])
{
    const uint32_t sum = (uint32_t)pbs_checksum(record);
    for (int i = 0; i < 4; ++i) record[PBS_CHECKSUM_OFFSET + i] = (uint8_t)(sum >> (8 * i));
}

QfsResult pbs_pack(const uint8_t record[PBS_SIZE], uint8_t *dst, size_t cap, size_t *len)
{
    uint8_t sealed[PBS_SIZE];
    memcpy(sealed, record, PBS_SIZE);
    pbs_seal(sealed);
    return fixed_record_pack(sealed, PBS_SIZE, dst, cap, len);
}

static const char specs_file[] = "specs.tsv";

int pbs_export(const uint8_t record[PBS_SIZE], FileSet *out, ExchangeError *err)
{
    return record_export(record, PBS_SIZE, pbs_fields, pbs_field_count,
                         "PBS performance record (CARSPECS/*.PBS); the checksum is recomputed on pack", out, specs_file, err);
}

int pbs_import(uint8_t record[PBS_SIZE], const FileSet *in, ExchangeError *err)
{
    if (!record_import(record, PBS_SIZE, pbs_fields, pbs_field_count, in, specs_file, err)) return 0;
    pbs_seal(record);
    return 1;
}
