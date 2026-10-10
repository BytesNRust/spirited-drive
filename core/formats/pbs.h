/* PBS: a car's performance record (CARSPECS/<name>.PBS), read by 004400f4. The
 * file is a QFS stream (Huffman, 30 FB) that unpacks to 0x778 bytes; the last
 * word holds the sum of the first 0x758 bytes. Field meanings live in
 * game/race/car_specs.h; here the record is bytes, untouched. */
#ifndef TNFS_PBS_H
#define TNFS_PBS_H

#include "formats/qfs.h"

#include <stddef.h>
#include <stdint.h>

enum {
    PBS_SIZE = 0x778,
    PBS_CHECKED_BYTES = 0x758,   /* 004400f4 sums these */
    PBS_CHECKSUM_OFFSET = 0x774  /* i32 compared with the sum */
};

/* `out` receives PBS_SIZE bytes. 0 when the file is neither a PBS-sized raw
 * record nor a QFS stream unpacking to at least that. */
int pbs_read(uint8_t out[PBS_SIZE], const uint8_t *file, size_t size);

int32_t pbs_checksum(const uint8_t record[PBS_SIZE]);

/* The stored checksum equals the computed one; when it does not, 004400f4
 * zeroes the car's power. */
int pbs_checksum_ok(const uint8_t record[PBS_SIZE]);

/* Stores the checksum of the first PBS_CHECKED_BYTES bytes at PBS_CHECKSUM_OFFSET. Without it
 * 004400f4 zeroes the car's power. */
void pbs_seal(uint8_t record[PBS_SIZE]);

/* The file bytes for a record: sealed, then packed as RefPack QFS. */
QfsResult pbs_pack(const uint8_t record[PBS_SIZE], uint8_t *dst, size_t cap, size_t *len);

#endif
