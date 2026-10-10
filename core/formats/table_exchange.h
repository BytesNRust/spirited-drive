/* Arrays of fixed-size records (TRI nodes, scenery, segments) <-> TSV. One row per record: an index column,
 * then one column per element of each field (name, or name[k] for arrays; byte runs are one hex column).
 * The first row names the columns and must match exactly on import, so a shifted or renamed column is
 * caught with its line. Import overlays the values on the records already in memory (fields not in the
 * table keep their bytes) and reports how many rows the file has. */
#ifndef TNFS_TABLE_EXCHANGE_H
#define TNFS_TABLE_EXCHANGE_H

#include "formats/record_exchange.h"

int table_export(const uint8_t *records, size_t stride, size_t rows, const RecordField *fields, size_t field_count,
                 const char *header, FileSet *out, const char *file, ExchangeError *err);

/* `records` has room for max_rows; rows beyond the file's keep their old bytes. The file must hold between
 * min_rows and max_rows rows with indices 0, 1, 2, ...; *rows_found gets the count. */
int table_import(uint8_t *records, size_t stride, size_t min_rows, size_t max_rows, const RecordField *fields,
                 size_t field_count, const FileSet *in, const char *file, size_t *rows_found, ExchangeError *err);

#endif
