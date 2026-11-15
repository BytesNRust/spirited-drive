/* A zip writer for the tests (dep59): local headers, the central directory and its end, with whatever flags, method, sizes and attributes
 * a test wants, so the reader's refusals can be reached. Not a general writer: the data is copied as given (stored or already deflated). */
#ifndef TNFS_TESTS_ZIP_BUILD_H
#define TNFS_TESTS_ZIP_BUILD_H

#include "formats/crc32.h"

#include <stdint.h>
#include <string.h>

typedef struct {
    const char *name;
    const uint8_t *data;   /* what is written as the entry's bytes */
    uint32_t csize;        /* their count */
    uint32_t usize;        /* the size the directory gives once unpacked (0: csize) */
    uint32_t crc;          /* 0: the CRC of `data` (right for a stored entry) */
    uint16_t method, flags;
    uint8_t host;          /* the "made by" system: 3 Unix */
    uint32_t ext_attr;
} ZbEntry;

static inline void zb_16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }
static inline void zb_32(uint8_t *p, uint32_t v) { zb_16(p, v & 0xffff), zb_16(p + 2, v >> 16); }

/* The archive of `n` entries in `out`; its size, 0 when it does not fit */
static inline size_t zb_build(const ZbEntry *e, int n, uint8_t *out, size_t cap)
{
    uint32_t offsets[64];
    size_t at = 0;
    if (n > 64) return 0;
    for (int i = 0; i < n; i++) {
        const size_t len = strlen(e[i].name);
        if (at + 30 + len + e[i].csize > cap) return 0;
        offsets[i] = (uint32_t)at;
        const uint32_t crc = e[i].crc != 0 ? e[i].crc : crc32_update(0, e[i].data, e[i].csize);
        memset(out + at, 0, 30);
        zb_32(out + at, 0x04034b50), zb_16(out + at + 4, 20), zb_16(out + at + 6, e[i].flags), zb_16(out + at + 8, e[i].method);
        zb_32(out + at + 14, crc), zb_32(out + at + 18, e[i].csize), zb_32(out + at + 22, e[i].usize ? e[i].usize : e[i].csize);
        zb_16(out + at + 26, (uint32_t)len);
        memcpy(out + at + 30, e[i].name, len);
        if (e[i].csize) memcpy(out + at + 30 + len, e[i].data, e[i].csize);
        at += 30 + len + e[i].csize;
    }
    const size_t cd = at;
    for (int i = 0; i < n; i++) {
        const size_t len = strlen(e[i].name);
        if (at + 46 + len > cap) return 0;
        const uint32_t crc = e[i].crc != 0 ? e[i].crc : crc32_update(0, e[i].data, e[i].csize);
        memset(out + at, 0, 46);
        zb_32(out + at, 0x02014b50), zb_16(out + at + 4, (uint32_t)e[i].host << 8 | 20), zb_16(out + at + 6, 20);
        zb_16(out + at + 8, e[i].flags), zb_16(out + at + 10, e[i].method), zb_32(out + at + 16, crc);
        zb_32(out + at + 20, e[i].csize), zb_32(out + at + 24, e[i].usize ? e[i].usize : e[i].csize), zb_16(out + at + 28, (uint32_t)len);
        zb_32(out + at + 38, e[i].ext_attr), zb_32(out + at + 42, offsets[i]);
        memcpy(out + at + 46, e[i].name, len);
        at += 46 + len;
    }
    if (at + 22 > cap) return 0;
    memset(out + at, 0, 22);
    zb_32(out + at, 0x06054b50), zb_16(out + at + 8, (uint32_t)n), zb_16(out + at + 10, (uint32_t)n);
    zb_32(out + at + 12, (uint32_t)(at - cd)), zb_32(out + at + 16, (uint32_t)cd);
    return at + 22;
}

/* a stored entry of the text `s` */
static inline ZbEntry zb_text(const char *name, const char *s)
{
    ZbEntry e = { name, (const uint8_t *)s, (uint32_t)strlen(s), 0, 0, 0, 0, 3, 0100644u << 16 };
    return e;
}

#endif
