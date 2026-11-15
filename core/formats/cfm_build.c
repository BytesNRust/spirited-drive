/* See cfm_build.h. */
#include "formats/cfm_build.h"

#include "formats/le.h"

#include <stdlib.h>
#include <string.h>

enum { TABLE_END = 8 + 4 * CFM_CHILDREN, LEAF_HEADER = 8 };

static const char *const TAG[CFM_CHILDREN] = { "ORIP", "SHPI", "ORIP", "SHPI" };

static size_t padded(size_t n) { return (n + 3) & ~(size_t)3; }

static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24); }

static int check_child(int i, const uint8_t *child, size_t len, ExchangeError *err)
{
    if (child == NULL || len < LEAF_HEADER) { exchange_fail(err, "child %d has %zu bytes: a chunk is 8 at least", i, child == NULL ? 0 : len); return 0; }
    if (memcmp(child, TAG[i], 4) != 0) { exchange_fail(err, "child %d is not an %s chunk", i, TAG[i]); return 0; }
    const uint32_t declared = le_u32(child + 4);
    if (declared < LEAF_HEADER || declared > len) {
        exchange_fail(err, "child %d declares %u bytes and has %zu", i, declared, len);
        return 0;
    }
    return 1;
}

int cfm_build(const uint8_t *const child[CFM_CHILDREN], const size_t len[CFM_CHILDREN], uint8_t **out, size_t *out_len, ExchangeError *err)
{
    *out = NULL, *out_len = 0;
    uint64_t total = TABLE_END;
    for (int i = 0; i < CFM_CHILDREN; i++) {
        if (!check_child(i, child[i], len[i], err)) return 0;
        total += padded(len[i]);
    }
    if (total > UINT32_MAX) { exchange_fail(err, "the model would pass 4 GB"); return 0; }
    uint8_t *b = calloc((size_t)total, 1);
    if (b == NULL) { exchange_fail(err, "out of memory"); return 0; }
    memcpy(b, "wwww", 4), put32(b + 4, CFM_CHILDREN);
    size_t at = TABLE_END;
    for (int i = 0; i < CFM_CHILDREN; i++) {
        put32(b + 8 + 4 * i, (uint32_t)at);
        memcpy(b + at, child[i], len[i]);
        at += padded(len[i]);
    }
    *out = b, *out_len = (size_t)total;
    return 1;
}

int cfm_describe(const uint8_t *cfm, size_t size, const uint8_t *child[CFM_CHILDREN], size_t len[CFM_CHILDREN], ExchangeError *err)
{
    if (size < TABLE_END || memcmp(cfm, "wwww", 4) != 0 || le_u32(cfm + 4) != CFM_CHILDREN) {
        exchange_fail(err, "not a car model: a wwww table of 4 children");
        return 0;
    }
    for (int i = 0; i < CFM_CHILDREN; i++) {
        const uint32_t at = le_u32(cfm + 8 + 4 * i);
        uint32_t end = (uint32_t)size;
        for (int j = 0; j < CFM_CHILDREN; j++) {
            const uint32_t other = le_u32(cfm + 8 + 4 * j);
            if (other > at && other < end) end = other;
        }
        if (at < TABLE_END || at >= size) { exchange_fail(err, "child %d at 0x%x lies outside the file", i, at); return 0; }
        child[i] = cfm + at, len[i] = end - at;
        if (!check_child(i, child[i], len[i], err)) return 0;
    }
    return 1;
}
