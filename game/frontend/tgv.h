/* The TGV movies' decoder (0048e8b4 and the calls under it): 'kVGT' key frames and 'fVGT' frames into 8-bit
 * shapes. */
#ifndef TNFS_TGV_H
#define TNFS_TGV_H

#include <stdint.h>

/* 004a2b90 (cdecl bits): the shape type (its first byte) for pixels of that many bits: 1 0x79, 4 0x7a, 8 0x7b,
 * 15 0x7e, 16 0x78, 24 0x7f, 32 0x7d; 0 for any other. */
uint32_t tgv_shape_type(uint32_t bits);

/* 0048e1dc (cdecl src, dst, blocks): each 8-byte block of src (4 colours, then a little-endian word of sixteen
 * 2-bit indices, the top two bits first) into 16 bytes of dst, each its index's colour. */
void tgv_expand_blocks(const uint8_t *src, uint8_t *dst, int32_t blocks);

/* 0048e120 (eax out, edx bits, ebx count, ecx width): count signed fields of `width` bits packed from bit 0 of `bits`
 * (field i at bit i * width, read as a little-endian dword from its byte) into out's dwords, the last first. The
 * shifts as x86's (by the width mod 32). */
void tgv_unpack_fields(int32_t *out, const uint8_t *bits, int32_t count, int32_t width);

/* 004a2a2c (cdecl out, bits, count, width): count unsigned fields of `width` bits into out's dwords, in fours: the
 * dword holding a four's first field is rotated right to it, masked, then rotated right by the width again for
 * the second (bits past the dword come back from its low end); the third and fourth the same from the third's
 * dword; the last count mod 4 one by one. Shifts and rotations by the width mod 32. */
void tgv_unpack_codes(int32_t *out, const uint8_t *bits, int32_t count, int32_t width);

#endif
