#include "formats/table_exchange.h"

#include "formats/record_value.h"
#include "formats/tsv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int columns_of(const RecordField *fields, size_t n)
{
    int cols = 1;
    for (size_t i = 0; i < n; ++i) cols += fields[i].kind == REC_BYTES ? 1 : fields[i].count;
    return cols;
}

static size_t record_end(const RecordField *fields, size_t n)
{
    size_t end = 0;
    for (size_t i = 0; i < n; ++i) {
        const size_t e = (size_t)fields[i].offset + (size_t)fields[i].count * (size_t)rec_kind_size[fields[i].kind];
        if (e > end) end = e;
    }
    return end;
}

static void column_name(const RecordField *f, uint16_t k, char *buf, size_t cap)
{
    if (f->kind == REC_BYTES || f->count == 1) snprintf(buf, cap, "%s", f->name);
    else snprintf(buf, cap, "%s[%u]", f->name, k);
}

int table_export(const uint8_t *records, size_t stride, size_t rows, const RecordField *fields, size_t field_count,
                 const char *header, FileSet *out, const char *file, ExchangeError *err)
{
    if (columns_of(fields, field_count) > TSV_MAX_CELLS || record_end(fields, field_count) > stride) {
        exchange_fail(err, "internal: bad column table for %s", file);
        return 0;
    }
    TsvWriter w;
    tsv_writer_init(&w);
    if (header) tsv_comment(&w, header);
    tsv_str(&w, "row");
    char name[48];
    for (size_t i = 0; i < field_count; ++i) {
        const int cells = fields[i].kind == REC_BYTES ? 1 : fields[i].count;
        for (int k = 0; k < cells; ++k) {
            column_name(&fields[i], (uint16_t)k, name, sizeof name);
            tsv_str(&w, name);
        }
    }
    tsv_row(&w);
    for (size_t r = 0; r < rows; ++r) {
        const uint8_t *rec = records + r * stride;
        tsv_uint(&w, (uint32_t)r);
        for (size_t i = 0; i < field_count; ++i) {
            const RecordField *f = &fields[i];
            if (f->kind == REC_BYTES) { tsv_hex(&w, rec + f->offset, f->count); continue; }
            for (uint16_t k = 0; k < f->count; ++k) {
                const int64_t v = rec_load(rec + f->offset + (size_t)k * rec_kind_size[f->kind], rec_kind_size[f->kind], rec_kind_signed[f->kind]);
                if (v < 0) tsv_int(&w, (int32_t)v);
                else tsv_uint(&w, (uint32_t)v);
            }
        }
        tsv_row(&w);
    }
    const int ok = !w.failed && fileset_add(out, file, w.data, w.len);
    if (!ok) exchange_fail(err, "cannot write %s", file);
    tsv_writer_free(&w);
    return ok;
}

int table_import(uint8_t *records, size_t stride, size_t min_rows, size_t max_rows, const RecordField *fields,
                 size_t field_count, const FileSet *in, const char *file, size_t *rows_found, ExchangeError *err)
{
    const SetFile *table = fileset_find(in, file);
    if (table == NULL) { exchange_fail(err, "%s not found", file); return 0; }
    const int cols = columns_of(fields, field_count);
    if (cols > TSV_MAX_CELLS || record_end(fields, field_count) > stride) { exchange_fail(err, "internal: bad column table for %s", file); return 0; }

    TsvReader reader;
    TsvRow row;
    tsv_reader_init(&reader, table->data, table->size);
    int ok = 1;
    size_t found = 0;
    if (tsv_next(&reader, &row) != TSV_OK) { exchange_fail(err, "%s: empty table", file); ok = 0; }
    if (ok) {   /* the column names */
        int c = 1;
        char name[48];
        int match = row.cells == cols && strcmp(row.cell[0], "row") == 0;
        for (size_t i = 0; match && i < field_count; ++i) {
            const int cells = fields[i].kind == REC_BYTES ? 1 : fields[i].count;
            for (int k = 0; k < cells && match; ++k, ++c) {
                column_name(&fields[i], (uint16_t)k, name, sizeof name);
                match = strcmp(row.cell[c], name) == 0;
            }
        }
        if (!match) { exchange_fail(err, "%s:%u: the column names differ from the ones an export writes", file, row.line); ok = 0; }
    }
    TsvResult t = TSV_OK;
    while (ok && (t = tsv_next(&reader, &row)) == TSV_OK) {
        uint32_t index = 0;
        if (row.cells != cols) { exchange_fail(err, "%s:%u: expected %d cells, found %d", file, row.line, cols, row.cells); ok = 0; break; }
        if (tsv_get_uint(&row, 0, &index) != TSV_OK || index != found) {
            exchange_fail(err, "%s:%u: row index must be %zu", file, row.line, found);
            ok = 0;
            break;
        }
        if (found >= max_rows) { exchange_fail(err, "%s:%u: more than %zu rows", file, row.line, max_rows); ok = 0; break; }
        uint8_t *rec = records + found * stride;
        int c = 1;
        for (size_t i = 0; ok && i < field_count; ++i) {
            const RecordField *f = &fields[i];
            if (f->kind == REC_BYTES) {
                size_t n = 0;
                if (tsv_get_hex(&row, c, rec + f->offset, f->count, &n) != TSV_OK || n != f->count) {
                    exchange_fail(err, "%s:%u: column '%s' needs %u bytes of hex", file, row.line, f->name, f->count);
                    ok = 0;
                }
                ++c;
                continue;
            }
            for (uint16_t k = 0; k < f->count && ok; ++k, ++c) {
                int64_t v;
                int32_t s;
                uint32_t u;
                if (tsv_get_int(&row, c, &s) == TSV_OK) v = s;
                else if (tsv_get_uint(&row, c, &u) == TSV_OK) v = u;
                else { exchange_fail(err, "%s:%u: column %d ('%s') is not a number", file, row.line, c, f->name); ok = 0; break; }
                if (!rec_fits(v, rec_kind_size[f->kind], rec_kind_signed[f->kind]) &&
                    !(rec_kind_signed[f->kind] && v >= 0 && rec_fits(v, rec_kind_size[f->kind], 0))) {
                    exchange_fail(err, "%s:%u: column %d ('%s') = %lld does not fit %s", file, row.line, c, f->name, (long long)v,
                                  rec_kind_names[f->kind]);
                    ok = 0;
                    break;
                }
                rec_store(rec + f->offset + (size_t)k * rec_kind_size[f->kind], rec_kind_size[f->kind], v);
            }
        }
        if (ok) ++found;
    }
    tsv_reader_free(&reader);
    if (ok && t != TSV_END) { exchange_fail(err, "%s: row too long", file); ok = 0; }
    if (ok && found < min_rows) { exchange_fail(err, "%s has %zu rows, expected at least %zu", file, found, min_rows); ok = 0; }
    if (ok) *rows_found = found;
    return ok;
}
