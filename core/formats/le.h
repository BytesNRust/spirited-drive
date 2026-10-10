/* Bounds-free little-endian loads; callers check the range first. */
#ifndef TNFS_LE_H
#define TNFS_LE_H

#include <stdint.h>

static inline uint32_t le_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

#endif
