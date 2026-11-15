/* A texture bank (SHPI) written from its entries, and read into them (dep59 tools B1). shpi_describe gives the entries of a bank in
 * the order the file holds them, pointing into it; shpi_build lays them out again, so a bank of the CD comes back byte for byte
 * (docs/modding/car-model.md, "A texture bank"). What the CD keeps with no known meaning travels in the entry: the header's block
 * size as stored, the bytes after the data (an 0x7c attachment or leftovers), the entry's place in the directory, the bytes before
 * its header. A new entry leaves them out: SHPI_BLOCK_AUTO, no tail, in directory order, no gap. */
#ifndef TNFS_SHPI_BUILD_H
#define TNFS_SHPI_BUILD_H

#include "formats/exchange.h"

#include <stddef.h>
#include <stdint.h>

enum { SHPI_PALETTE_BYTES = 768, SHPI_MAX_SIDE = 32767 };
#define SHPI_BLOCK_AUTO 0xffffffffu   /* 0, or 16 + the data when a tail follows it */

typedef struct {
    char name[4];              /* the FSH entry name */
    uint8_t type;              /* 0x7b a bitmap; 0x22 / 0x24 a palette */
    uint16_t width, height;
    int16_t misc[4];           /* the entry header's other words, as the CD stores them */
    const uint8_t *data;       /* width x height pixels, or 768 palette bytes */
    const uint8_t *tail;       /* what follows the data in the entry, or NULL */
    size_t tail_len;
    uint32_t block_size;       /* the header's 24-bit word as stored, or SHPI_BLOCK_AUTO */
    uint32_t dir_index;        /* its place in the directory; the file holds the entries in the array's order */
    const uint8_t *lead;       /* bytes before its header (after the directory or the previous entry's tail), or NULL */
    size_t lead_len;
} ShpiDescEntry;

/* a SHPI chunk of `n` entries in that order (malloc'd); 0 with the reason in `err`: a type other than 0x7b, 0x22 or 0x24, a bitmap
 * side of 0 or past 32767, two entries of one name, no !PAL, directory places that are not 0..n-1 once each, a block size past 24
 * bits. Every pixel index is taken (0xFA..0xFF are the race's own, docs/modding/car-model.md). */
int shpi_build(const ShpiDescEntry *entries, size_t n, uint8_t **out, size_t *out_len, ExchangeError *err);
/* the entries of an existing SHPI (its declared length `size`), pointing into it, in the file's order; 0 with the reason in `err` */
int shpi_describe(const uint8_t *shpi, size_t size, ShpiDescEntry *out, size_t cap, size_t *n, ExchangeError *err);

#endif
