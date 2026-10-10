/* RefPack encoder (codec 0x10). Written from the stream format that 004a823c
 * decodes (see qfs_refpack.c); it makes no attempt to match EA's choices.
 *
 *   00..7F  2 bytes: 0-3 literals, copy 3..10,   offset 1..1024
 *   80..BF  3 bytes: 0-3 literals, copy 4..67,   offset 1..16384
 *   C0..DF  4 bytes: 0-3 literals, copy 5..1028, offset 1..131072
 *   E0..FB  1 byte:  4..112 literals, a multiple of 4
 *   FC..FF  1 byte:  0-3 literals, end of stream
 * Literals ahead of a copy are emitted as runs of 4..112 plus the 0-3 left,
 * which ride in the copy's own opcode. */
#include "formats/qfs.h"

#include <stdlib.h>
#include <string.h>

enum {
    HEADER_LEN = 5,
    SIZE_LIMIT = 0x1000000,  /* 24-bit size field */
    SHORT_MAX_LEN = 10, SHORT_MAX_OFF = 1024,
    MEDIUM_MAX_LEN = 67, MEDIUM_MAX_OFF = 16384,
    LONG_MAX_LEN = 1028, LONG_MAX_OFF = 131072,
    RUN_MAX = 112,
    HASH_BITS = 16,
    CHAIN_DEPTH = 48,
    NO_POS = -1
};

size_t qfs_compress_bound(size_t n)
{
    /* All literals: one opcode per 112 bytes, the stop code, the header. */
    return HEADER_LEN + n + n / RUN_MAX + 2;
}

typedef struct {
    uint8_t *dst;
    size_t cap;
    size_t len;
    int overflow;
} Out;

static void put(Out *o, uint8_t byte)
{
    if (o->len >= o->cap) { o->overflow = 1; return; }
    o->dst[o->len++] = byte;
}

static void put_literals(Out *o, const uint8_t *p, size_t n)
{
    for (size_t i = 0; i < n; ++i) put(o, p[i]);
}

/* Emits the 4..112-byte runs for `pending` literals at p and returns how many (0-3) are left. */
static size_t flush_runs(Out *o, const uint8_t *p, size_t pending)
{
    while (pending >= 4) {
        size_t run = pending > RUN_MAX ? RUN_MAX : pending & ~(size_t)3;
        put(o, (uint8_t)(0xE0 + (run - 4) / 4));
        put_literals(o, p, run);
        p += run;
        pending -= run;
    }
    return pending;
}

static void put_copy(Out *o, const uint8_t *lit, size_t lits, size_t len, size_t off)
{
    const size_t d = off - 1;
    if (len <= SHORT_MAX_LEN && off <= SHORT_MAX_OFF) {
        put(o, (uint8_t)(((d >> 8) << 5) | ((len - 3) << 2) | lits));
        put(o, (uint8_t)d);
    } else if (len >= 4 && len <= MEDIUM_MAX_LEN && off <= MEDIUM_MAX_OFF) {
        put(o, (uint8_t)(0x80 | (len - 4)));
        put(o, (uint8_t)((lits << 6) | (d >> 8)));
        put(o, (uint8_t)d);
    } else {
        put(o, (uint8_t)(0xC0 | ((d >> 16) << 4) | (((len - 5) >> 8) << 2) | lits));
        put(o, (uint8_t)(d >> 8));
        put(o, (uint8_t)d);
        put(o, (uint8_t)(len - 5));
    }
    put_literals(o, lit, lits);
}

static uint32_t hash3(const uint8_t *p)
{
    return (((uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2]) * 2654435761u) >> (32 - HASH_BITS);
}

/* Smallest length a copy at this offset must have to beat literals and fit a form. */
static size_t min_len(size_t off)
{
    if (off <= SHORT_MAX_OFF) return 3;
    if (off <= MEDIUM_MAX_OFF) return 4;
    return 5;
}

static size_t match_len(const uint8_t *src, size_t pos, size_t from, size_t limit)
{
    size_t l = 0;
    while (l < limit && src[from + l] == src[pos + l]) ++l;
    return l;
}

/* Longest match at pos; 0 when none worth a copy. */
static size_t find_match(const uint8_t *src, size_t n, size_t pos, const int32_t *head, const int32_t *prev, size_t *best_off)
{
    size_t best = 0;
    if (pos + 3 > n) return 0;
    const size_t limit = n - pos < LONG_MAX_LEN ? n - pos : LONG_MAX_LEN;
    int32_t cand = head[hash3(src + pos)];
    for (int depth = 0; cand != NO_POS && depth < CHAIN_DEPTH; ++depth, cand = prev[cand]) {
        const size_t off = pos - (size_t)cand;
        if (off > LONG_MAX_OFF) break;
        const size_t l = match_len(src, pos, (size_t)cand, limit);
        if (l >= min_len(off) && l > best) {
            best = l;
            *best_off = off;
            if (best == limit) break;
        }
    }
    return best;
}

static void insert(const uint8_t *src, size_t n, size_t pos, int32_t *head, int32_t *prev)
{
    if (pos + 3 > n) return;
    const uint32_t h = hash3(src + pos);
    prev[pos] = head[h];
    head[h] = (int32_t)pos;
}

QfsResult qfs_compress(const uint8_t *src, size_t n, uint8_t *dst, size_t dst_cap, size_t *out_len)
{
    if (n >= SIZE_LIMIT) return QFS_ERR_OVERFLOW;
    int32_t *head = malloc(((size_t)1 << HASH_BITS) * sizeof *head);
    int32_t *prev = malloc((n ? n : 1) * sizeof *prev);
    if (head == NULL || prev == NULL) { free(head); free(prev); return QFS_ERR_MEMORY; }
    for (size_t i = 0; i < ((size_t)1 << HASH_BITS); ++i) head[i] = NO_POS;

    Out o = { dst, dst_cap, 0, 0 };
    put(&o, 0x10);
    put(&o, 0xFB);
    put(&o, (uint8_t)(n >> 16));
    put(&o, (uint8_t)(n >> 8));
    put(&o, (uint8_t)n);

    size_t pos = 0, lit_start = 0;
    while (pos < n) {
        size_t off = 0;
        const size_t len = find_match(src, n, pos, head, prev, &off);
        if (len == 0) {
            insert(src, n, pos, head, prev);
            ++pos;
            continue;
        }
        const size_t left = flush_runs(&o, src + lit_start, pos - lit_start);
        put_copy(&o, src + pos - left, left, len, off);
        for (size_t k = 0; k < len; ++k) insert(src, n, pos + k, head, prev);
        pos += len;
        lit_start = pos;
    }
    const size_t left = flush_runs(&o, src + lit_start, n - lit_start);
    put(&o, (uint8_t)(0xFC | left));
    put_literals(&o, src + n - left, left);

    free(head);
    free(prev);
    if (o.overflow) return QFS_ERR_OVERFLOW;
    *out_len = o.len;
    return QFS_OK;
}
