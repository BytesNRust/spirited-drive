/* The CRC-32 of PNG chunks and zip entries (IEEE 802.3, reflected 0xEDB88320): one implementation for both readers. */
#ifndef TNFS_CRC32_H
#define TNFS_CRC32_H

#include <stddef.h>
#include <stdint.h>

/* `crc` (0 to start) carried over `n` more bytes; the result is the CRC of everything so far. */
uint32_t crc32_update(uint32_t crc, const uint8_t *p, size_t n);

#endif
