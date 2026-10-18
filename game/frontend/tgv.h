/* The TGV movies' decoder (0048e8b4 and the calls under it): 'kVGT' key frames and 'fVGT' frames into 8-bit
 * shapes. */
#ifndef TNFS_TGV_H
#define TNFS_TGV_H

#include <stdint.h>

/* 004a2b90 (cdecl bits): the shape type (its first byte) for pixels of that many bits: 1 0x79, 4 0x7a, 8 0x7b,
 * 15 0x7e, 16 0x78, 24 0x7f, 32 0x7d; 0 for any other. */
uint32_t tgv_shape_type(uint32_t bits);

#endif
