/* A fixed-size record stored either raw or QFS-packed (the PBS files in
 * CARSPECS, the PDN files in CARFAMS). Shared by pbs.c and pdn.c. */
#ifndef TNFS_FIXED_RECORD_H
#define TNFS_FIXED_RECORD_H

#include <stddef.h>
#include <stdint.h>

/* Copies the first `record_size` bytes of the file, unpacking it first when it
 * is a QFS stream. 0 when the file is shorter than the record or does not
 * unpack. */
int fixed_record_read(uint8_t *out, size_t record_size, const uint8_t *file, size_t size);

#endif
