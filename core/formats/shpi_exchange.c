#include "formats/shpi_exchange.h"

#include "formats/doc.h"
#include "formats/fsh.h"
#include "formats/le.h"
#include "formats/png.h"
#include "formats/shpi_build.h"
#include "formats/tsv.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    ENTRY_HEADER = 16,
    PALETTE_BYTES = 768,
    TAIL_MAX = 1 << 24,   /* bytes after the pixels one entry may carry */
    LEAD_MAX = 4096       /* bytes between the directory and the first entry a bank.tsv may carry (the CD's most: 2) */
};

static const char entries_file[] = SHPI_ENTRIES_FILE;

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

/* the one rule for an entry's file: its directory index and label */
static void entry_file(uint32_t index, const uint8_t name[4], int bitmap, char file[SHPI_FILE_NAME_SIZE])
{
    char label[5];
    label_of(name, label);
    snprintf(file, SHPI_FILE_NAME_SIZE, "%04u_%s.%s", (unsigned)index, label, bitmap ? "png" : "bin");
}

int shpi_entry_file(const uint8_t *data, size_t size, const char name[4], char file[SHPI_FILE_NAME_SIZE])
{
    file[0] = '\0';
    if (size < 16 || memcmp(data, "SHPI", 4) != 0) return 0;
    const uint32_t count = le_u32(data + 8);
    for (uint32_t i = 0; i < count && 16 + 8 * (size_t)i + 8 <= size; ++i) {
        const uint8_t *dir = data + 16 + 8 * (size_t)i;
        const uint32_t at = le_u32(dir + 4);
        if (memcmp(dir, name, 4) != 0) continue;
        if (at >= size || !is_bitmap(data[at])) return 0;
        entry_file(i, dir, 1, file);
        return 1;
    }
    return 0;
}

/* the SHPI's first palette entry as RGB (shpi_export's reference palette with no external one), else the grey ramp */
static void first_palette(const uint8_t *data, size_t size, uint8_t rgb[PALETTE_BYTES])
{
    const uint32_t count = le_u32(data + 8);
    for (uint32_t i = 0; i < count && 16 + 8 * (size_t)i + 8 <= size; ++i) {
        const uint32_t at = le_u32(data + 16 + 8 * (size_t)i + 4);
        if ((size_t)at < size && size - at >= ENTRY_HEADER + PALETTE_BYTES && is_palette(data[at])) {
            expand_palette(data[at], data + at + ENTRY_HEADER, rgb);
            return;
        }
    }
    grey_ramp(rgb);
}

int shpi_entry_rgba(const uint8_t *data, size_t size, const char name[4], uint8_t key, uint8_t **rgba, uint32_t *w, uint32_t *h,
                    uint32_t *keyed)
{
    *rgba = NULL, *w = *h = 0, *keyed = 0;
    if (size < 16 || memcmp(data, "SHPI", 4) != 0) return 0;
    const uint32_t count = le_u32(data + 8);
    for (uint32_t i = 0; i < count && 16 + 8 * (size_t)i + 8 <= size; ++i) {
        const uint32_t at = le_u32(data + 16 + 8 * (size_t)i + 4);
        if (memcmp(data + 16 + 8 * (size_t)i, name, 4) != 0) continue;
        if ((size_t)at >= size || size - at < ENTRY_HEADER || !is_bitmap(data[at])) return 0;
        const uint32_t bw = (uint32_t)(data[at + 4] | data[at + 5] << 8), bh = (uint32_t)(data[at + 6] | data[at + 7] << 8);
        const size_t n = (size_t)bw * bh;
        if (n == 0 || size - at - ENTRY_HEADER < n) return 0;
        uint8_t *out = malloc(4 * n);
        if (out == NULL) return 0;
        uint8_t rgb[PALETTE_BYTES];
        first_palette(data, size, rgb);
        const uint8_t *px = data + at + ENTRY_HEADER;
        for (size_t k = 0; k < n; ++k) {
            memcpy(out + 4 * k, rgb + 3 * (size_t)px[k], 3);
            out[4 * k + 3] = px[k] == key ? 0 : 0xff;
            *keyed += px[k] == key;
        }
        *rgba = out, *w = bw, *h = bh;
        return 1;
    }
    return 0;
}

/* entry `i`'s place in the file: the entries before it by offset, the directory's order among equals (shpi_describe's order) */
static uint32_t physical_place(const uint8_t *head, uint32_t count, uint32_t i)
{
    const uint32_t at = le_u32(head + 16 + 8 * (size_t)i + 4);
    uint32_t place = 0;
    for (uint32_t k = 0; k < count; ++k) {
        const uint32_t other = le_u32(head + 16 + 8 * (size_t)k + 4);
        place += other < at || (other == at && k < i);
    }
    return place;
}

/* bank.tsv: the head's tag (bytes 12..15), the bytes between the directory and the first entry, and the size word (bytes 4..7: auto when
 * it is the bank's length), each always written */
static int write_bank_file(const uint8_t *head, size_t head_size, uint32_t count, size_t size, FileSet *out, ExchangeError *err)
{
    const size_t directory_end = 16 + 8 * (size_t)count;
    TsvWriter tsv;
    tsv_writer_init(&tsv);
    tsv_comment(&tsv, "key\tvalue: tag is the bank's bytes 12..15, lead_hex the bytes between its directory and its first entry, size_word its bytes 4..7 (auto: its length)");
    tsv_str(&tsv, "tag");
    tsv_hex(&tsv, head + 12, 4);
    tsv_row(&tsv);
    tsv_str(&tsv, "lead_hex");
    tsv_hex(&tsv, head + directory_end, head_size > directory_end ? head_size - directory_end : 0);
    tsv_row(&tsv);
    tsv_str(&tsv, "size_word");
    const uint32_t word = le_u32(head + 4);
    if (word == size) tsv_str(&tsv, "auto");
    else tsv_uint(&tsv, word);
    tsv_row(&tsv);
    const int ok = !tsv.failed && fileset_add(out, SHPI_BANK_FILE, tsv.data, tsv.len);
    if (!ok) exchange_fail(err, "cannot write %s", SHPI_BANK_FILE);
    tsv_writer_free(&tsv);
    return ok;
}

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
    tsv_comment(&tsv, "index\tname_hex\tlabel\ttype\tblock_size\twidth\theight\tmisc0\tmisc1\tmisc2\tmisc3\tfile\ttail_hex\tpos");
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
        char label[5], file[SHPI_FILE_NAME_SIZE];
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
        entry_file(i, name, bitmap, file);
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
        tsv_uint(&tsv, physical_place(head, count, i));
        tsv_row(&tsv);
    }
    if (ok && (tsv.failed || !fileset_add(out, entries_file, tsv.data, tsv.len))) {
        exchange_fail(err, "cannot write %s", entries_file);
        ok = 0;
    }
    tsv_writer_free(&tsv);
    if (ok) ok = write_bank_file(head, head_size, count, size, out, err);
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
    uint32_t pos;   /* the physical place; the index when the row has no pos (an entries.tsv of before dep85) */
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
    row->pos = row->index;
    if (r->cells > 13 && tsv_get_uint(r, 13, &row->pos) != TSV_OK) {
        exchange_fail(err, "%s:%u: pos is not a number", source, r->line);
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

/* ------------------------------------------------------------------ from flat files (dep85) */

/* bank.tsv: `tag` (4 bytes), `lead_hex` and `size_word`, each once; a key it does not know is refused */
static int read_bank_file(const FileSet *in, ShpiHead *head, uint8_t *lead, size_t *lead_len, ExchangeError *err)
{
    const SetFile *f = fileset_find(in, SHPI_BANK_FILE);
    if (f == NULL) { exchange_fail(err, "%s not found: a bank's head is not made up", SHPI_BANK_FILE); return 0; }
    int tag = 0, has_lead = 0, has_size = 0, ok = 1;
    memset(head, 0, sizeof *head);
    TsvReader reader;
    tsv_reader_init(&reader, f->data, f->size);
    TsvRow r;
    while (ok && tsv_next(&reader, &r) == TSV_OK) {
        size_t n = 0;
        const char *value = r.cells > 1 ? r.cell[1] : "";
        if (strcmp(r.cell[0], "tag") == 0 && !tag) {
            ok = r.cells > 1 && tsv_get_hex(&r, 1, (uint8_t *)head->tag, 4, &n) == TSV_OK && n == 4;
            if (!ok) exchange_fail(err, "%s:%u: tag is 8 hex digits (4 bytes)", SHPI_BANK_FILE, r.line);
            tag = 1;
        } else if (strcmp(r.cell[0], "lead_hex") == 0 && !has_lead) {
            ok = value[0] == '\0' || tsv_get_hex(&r, 1, lead, LEAD_MAX, &n) == TSV_OK;
            if (!ok) exchange_fail(err, "%s:%u: lead_hex is not hex, or longer than %d bytes", SHPI_BANK_FILE, r.line, LEAD_MAX);
            *lead_len = value[0] == '\0' ? 0 : n;
            has_lead = 1;
        } else if (strcmp(r.cell[0], "size_word") == 0 && !has_size) {
            head->keep_size_word = strcmp(value, "auto") != 0;
            ok = !head->keep_size_word || tsv_get_uint(&r, 1, &head->size_word) == TSV_OK;
            if (!ok) exchange_fail(err, "%s:%u: size_word is auto or a number", SHPI_BANK_FILE, r.line);
            has_size = 1;
        } else {
            exchange_fail(err, "%s:%u: '%s' is not tag, lead_hex or size_word (or is there twice)", SHPI_BANK_FILE, r.line, r.cell[0]);
            ok = 0;
        }
    }
    tsv_reader_free(&reader);
    if (ok && (!tag || !has_lead || !has_size)) exchange_fail(err, "%s has no %s", SHPI_BANK_FILE, !tag ? "tag" : !has_lead ? "lead_hex" : "size_word"), ok = 0;
    return ok;
}

static void free_rows(Row *rows, uint32_t n)
{
    for (uint32_t i = 0; rows != NULL && i < n; ++i) free(rows[i].tail);
    free(rows);
}

/* entries.tsv's rows, in the directory's order (index 0, 1, ...), their physical places once each; *rows malloc'd */
static int read_rows(const FileSet *in, Row **rows, uint32_t *count, ExchangeError *err)
{
    *rows = NULL, *count = 0;
    const SetFile *table = fileset_find(in, entries_file);
    if (table == NULL) { exchange_fail(err, "%s not found", entries_file); return 0; }
    uint32_t cap = 0;
    int ok = 1;
    TsvReader reader;
    tsv_reader_init(&reader, table->data, table->size);
    TsvRow tr;
    while (ok && tsv_next(&reader, &tr) == TSV_OK) {
        if (*count == cap) {
            Row *grown = realloc(*rows, (cap = cap ? 2 * cap : 16) * sizeof **rows);
            if (grown == NULL) { exchange_fail(err, "out of memory"); ok = 0; break; }
            *rows = grown;
        }
        Row *row = &(*rows)[*count];
        ok = parse_row(&tr, entries_file, row, err);
        ++*count;   /* its tail is freed with the others, read or not */
        if (ok && row->index != *count - 1) exchange_fail(err, "%s:%u: entry %u where %u was due", entries_file, tr.line, row->index, *count - 1), ok = 0;
    }
    tsv_reader_free(&reader);
    if (ok && *count == 0) exchange_fail(err, "%s lists no entry", entries_file), ok = 0;
    uint8_t *taken = ok ? calloc(*count, 1) : NULL;
    if (ok && taken == NULL) exchange_fail(err, "out of memory"), ok = 0;
    for (uint32_t i = 0; ok && i < *count; ++i)
        if ((*rows)[i].pos >= *count || taken[(*rows)[i].pos]++)
            exchange_fail(err, "%s: entry %u's pos %u is taken or past the %u entries", entries_file, i, (*rows)[i].pos, *count), ok = 0;
    free(taken);
    if (!ok) free_rows(*rows, *count), *rows = NULL, *count = 0;
    return ok;
}

/* the reference palette from the rows: the first palette entry in the directory's order (shpi_export's), else external, else grey */
static int flat_palette(const FileSet *in, const Row *rows, uint32_t count, const uint8_t *external, uint8_t rgb[PALETTE_BYTES], ExchangeError *err)
{
    for (uint32_t i = 0; i < count; ++i) {
        if (!is_palette(rows[i].type)) continue;
        const SetFile *f = fileset_find(in, rows[i].file);
        if (f == NULL || f->size < PALETTE_BYTES) {
            exchange_fail(err, "%s: palette file '%s' missing or shorter than 768 bytes", entries_file, rows[i].file);
            return 0;
        }
        expand_palette(rows[i].type, f->data, rgb);
        return 1;
    }
    if (external != NULL) memcpy(rgb, external, PALETTE_BYTES);
    else grey_ramp(rgb);
    return 1;
}

/* row `i` as an entry: a bitmap's indices from its PNG (malloc'd in *owned), another entry's bytes from its file (data, then the rest as tail) */
static int flat_entry(const FileSet *in, const Row *row, uint32_t i, const uint8_t rgb[PALETTE_BYTES], ShpiDescEntry *e, uint8_t **owned,
                      ImportStats *stats, ExchangeError *err)
{
    memset(e, 0, sizeof *e);
    memcpy(e->name, row->name, 4);
    e->type = row->type, e->width = (uint16_t)row->width, e->height = (uint16_t)row->height, e->dir_index = i;
    for (int m = 0; m < 4; ++m) e->misc[m] = (int16_t)row->misc[m];
    const SetFile *f = fileset_find(in, row->file);
    if (f == NULL) { exchange_fail(err, "%s: file '%s' (entry %u) not found", entries_file, row->file, i); return 0; }
    if (!is_bitmap(row->type)) {
        const size_t data = is_palette(row->type) ? PALETTE_BYTES : 0;
        if (f->size < data) { exchange_fail(err, "%s: shorter than a palette's 768 bytes", row->file); return 0; }
        e->data = f->data, e->tail = f->data + data, e->tail_len = f->size - data;
        e->block_size = row->auto_block ? SHPI_BLOCK_AUTO : row->block_size;
        return 1;
    }
    PngImage img;
    const PngResult pr = png_read(f->data, f->size, &img);
    if (pr != PNG_OK) { exchange_fail(err, "%s: %s", row->file, png_result_str(pr)); return 0; }
    int ok = img.width == row->width && img.height == row->height;
    if (!ok) exchange_fail(err, "%s: image is %ux%u but %s says %ux%u (edit both to resize)", row->file, img.width, img.height, entries_file, row->width, row->height);
    *owned = ok ? malloc((size_t)img.width * img.height) : NULL;
    if (ok && *owned == NULL) exchange_fail(err, "out of memory"), ok = 0;
    if (ok) to_indices(&img, rgb, *owned, stats);
    png_free(img.pixels);
    e->data = *owned, e->tail = row->tail, e->tail_len = row->tail_len;
    e->block_size = row->auto_block ? (uint32_t)(ENTRY_HEADER + (size_t)row->width * row->height) : row->block_size;
    return ok;
}

int shpi_from_flat(const FileSet *in, const uint8_t *external_palette, uint8_t **out, size_t *out_len, ImportStats *stats, ExchangeError *err)
{
    *out = NULL, *out_len = 0;
    memset(stats, 0, sizeof *stats);
    ShpiHead head;
    uint8_t lead[LEAD_MAX];
    size_t lead_len = 0;
    Row *rows = NULL;
    uint32_t count = 0;
    if (!read_bank_file(in, &head, lead, &lead_len, err) || !read_rows(in, &rows, &count, err)) return 0;
    uint8_t rgb[PALETTE_BYTES];
    ShpiDescEntry *placed = calloc(count, sizeof *placed);
    uint8_t **owned = calloc(count, sizeof *owned);
    int ok = placed != NULL && owned != NULL;
    if (!ok) exchange_fail(err, "out of memory");
    ok = ok && flat_palette(in, rows, count, external_palette, rgb, err);
    for (uint32_t i = 0; ok && i < count; ++i) ok = flat_entry(in, &rows[i], i, rgb, &placed[rows[i].pos], &owned[i], stats, err);
    if (ok) placed[0].lead = lead_len > 0 ? lead : NULL, placed[0].lead_len = lead_len;
    ok = ok && shpi_build_head(&head, placed, count, out, out_len, err);
    for (uint32_t i = 0; owned != NULL && i < count; ++i) free(owned[i]);
    free(owned);
    free(placed);
    free_rows(rows, count);
    return ok;
}
