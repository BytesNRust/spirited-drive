/* port: SHA-256 (FIPS 180-4), for the first-run wizard's check of the CD's key files (data_check.h). */
#ifndef TNFS_SHA256_H
#define TNFS_SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t h[8];
    uint64_t bytes;
    uint8_t block[64];
    size_t used;
} Sha256;

void sha256_init(Sha256 *s);
void sha256_update(Sha256 *s, const void *data, size_t len);
/* the digest as 64 lower-case hex digits and a NUL */
void sha256_hex(Sha256 *s, char out[65]);

#endif
