/* See shpi_build.h. */
#include "formats/shpi_build.h"

#include <stdlib.h>
#include <string.h>

enum { HEADER = 16, DIR_ENTRY = 8, ENTRY_HEADER = 16, BITMAP = 0x7b, PALETTE_6BIT = 0x22, PALETTE_8BIT = 0x24, BLOCK_LIMIT = 1 << 24 };

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24); }
static void put16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }

static int is_palette(uint8_t type) { return type == PALETTE_6BIT || type == PALETTE_8BIT; }

/* the bytes of an entry's data: a palette's 768, a bitmap's pixels, nothing for another type */
static size_t data_size(uint8_t type, uint16_t width, uint16_t height)
{
    if (is_palette(type)) return SHPI_PALETTE_BYTES;
    return type == BITMAP ? (size_t)width * height : 0;
}

static void shown(char out[5], const char name[4])
{
    memcpy(out, name, 4);
    out[4] = '\0';
}

/* ---- describe */

int shpi_describe(const uint8_t *b, size_t size, ShpiDescEntry *out, size_t cap, size_t *n_out, ExchangeError *err)
{
    *n_out = 0;
    if (size < HEADER || memcmp(b, "SHPI", 4) != 0) { exchange_fail(err, "not a SHPI bank"); return 0; }
    const uint32_t n = le32(b + 8);
    const uint64_t directory_end = HEADER + (uint64_t)DIR_ENTRY * n;
    if (n > cap || directory_end > size) { exchange_fail(err, "a bank of %u entries does not fit", n); return 0; }
    /* the file's order: by offset, the directory's order among equals */
    for (uint32_t i = 0; i < n; i++) {
        size_t at = i;
        const uint32_t offset = le32(b + HEADER + DIR_ENTRY * i + 4);
        while (at > 0 && le32(b + HEADER + DIR_ENTRY * out[at - 1].dir_index + 4) > offset) out[at] = out[at - 1], at--;
        memset(&out[at], 0, sizeof out[at]);
        out[at].dir_index = i;
    }
    uint64_t previous_end = directory_end;
    for (uint32_t k = 0; k < n; k++) {
        ShpiDescEntry *e = &out[k];
        const uint8_t *d = b + HEADER + DIR_ENTRY * e->dir_index;
        const uint32_t at = le32(d + 4);
        const uint64_t end = k + 1 < n ? le32(b + HEADER + DIR_ENTRY * out[k + 1].dir_index + 4) : size;
        if (at < previous_end || at + (uint64_t)ENTRY_HEADER > end) { exchange_fail(err, "entry %u lies outside its place", e->dir_index); return 0; }
        const uint8_t *h = b + at;
        memcpy(e->name, d, 4);
        e->type = h[0], e->block_size = h[1] | (uint32_t)h[2] << 8 | (uint32_t)h[3] << 16;
        e->width = le16(h + 4), e->height = le16(h + 6);
        for (int i = 0; i < 4; i++) e->misc[i] = (int16_t)le16(h + 8 + 2 * i);
        const size_t data = data_size(e->type, e->width, e->height);
        if (at + (uint64_t)ENTRY_HEADER + data > end) {
            char name[5];
            shown(name, e->name);
            exchange_fail(err, "entry '%s''s data runs past its place", name);
            return 0;
        }
        e->data = h + ENTRY_HEADER;
        e->tail_len = (size_t)(end - at - ENTRY_HEADER - data);
        e->tail = e->tail_len > 0 ? e->data + data : NULL;
        e->lead_len = (size_t)(at - previous_end);
        e->lead = e->lead_len > 0 ? b + previous_end : NULL;
        previous_end = end;
    }
    *n_out = n;
    return 1;
}

/* ---- build */

static int check_entry(const ShpiDescEntry *e, ExchangeError *err)
{
    char name[5];
    shown(name, e->name);
    if (e->type != BITMAP && !is_palette(e->type)) { exchange_fail(err, "entry '%s' is of type 0x%02x (0x7b, 0x22 or 0x24)", name, e->type); return 0; }
    if (e->type == BITMAP && (e->width == 0 || e->height == 0 || e->width > SHPI_MAX_SIDE || e->height > SHPI_MAX_SIDE)) {
        exchange_fail(err, "entry '%s' is %u x %u: a side is 1 to %d", name, e->width, e->height, SHPI_MAX_SIDE);
        return 0;
    }
    if (e->data == NULL) { exchange_fail(err, "entry '%s' has no data", name); return 0; }
    if (e->block_size != SHPI_BLOCK_AUTO && e->block_size >= BLOCK_LIMIT) {
        exchange_fail(err, "entry '%s''s block size 0x%x does not fit 24 bits", name, e->block_size);
        return 0;
    }
    return 1;
}

static int check(const ShpiDescEntry *e, size_t n, ExchangeError *err)
{
    if (n == 0) { exchange_fail(err, "a bank needs an entry"); return 0; }
    uint8_t *placed = calloc(n, 1);
    if (placed == NULL) { exchange_fail(err, "out of memory"); return 0; }
    int ok = 1, pal = 0;
    for (size_t i = 0; ok && i < n; i++) {
        char name[5];
        shown(name, e[i].name);
        ok = check_entry(&e[i], err);
        for (size_t j = 0; ok && j < i; j++)
            if (memcmp(e[j].name, e[i].name, 4) == 0) exchange_fail(err, "two entries are named '%s'", name), ok = 0;
        if (ok && (e[i].dir_index >= n || placed[e[i].dir_index]++))
            exchange_fail(err, "entry '%s''s directory place %u is taken or past the %zu entries", name, e[i].dir_index, n), ok = 0;
        pal |= memcmp(e[i].name, "!PAL", 4) == 0 || memcmp(e[i].name, "!pal", 4) == 0;   /* a car's bank, a menu's (RUSTSEL.QFS) */
    }
    free(placed);
    if (ok && !pal) exchange_fail(err, "the bank has no !PAL entry"), ok = 0;
    return ok;
}

int shpi_build(const ShpiDescEntry *e, size_t n, uint8_t **out, size_t *out_len, ExchangeError *err)
{
    *out = NULL, *out_len = 0;
    if (!check(e, n, err)) return 0;
    uint64_t total = HEADER + (uint64_t)DIR_ENTRY * n;
    for (size_t i = 0; i < n; i++) total += e[i].lead_len + ENTRY_HEADER + data_size(e[i].type, e[i].width, e[i].height) + e[i].tail_len;
    if (total > UINT32_MAX) { exchange_fail(err, "the bank would pass 4 GB"); return 0; }
    uint8_t *b = calloc((size_t)total, 1);
    if (b == NULL) { exchange_fail(err, "out of memory"); return 0; }
    memcpy(b, "SHPI", 4), put32(b + 4, (uint32_t)total), put32(b + 8, (uint32_t)n), memcpy(b + 12, "WRAP", 4);
    size_t at = HEADER + (size_t)DIR_ENTRY * n;
    for (size_t i = 0; i < n; i++) {
        const ShpiDescEntry *x = &e[i];
        const size_t data = data_size(x->type, x->width, x->height);
        if (x->lead_len > 0) memcpy(b + at, x->lead, x->lead_len);
        at += x->lead_len;
        uint8_t *dir = b + HEADER + DIR_ENTRY * x->dir_index;
        memcpy(dir, x->name, 4), put32(dir + 4, (uint32_t)at);
        const uint32_t block = x->block_size != SHPI_BLOCK_AUTO ? x->block_size : x->tail_len > 0 ? (uint32_t)(ENTRY_HEADER + data) : 0;
        uint8_t *h = b + at;
        h[0] = x->type, h[1] = (uint8_t)block, h[2] = (uint8_t)(block >> 8), h[3] = (uint8_t)(block >> 16);
        put16(h + 4, x->width), put16(h + 6, x->height);
        for (int k = 0; k < 4; k++) put16(h + 8 + 2 * k, (uint16_t)x->misc[k]);
        memcpy(h + ENTRY_HEADER, x->data, data);
        if (x->tail_len > 0) memcpy(h + ENTRY_HEADER + data, x->tail, x->tail_len);
        at += ENTRY_HEADER + data + x->tail_len;
    }
    *out = b, *out_len = (size_t)total;
    return 1;
}
