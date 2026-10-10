/* A fixed-size record stored either raw or QFS-packed (the PBS files in
 * CARSPECS, the PDN files in CARFAMS). Shared by pbs.c and pdn.c. */
#ifndef TNFS_FIXED_RECORD_H
#define TNFS_FIXED_RECORD_H

#include "formats/qfs.h"

#include <stddef.h>
#include <stdint.h>

/* Copies the first `record_size` bytes of the file, unpacking it first when it
 * is a QFS stream. 0 when the file is shorter than the record or does not
 * unpack. */
int fixed_record_read(uint8_t *out, size_t record_size, const uint8_t *file, size_t size);

/* Packs a record as a RefPack QFS stream the game can unpack in place (004a2520), checking
 * it with qfs_inplace_ok. QFS_OK and *len, or an error (QFS_ERR_OVERFLOW also when the
 * stream would not unpack in place). */
QfsResult fixed_record_pack(const uint8_t *record, size_t size, uint8_t *dst, size_t cap, size_t *len);

#endif
