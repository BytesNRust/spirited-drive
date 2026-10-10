/* Fixed-size binary records (PBS, PDN) <-> TSV. A field table partitions the record into named numbers and
 * opaque byte runs; an export writes one row per field:
 *     offset  name  type  value...        (one value per element, hex for byte runs)
 * An import reads such a table back over a copy of the original record. Rows may be left out (their bytes
 * stay as they were) but an unknown name, a wrong offset or type, or a value that does not fit the type is
 * refused with the file and line. */
#ifndef TNFS_RECORD_EXCHANGE_H
#define TNFS_RECORD_EXCHANGE_H

#include "formats/exchange.h"

typedef enum { REC_I32, REC_U32, REC_I16, REC_U16, REC_I8, REC_U8, REC_BYTES } RecordKind;

typedef struct {
    uint16_t offset;
    uint8_t kind;      /* RecordKind */
    uint16_t count;    /* elements; for REC_BYTES the length in bytes */
    const char *name;
} RecordField;

/* The table must be sorted by offset, not overlap, and stay inside `size`. */
int record_export(const uint8_t *record, size_t size, const RecordField *fields, size_t field_count,
                  const char *header, FileSet *out, const char *file, ExchangeError *err);

/* `record` holds the original on entry and the edited record on success. */
int record_import(uint8_t *record, size_t size, const RecordField *fields, size_t field_count,
                  const FileSet *in, const char *file, ExchangeError *err);

#endif
