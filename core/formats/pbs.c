#include "formats/pbs.h"

#include "formats/fixed_record.h"
#include "formats/le.h"

#include <stdlib.h>
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

/* the names exports used before the fields were known (2026-11-16), read as today's: { old, new } */
static const char *const OLD_NAMES[][2] = { { "\tunknown_018\t", "\tfront_drive_share\t" }, { "\tunknown_01c\t", "\tfront_brake_share\t" } };

/* `in` with specs.tsv's old names made today's, into `out` (its own copy); 0 out of memory */
static int with_new_names(const FileSet *in, FileSet *out, ExchangeError *err)
{
    fileset_init(out);
    for (size_t i = 0; i < in->count; ++i) {
        const SetFile *f = &in->file[i];
        if (strcmp(f->name, specs_file) != 0) {
            if (!fileset_add(out, f->name, f->data, f->size)) return exchange_fail(err, "out of memory"), 0;
            continue;
        }
        /* each old name is at most as long as its new one plus 4: room for every row renamed */
        const size_t cap = f->size * 2 + 64;
        char *text = malloc(cap), *at;
        if (text == NULL) return exchange_fail(err, "out of memory"), 0;
        memcpy(text, f->data, f->size), text[f->size] = '\0';
        for (size_t k = 0; k < sizeof OLD_NAMES / sizeof OLD_NAMES[0]; ++k)
            while ((at = strstr(text, OLD_NAMES[k][0])) != NULL) {
                const size_t from = strlen(OLD_NAMES[k][0]), to = strlen(OLD_NAMES[k][1]);
                memmove(at + to, at + from, strlen(at + from) + 1);
                memcpy(at, OLD_NAMES[k][1], to);
            }
        if (!fileset_add_owned(out, f->name, (uint8_t *)text, strlen(text))) return free(text), exchange_fail(err, "out of memory"), 0;
    }
    return 1;
}

int pbs_import(uint8_t record[PBS_SIZE], const FileSet *in, ExchangeError *err)
{
    FileSet named;
    int ok = with_new_names(in, &named, err) && record_import(record, PBS_SIZE, pbs_fields, pbs_field_count, &named, specs_file, err);
    fileset_free(&named);
    if (!ok) return 0;
    pbs_seal(record);
    return 1;
}
