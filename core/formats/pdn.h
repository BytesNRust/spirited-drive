/* PDN: a car's body and AI record (CARFAMS/<name>.PDN), read by 0040fa3c and
 * 004407b8 from 00517580[slot]. QFS-packed, 0x1cc bytes unpacked. Only the
 * words the loaders copy are named; every other byte is opaque and a writer
 * must keep it. */
#ifndef TNFS_PDN_H
#define TNFS_PDN_H

#include "formats/qfs.h"

#include <stddef.h>
#include <stdint.h>

enum {
    PDN_SIZE = 0x1cc,
    PDN_MASS = 0x14,           /* i32, car +0x168 (0040fa3c) */
    PDN_AI_SPEEDS = 0x1c,      /* PDN_AI_SPEED_COUNT x i32, car +0x178 (0046f3e8) */
    PDN_AI_SPEED_COUNT = 100,
    PDN_FACTORS = 0x1ac        /* 6 x i32, car +0x308 */
};

/* `out` receives PDN_SIZE bytes. 0 when the file is neither a PDN-sized raw
 * record nor a QFS stream unpacking to at least that. */
int pdn_read(uint8_t out[PDN_SIZE], const uint8_t *file, size_t size);

/* The little-endian word at `offset` (a multiple of 4, below PDN_SIZE - 3). */
int32_t pdn_i32(const uint8_t record[PDN_SIZE], int offset);

/* The file bytes for a record, packed as RefPack QFS. */
QfsResult pdn_pack(const uint8_t record[PDN_SIZE], uint8_t *dst, size_t cap, size_t *len);

#endif
