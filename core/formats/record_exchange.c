#include "formats/record_exchange.h"

#include "formats/tsv.h"

#include <stdio.h>

#include <stdlib.h>
#include <string.h>

static const char *const kind_names[] = { "i32", "u32", "i16", "u16", "i8", "u8", "bytes" };
static const int kind_size[] = { 4, 4, 2, 2, 1, 1, 1 };
static const int kind_signed[] = { 1, 0, 1, 0, 1, 0, 0 };

static int64_t load(const uint8_t *p, int size, int is_signed)
{
    uint32_t v = 0;
    for (int i = 0; i < size; ++i) v |= (uint32_t)p[i] << (8 * i);
    if (!is_signed) return v;
    const int bits = 8 * size;
    return bits == 32 ? (int64_t)(int32_t)v : (int64_t)(v ^ (1u << (bits - 1))) - (int64_t)(1u << (bits - 1));
}

static void store(uint8_t *p, int size, int64_t v)
{
    for (int i = 0; i < size; ++i) p[i] = (uint8_t)((uint64_t)v >> (8 * i));
}

static int fits(int64_t v, int size, int is_signed)
{
    const int bits = 8 * size;
    if (is_signed) return v >= -((int64_t)1 << (bits - 1)) && v < ((int64_t)1 << (bits - 1));
    return v >= 0 && v < ((int64_t)1 << bits);
}

static size_t field_bytes(const RecordField *f)
{
    return (size_t)f->count * (size_t)kind_size[f->kind];
}

static int table_ok(size_t size, const RecordField *fields, size_t n)
{
    size_t end = 0;
    for (size_t i = 0; i < n; ++i) {
        if (fields[i].offset < end || fields[i].count == 0 || fields[i].kind > REC_BYTES) return 0;
        end = (size_t)fields[i].offset + field_bytes(&fields[i]);
        if (end > size) return 0;
    }
    return 1;
}

int record_export(const uint8_t *record, size_t size, const RecordField *fields, size_t field_count,
                  const char *header, FileSet *out, const char *file, ExchangeError *err)
{
    if (!table_ok(size, fields, field_count)) { exchange_fail(err, "internal: bad field table for %s", file); return 0; }
    TsvWriter w;
    tsv_writer_init(&w);
    if (header) tsv_comment(&w, header);
    tsv_comment(&w, "offset\tname\ttype\tvalue...");
    char off[12];
    for (size_t i = 0; i < field_count; ++i) {
        const RecordField *f = &fields[i];
        snprintf(off, sizeof off, "0x%03x", f->offset);
        tsv_str(&w, off);
        tsv_str(&w, f->name);
        tsv_str(&w, kind_names[f->kind]);
        if (f->kind == REC_BYTES) {
            tsv_hex(&w, record + f->offset, f->count);
        } else {
            for (uint16_t k = 0; k < f->count; ++k) {
                const int64_t v = load(record + f->offset + (size_t)k * kind_size[f->kind], kind_size[f->kind], kind_signed[f->kind]);
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

int record_import(uint8_t *record, size_t size, const RecordField *fields, size_t field_count,
                  const FileSet *in, const char *file, ExchangeError *err)
{
    const SetFile *table = fileset_find(in, file);
    if (table == NULL) { exchange_fail(err, "%s not found", file); return 0; }
    if (!table_ok(size, fields, field_count)) { exchange_fail(err, "internal: bad field table for %s", file); return 0; }
    uint8_t *work = malloc(size);
    if (work == NULL) { exchange_fail(err, "out of memory"); return 0; }
    memcpy(work, record, size);
    TsvReader r;
    TsvRow row;
    tsv_reader_init(&r, table->data, table->size);
    int ok = 1;
    TsvResult t;
    while (ok && (t = tsv_next(&r, &row)) == TSV_OK) {
        uint32_t offset = 0;
        const RecordField *f = NULL;
        if (row.cells < 4 || strncmp(row.cell[0], "0x", 2) != 0 || tsv_get_uint(&row, 0, &offset) != TSV_OK) {
            exchange_fail(err, "%s:%u: expected offset, name, type and values", file, row.line);
            ok = 0;
            break;
        }
        for (size_t i = 0; i < field_count; ++i)
            if (fields[i].offset == offset) f = &fields[i];
        if (f == NULL || strcmp(f->name, row.cell[1]) != 0) {
            exchange_fail(err, "%s:%u: no field '%s' at offset 0x%x", file, row.line, row.cell[1], offset);
            ok = 0;
            break;
        }
        if (strcmp(kind_names[f->kind], row.cell[2]) != 0) {
            exchange_fail(err, "%s:%u: '%s' is %s, not %s", file, row.line, f->name, kind_names[f->kind], row.cell[2]);
            ok = 0;
            break;
        }
        if (f->kind == REC_BYTES) {
            size_t n = 0;
            if (tsv_get_hex(&row, 3, work + f->offset, f->count, &n) != TSV_OK || n != f->count) {
                exchange_fail(err, "%s:%u: '%s' needs %u bytes of hex", file, row.line, f->name, f->count);
                ok = 0;
            }
            continue;
        }
        if (row.cells != 3 + f->count) {
            exchange_fail(err, "%s:%u: '%s' has %u values, found %d", file, row.line, f->name, f->count, row.cells - 3);
            ok = 0;
            break;
        }
        for (uint16_t k = 0; k < f->count && ok; ++k) {
            int64_t v;
            int32_t s;
            uint32_t u;
            if (tsv_get_int(&row, 3 + k, &s) == TSV_OK) v = s;
            else if (tsv_get_uint(&row, 3 + k, &u) == TSV_OK) v = u;
            else {
                exchange_fail(err, "%s:%u: '%s'[%u] is not a number", file, row.line, f->name, k);
                ok = 0;
                break;
            }
            if (!fits(v, kind_size[f->kind], kind_signed[f->kind]) && !(kind_signed[f->kind] && v >= 0 && fits(v, kind_size[f->kind], 0))) {
                exchange_fail(err, "%s:%u: '%s'[%u] = %lld does not fit %s", file, row.line, f->name, k, (long long)v, kind_names[f->kind]);
                ok = 0;
                break;
            }
            store(work + f->offset + (size_t)k * kind_size[f->kind], kind_size[f->kind], v);
        }
    }
    tsv_reader_free(&r);
    if (ok && t != TSV_END) { exchange_fail(err, "%s: row too long", file); ok = 0; }
    if (ok) memcpy(record, work, size);
    free(work);
    return ok;
}
