#include "formats/shpi_exchange.h"

#include "formats/doc.h"
#include "formats/fsh.h"
#include "formats/le.h"
#include "formats/png.h"
#include "formats/tsv.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    ENTRY_HEADER = 16,
    PALETTE_BYTES = 768,
    TAIL_MAX = 1 << 24    /* bytes after the pixels one entry may carry */
};

static const char entries_file[] = "entries.tsv";

static void put_u16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put_u24(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); }

static int is_bitmap(uint8_t type) { return type == FSH_TYPE_BITMAP; }
static int is_palette(uint8_t type) { return type == FSH_TYPE_PALETTE_6BIT || type == FSH_TYPE_PALETTE_8BIT; }

/* File-name safe label from the 4 name bytes. */
static void label_of(const uint8_t name[4], char label[5])
{
    for (int i = 0; i < 4; ++i) label[i] = (isalnum(name[i]) || name[i] == '_' || name[i] == '-') ? (char)name[i] : '_';
    label[4] = '\0';
}

static void grey_ramp(uint8_t rgb[PALETTE_BYTES])
{
    for (int i = 0; i < 256; ++i) rgb[3 * i] = rgb[3 * i + 1] = rgb[3 * i + 2] = (uint8_t)i;
}

static void expand_palette(uint8_t type, const uint8_t *payload, uint8_t rgb[PALETTE_BYTES])
{
    const int shift = type == FSH_TYPE_PALETTE_8BIT ? 0 : 2;
    for (int i = 0; i < PALETTE_BYTES; ++i) rgb[i] = (uint8_t)(payload[i] << shift);
}

/* ------------------------------------------------------------------ export */

int shpi_export(const uint8_t *data, size_t size, const uint8_t *external_palette, FileSet *out, ExchangeError *err)
{
    DocNode *doc = NULL;
    const DocResult dr = doc_parse_shpi(data, size, &doc);
    if (dr != DOC_OK) { exchange_fail(err, "not a readable SHPI directory: %s", doc_result_str(dr)); return 0; }
    const uint32_t count = doc_entry_count(doc);
    const uint8_t *head = NULL;
    size_t head_size = 0;
    head = doc_head(doc, &head_size);

    /* the reference palette */
    uint8_t rgb[PALETTE_BYTES];
    int have_palette = 0;
    for (uint32_t i = 0; i < count && !have_palette; ++i) {
        DocNode *child = NULL;
        size_t n = 0;
        doc_entry(doc, i, &child);
        const uint8_t *b = child ? doc_bytes(child, &n) : NULL;
        if (b != NULL && n >= ENTRY_HEADER + PALETTE_BYTES && is_palette(b[0])) {
            expand_palette(b[0], b + ENTRY_HEADER, rgb);
            have_palette = 1;
        }
    }
    if (!have_palette) {
        if (external_palette != NULL) memcpy(rgb, external_palette, PALETTE_BYTES);
        else grey_ramp(rgb);
    }

    TsvWriter tsv;
    tsv_writer_init(&tsv);
    tsv_comment(&tsv, "index\tname_hex\tlabel\ttype\tblock_size\twidth\theight\tmisc0\tmisc1\tmisc2\tmisc3\tfile\ttail_hex");
    int ok = 1;
    for (uint32_t i = 0; i < count && ok; ++i) {
        DocNode *child = NULL;
        size_t n = 0;
        doc_entry(doc, i, &child);
        const uint8_t *b = child ? doc_bytes(child, &n) : NULL;
        const uint8_t *name = head + 16 + 8 * (size_t)i;
        if (b == NULL || n < ENTRY_HEADER) {
            exchange_fail(err, "entry %u (offset 0x%x): shorter than its 16-byte header", i, (unsigned)le_u32(name + 4));
            ok = 0;
            break;
        }
        const uint8_t type = b[0];
        const uint32_t block_size = (uint32_t)b[1] | (uint32_t)b[2] << 8 | (uint32_t)b[3] << 16;
        const uint16_t w = (uint16_t)(b[4] | b[5] << 8), h = (uint16_t)(b[6] | b[7] << 8);
        char label[5], file[40];
        label_of(name, label);
        const size_t pixels = (size_t)w * h;
        const int bitmap = is_bitmap(type);
        if (bitmap && n < ENTRY_HEADER + pixels) {
            exchange_fail(err, "entry %u (offset 0x%x): %ux%u pixels do not fit its %zu bytes", i,
                          (unsigned)le_u32(name + 4), w, h, n);
            ok = 0;
            break;
        }
        if (bitmap && n - ENTRY_HEADER - pixels > TAIL_MAX) {
            exchange_fail(err, "entry %u (offset 0x%x): %zu bytes after the pixels, more than the %d the exchange keeps", i,
                          (unsigned)le_u32(name + 4), n - ENTRY_HEADER - pixels, TAIL_MAX);
            ok = 0;
            break;
        }
        snprintf(file, sizeof file, "%04u_%s.%s", i, label, bitmap ? "png" : "bin");
        if (bitmap) {
            uint8_t *png = NULL;
            size_t png_len = 0;
            if (png_write_indexed(b + ENTRY_HEADER, w, h, rgb, &png, &png_len) != PNG_OK ||
                !fileset_add_owned(out, file, png, png_len)) {
                free(png);
                exchange_fail(err, "entry %u (offset 0x%x): cannot write %s", i, (unsigned)le_u32(name + 4), file);
                ok = 0;
                break;
            }
        } else if (!fileset_add(out, file, b + ENTRY_HEADER, n - ENTRY_HEADER)) {
            exchange_fail(err, "entry %u: cannot write %s", i, file);
            ok = 0;
            break;
        }
        tsv_uint(&tsv, i);
        tsv_hex(&tsv, name, 4);
        tsv_str(&tsv, label);
        tsv_uint(&tsv, type);
        if (bitmap && block_size == ENTRY_HEADER + pixels) tsv_str(&tsv, "auto");
        else tsv_uint(&tsv, block_size);
        tsv_uint(&tsv, w);
        tsv_uint(&tsv, h);
        for (int m = 0; m < 4; ++m) tsv_int(&tsv, (int16_t)(b[8 + 2 * m] | b[9 + 2 * m] << 8));
        tsv_str(&tsv, file);
        if (bitmap) tsv_hex(&tsv, b + ENTRY_HEADER + pixels, n - ENTRY_HEADER - pixels);
        else tsv_hex(&tsv, NULL, 0);
        tsv_row(&tsv);
    }
    if (ok && (tsv.failed || !fileset_add(out, entries_file, tsv.data, tsv.len))) {
        exchange_fail(err, "cannot write %s", entries_file);
        ok = 0;
    }
    tsv_writer_free(&tsv);
    doc_free(doc);
    return ok;
}

/* ------------------------------------------------------------------ import */

static void quantizer_exact(const uint8_t rgb[PALETTE_BYTES], const uint8_t pixel[3], int *index, int *exact)
{
    long best = -1;
    *exact = 0;
    for (int i = 0; i < 256; ++i) {
        const long dr = rgb[3 * i] - pixel[0], dg = rgb[3 * i + 1] - pixel[1], db = rgb[3 * i + 2] - pixel[2];
        const long d = dr * dr + dg * dg + db * db;
        if (best < 0 || d < best) { best = d; *index = i; }
        if (d == 0) { *exact = 1; return; }   /* lowest index with that colour */
    }
}

/* Indices for a decoded PNG against the reference palette. */
static void to_indices(const PngImage *img, const uint8_t rgb[PALETTE_BYTES], uint8_t *indices, ImportStats *stats)
{
    const size_t n = (size_t)img->width * img->height;
    stats->pixels += (uint32_t)n;
    if (img->indexed && memcmp(img->palette, rgb, PALETTE_BYTES) == 0) {
        memcpy(indices, img->pixels, n);
        return;
    }
    for (size_t i = 0; i < n; ++i) {
        uint8_t c[3];
        if (img->indexed) memcpy(c, img->palette + 3 * (size_t)img->pixels[i], 3);
        else {
            if (img->pixels[4 * i + 3] < 128) { indices[i] = 0; continue; }   /* transparent */
            memcpy(c, img->pixels + 4 * i, 3);
        }
        int index, exact;
        quantizer_exact(rgb, c, &index, &exact);
        indices[i] = (uint8_t)index;
        if (!exact) stats->reassigned++;
    }
}

typedef struct {
    uint32_t index;
    uint8_t name[4];
    uint8_t type;
    int auto_block;
    uint32_t block_size, width, height;
    int32_t misc[4];
    char file[64];
    uint8_t *tail;
    size_t tail_len;
} Row;

static int parse_row(const TsvRow *r, const char *source, Row *row, ExchangeError *err)
{
    uint32_t type = 0;
    size_t n = 0;
    TsvResult t;
    memset(row, 0, sizeof *row);
    if (r->cells < 13) { exchange_fail(err, "%s:%u: expected 13 cells, found %d", source, r->line, r->cells); return 0; }
    if ((t = tsv_get_uint(r, 0, &row->index)) != TSV_OK || (t = tsv_get_hex(r, 1, row->name, 4, &n)) != TSV_OK ||
        n != 4 || (t = tsv_get_uint(r, 3, &type)) != TSV_OK || type > 255) {
        exchange_fail(err, "%s:%u: bad index, name or type", source, r->line);
        return 0;
    }
    row->type = (uint8_t)type;
    if (strcmp(r->cell[4], "auto") == 0) row->auto_block = 1;
    else if ((t = tsv_get_uint(r, 4, &row->block_size)) != TSV_OK || row->block_size > 0xFFFFFF) {
        exchange_fail(err, "%s:%u: block_size must be auto or a 24-bit number", source, r->line);
        return 0;
    }
    if ((t = tsv_get_uint(r, 5, &row->width)) != TSV_OK || (t = tsv_get_uint(r, 6, &row->height)) != TSV_OK ||
        row->width > 0xFFFF || row->height > 0xFFFF) {
        exchange_fail(err, "%s:%u: bad width or height", source, r->line);
        return 0;
    }
    for (int m = 0; m < 4; ++m) {
        if (tsv_get_int(r, 7 + m, &row->misc[m]) != TSV_OK || row->misc[m] < -32768 || row->misc[m] > 32767) {
            exchange_fail(err, "%s:%u: misc%d is not a 16-bit number", source, r->line, m);
            return 0;
        }
    }
    if (strlen(r->cell[11]) >= sizeof row->file || !fileset_name_ok(r->cell[11])) {
        exchange_fail(err, "%s:%u: unusable file name '%s'", source, r->line, r->cell[11]);
        return 0;
    }
    strcpy(row->file, r->cell[11]);
    const size_t hex_len = strlen(r->cell[12]);
    row->tail = malloc(hex_len / 2 + 1);
    if (row->tail == NULL) { exchange_fail(err, "out of memory"); return 0; }
    if (tsv_get_hex(r, 12, row->tail, hex_len / 2 + 1, &row->tail_len) != TSV_OK) {
        exchange_fail(err, "%s:%u: tail_hex is not a hex string", source, r->line);
        return 0;
    }
    return 1;
}

int shpi_import(const uint8_t *original, size_t size, const uint8_t *external_palette, const FileSet *in,
                uint8_t **out, size_t *out_len, ImportStats *stats, ExchangeError *err)
{
    const SetFile *table = fileset_find(in, entries_file);
    if (table == NULL) { exchange_fail(err, "%s not found", entries_file); return 0; }
    DocNode *doc = NULL;
    const DocResult dr = doc_parse_shpi(original, size, &doc);
    if (dr != DOC_OK) { exchange_fail(err, "original is not a readable SHPI directory: %s", doc_result_str(dr)); return 0; }
    const uint32_t count = doc_entry_count(doc);
    size_t head_size = 0;
    const uint8_t *head = doc_head(doc, &head_size);

    Row *rows = calloc(count ? count : 1, sizeof *rows);
    int ok = rows != NULL;
    if (!ok) exchange_fail(err, "out of memory");
    uint32_t found = 0;
    TsvReader reader;
    tsv_reader_init(&reader, table->data, table->size);
    TsvRow tr;
    while (ok && tsv_next(&reader, &tr) == TSV_OK) {
        Row row;
        if (!parse_row(&tr, entries_file, &row, err)) { free(row.tail); ok = 0; break; }
        if (row.index != found || found >= count) {
            exchange_fail(err, "%s:%u: entry %u out of order or beyond the original's %u entries", entries_file, tr.line, row.index, count);
            free(row.tail);
            ok = 0;
            break;
        }
        if (memcmp(row.name, head + 16 + 8 * (size_t)found, 4) != 0) {
            exchange_fail(err, "%s:%u: name differs from the original's entry %u (renaming entries is not supported)", entries_file, tr.line, found);
            free(row.tail);
            ok = 0;
            break;
        }
        rows[found++] = row;
    }
    tsv_reader_free(&reader);
    if (ok && found != count) {
        exchange_fail(err, "%s lists %u of the original's %u entries", entries_file, found, count);
        ok = 0;
    }

    /* the reference palette: the SHPI's first palette entry as imported, else external, else grey */
    uint8_t rgb[PALETTE_BYTES];
    int have_palette = 0;
    for (uint32_t i = 0; ok && i < count && !have_palette; ++i) {
        if (!is_palette(rows[i].type)) continue;
        const SetFile *f = fileset_find(in, rows[i].file);
        if (f == NULL || f->size < PALETTE_BYTES) {
            exchange_fail(err, "%s: palette file '%s' missing or shorter than 768 bytes", entries_file, rows[i].file);
            ok = 0;
        } else {
            expand_palette(rows[i].type, f->data, rgb);
            have_palette = 1;
        }
    }
    if (ok && !have_palette) {
        if (external_palette != NULL) memcpy(rgb, external_palette, PALETTE_BYTES);
        else grey_ramp(rgb);
    }

    memset(stats, 0, sizeof *stats);
    for (uint32_t i = 0; ok && i < count; ++i) {
        const Row *row = &rows[i];
        const SetFile *f = fileset_find(in, row->file);
        if (f == NULL) { exchange_fail(err, "%s: file '%s' (entry %u) not found", entries_file, row->file, i); ok = 0; break; }
        uint8_t *entry = NULL;
        size_t entry_len = 0;
        if (is_bitmap(row->type)) {
            PngImage img;
            const PngResult pr = png_read(f->data, f->size, &img);
            if (pr != PNG_OK) { exchange_fail(err, "%s: %s", row->file, png_result_str(pr)); ok = 0; break; }
            if (img.width != row->width || img.height != row->height) {
                exchange_fail(err, "%s: image is %ux%u but %s says %ux%u (edit both to resize)", row->file, img.width, img.height,
                              entries_file, row->width, row->height);
                png_free(img.pixels);
                ok = 0;
                break;
            }
            const size_t pixels = (size_t)img.width * img.height;
            entry_len = ENTRY_HEADER + pixels + row->tail_len;
            entry = malloc(entry_len);
            if (entry == NULL) { png_free(img.pixels); exchange_fail(err, "out of memory"); ok = 0; break; }
            to_indices(&img, rgb, entry + ENTRY_HEADER, stats);
            png_free(img.pixels);
            memcpy(entry + ENTRY_HEADER + pixels, row->tail, row->tail_len);
        } else {
            entry_len = ENTRY_HEADER + f->size;
            entry = malloc(entry_len);
            if (entry == NULL) { exchange_fail(err, "out of memory"); ok = 0; break; }
            memcpy(entry + ENTRY_HEADER, f->data, f->size);
        }
        entry[0] = row->type;
        put_u24(entry + 1, row->auto_block ? (uint32_t)(ENTRY_HEADER + (size_t)row->width * row->height) : row->block_size);
        put_u16(entry + 4, (uint16_t)row->width);
        put_u16(entry + 6, (uint16_t)row->height);
        for (int m = 0; m < 4; ++m) put_u16(entry + 8 + 2 * m, (uint16_t)(int16_t)row->misc[m]);
        DocNode *child = NULL;
        doc_entry(doc, i, &child);
        if (child == NULL || doc_set_bytes(child, entry, entry_len) != DOC_OK) {
            exchange_fail(err, "entry %u cannot be replaced", i);
            ok = 0;
        }
        free(entry);
    }
    if (ok) {
        const size_t total = doc_size(doc);
        uint8_t *buf = malloc(total ? total : 1);
        size_t len = 0;
        if (buf == NULL || doc_write(doc, buf, total, &len) != DOC_OK) {
            free(buf);
            exchange_fail(err, "cannot assemble the SHPI");
            ok = 0;
        } else {
            *out = buf;
            *out_len = len;
        }
    }
    if (rows != NULL) for (uint32_t i = 0; i < count; ++i) free(rows[i].tail);
    free(rows);
    doc_free(doc);
    return ok;
}
