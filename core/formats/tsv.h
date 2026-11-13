/* Tab-separated tables: the exchange format for specs and metadata (specs, node
 * lists, sample headers, entry directories). One row per line, cells split at
 * tabs; blank lines and lines starting with '#' are skipped; CRLF is accepted.
 * Strings escape \t, \n and \\ ; byte strings are lower-case hex ("" for none);
 * integers are decimal or 0x hex. A reader reports the line of a bad cell so a
 * hand-edited file can be fixed. */
#ifndef TNFS_TSV_H
#define TNFS_TSV_H

#include <stddef.h>
#include <stdint.h>

enum { TSV_MAX_CELLS = 256 };

typedef enum {
    TSV_OK = 0,
    TSV_END,           /* no more rows */
    TSV_ERR_MISSING,   /* cell index past the row */
    TSV_ERR_NUMBER,
    TSV_ERR_HEX,
    TSV_ERR_CAPACITY,  /* decoded bytes do not fit */
    TSV_ERR_ROW        /* more than TSV_MAX_CELLS cells */
} TsvResult;

typedef struct {
    uint8_t *data;
    size_t len, cap;
    int failed;        /* an allocation failed; the output is incomplete */
    int in_row;        /* cells written since the last tsv_row */
} TsvWriter;

void tsv_writer_init(TsvWriter *w);
void tsv_writer_free(TsvWriter *w);
void tsv_comment(TsvWriter *w, const char *text);   /* one '#' line */
void tsv_str(TsvWriter *w, const char *s);
void tsv_int(TsvWriter *w, int32_t v);
void tsv_uint(TsvWriter *w, uint32_t v);
void tsv_hex(TsvWriter *w, const uint8_t *bytes, size_t n);
void tsv_row(TsvWriter *w);                          /* ends the line */

typedef struct {
    const uint8_t *data;
    size_t len, pos;
    uint32_t line;     /* 1-based line of the next unread byte */
    char *scratch;     /* unescaped copy of the current row */
    size_t scratch_cap;
} TsvReader;

typedef struct {
    uint32_t line;
    int cells;
    const char *cell[TSV_MAX_CELLS];   /* NUL-terminated, valid until the next tsv_next */
} TsvRow;

void tsv_reader_init(TsvReader *r, const uint8_t *data, size_t len);
void tsv_reader_free(TsvReader *r);
TsvResult tsv_next(TsvReader *r, TsvRow *row);

TsvResult tsv_get_int(const TsvRow *row, int index, int32_t *out);
TsvResult tsv_get_uint(const TsvRow *row, int index, uint32_t *out);
TsvResult tsv_get_hex(const TsvRow *row, int index, uint8_t *out, size_t cap, size_t *n);

#endif
