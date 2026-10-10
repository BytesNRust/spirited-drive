#include "formats/tsv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void reserve(TsvWriter *w, size_t extra)
{
    if (w->failed || w->len + extra <= w->cap) return;
    size_t cap = w->cap ? w->cap : 256;
    while (cap < w->len + extra) cap *= 2;
    uint8_t *grown = realloc(w->data, cap);
    if (grown == NULL) { w->failed = 1; return; }
    w->data = grown;
    w->cap = cap;
}

static void put(TsvWriter *w, const char *s, size_t n)
{
    reserve(w, n);
    if (w->failed) return;
    memcpy(w->data + w->len, s, n);
    w->len += n;
}

static void separator(TsvWriter *w)
{
    if (w->in_row) put(w, "\t", 1);
    w->in_row = 1;
}

void tsv_writer_init(TsvWriter *w)
{
    memset(w, 0, sizeof *w);
}

void tsv_writer_free(TsvWriter *w)
{
    free(w->data);
    memset(w, 0, sizeof *w);
}

void tsv_comment(TsvWriter *w, const char *text)
{
    put(w, "# ", 2);
    put(w, text, strlen(text));
    put(w, "\n", 1);
}

void tsv_str(TsvWriter *w, const char *s)
{
    separator(w);
    for (; *s; ++s) {
        if (*s == '\t') put(w, "\\t", 2);
        else if (*s == '\n') put(w, "\\n", 2);
        else if (*s == '\\') put(w, "\\\\", 2);
        else put(w, s, 1);
    }
}

void tsv_int(TsvWriter *w, int32_t v)
{
    char buf[16];
    separator(w);
    put(w, buf, (size_t)snprintf(buf, sizeof buf, "%d", v));
}

void tsv_uint(TsvWriter *w, uint32_t v)
{
    char buf[16];
    separator(w);
    put(w, buf, (size_t)snprintf(buf, sizeof buf, "%u", v));
}

void tsv_hex(TsvWriter *w, const uint8_t *bytes, size_t n)
{
    static const char digits[] = "0123456789abcdef";
    separator(w);
    reserve(w, 2 * n);
    if (w->failed) return;
    for (size_t i = 0; i < n; ++i) {
        w->data[w->len++] = (uint8_t)digits[bytes[i] >> 4];
        w->data[w->len++] = (uint8_t)digits[bytes[i] & 15];
    }
}

void tsv_row(TsvWriter *w)
{
    put(w, "\n", 1);
    w->in_row = 0;
}

void tsv_reader_init(TsvReader *r, const uint8_t *data, size_t len)
{
    memset(r, 0, sizeof *r);
    r->data = data;
    r->len = len;
    r->line = 1;
}

void tsv_reader_free(TsvReader *r)
{
    free(r->scratch);
    r->scratch = NULL;
}

static int blank(const uint8_t *p, size_t n)
{
    for (size_t i = 0; i < n; ++i)
        if (p[i] != ' ' && p[i] != '\r') return 0;
    return 1;
}

TsvResult tsv_next(TsvReader *r, TsvRow *row)
{
    while (r->pos < r->len) {
        const uint8_t *start = r->data + r->pos;
        const uint8_t *nl = memchr(start, '\n', r->len - r->pos);
        const size_t n = nl ? (size_t)(nl - start) : r->len - r->pos;
        const uint32_t line = r->line;
        r->pos += n + (nl ? 1 : 0);
        r->line++;
        if (n == 0 || start[0] == '#' || blank(start, n)) continue;

        size_t end = n;
        if (start[end - 1] == '\r') --end;
        if (r->scratch_cap < end + 1) {
            char *grown = realloc(r->scratch, end + 1);
            if (grown == NULL) return TSV_ERR_ROW;
            r->scratch = grown;
            r->scratch_cap = end + 1;
        }
        char *out = r->scratch;
        row->line = line;
        row->cells = 1;
        row->cell[0] = out;
        for (size_t i = 0; i < end; ++i) {
            char c = (char)start[i];
            if (c == '\t') {
                *out++ = '\0';
                if (row->cells == TSV_MAX_CELLS) return TSV_ERR_ROW;
                row->cell[row->cells++] = out;
                continue;
            }
            if (c == '\\' && i + 1 < end) {
                const char e = (char)start[i + 1];
                if (e == 't') { c = '\t'; ++i; }
                else if (e == 'n') { c = '\n'; ++i; }
                else if (e == '\\') { c = '\\'; ++i; }
            }
            *out++ = c;
        }
        *out = '\0';
        return TSV_OK;
    }
    return TSV_END;
}

static TsvResult number(const TsvRow *row, int index, int64_t *out)
{
    if (index < 0 || index >= row->cells) return TSV_ERR_MISSING;
    const char *s = row->cell[index];
    int negative = 0;
    if (*s == '-') { negative = 1; ++s; }
    int base = 10;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; s += 2; }
    if (*s == '\0') return TSV_ERR_NUMBER;
    int64_t v = 0;
    for (; *s; ++s) {
        int d;
        if (*s >= '0' && *s <= '9') d = *s - '0';
        else if (base == 16 && *s >= 'a' && *s <= 'f') d = *s - 'a' + 10;
        else if (base == 16 && *s >= 'A' && *s <= 'F') d = *s - 'A' + 10;
        else return TSV_ERR_NUMBER;
        if (d >= base) return TSV_ERR_NUMBER;
        v = v * base + d;
        if (v > 0xFFFFFFFFLL) return TSV_ERR_NUMBER;
    }
    *out = negative ? -v : v;
    return TSV_OK;
}

TsvResult tsv_get_int(const TsvRow *row, int index, int32_t *out)
{
    int64_t v;
    const TsvResult r = number(row, index, &v);
    if (r != TSV_OK) return r;
    if (v < INT32_MIN || v > INT32_MAX) return TSV_ERR_NUMBER;
    *out = (int32_t)v;
    return TSV_OK;
}

TsvResult tsv_get_uint(const TsvRow *row, int index, uint32_t *out)
{
    int64_t v;
    const TsvResult r = number(row, index, &v);
    if (r != TSV_OK) return r;
    if (v < 0) return TSV_ERR_NUMBER;
    *out = (uint32_t)v;
    return TSV_OK;
}

static int nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

TsvResult tsv_get_hex(const TsvRow *row, int index, uint8_t *out, size_t cap, size_t *n)
{
    if (index < 0 || index >= row->cells) return TSV_ERR_MISSING;
    const char *s = row->cell[index];
    const size_t len = strlen(s);
    if (len % 2) return TSV_ERR_HEX;
    for (size_t i = 0; i < len; ++i)
        if (nibble(s[i]) < 0) return TSV_ERR_HEX;
    if (len / 2 > cap) return TSV_ERR_CAPACITY;
    for (size_t i = 0; i < len / 2; ++i) out[i] = (uint8_t)(nibble(s[2 * i]) << 4 | nibble(s[2 * i + 1]));
    *n = len / 2;
    return TSV_OK;
}

const char *tsv_result_str(TsvResult result)
{
    switch (result) {
    case TSV_OK: return "ok";
    case TSV_END: return "end of table";
    case TSV_ERR_MISSING: return "missing cell";
    case TSV_ERR_NUMBER: return "not a 32-bit number";
    case TSV_ERR_HEX: return "not a hex byte string";
    case TSV_ERR_CAPACITY: return "too many bytes";
    case TSV_ERR_ROW: return "row too long or out of memory";
    }
    return "unknown error";
}
