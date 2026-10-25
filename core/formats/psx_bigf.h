/* port: the PS1 car files as the CD carries them (Z<CAR>.VSM and Z<CAR>.WSM, read at runtime from the user's own
 * extraction; nothing of them is in the repo). A BIGF archive (big-endian) holds a SHPP texture directory (<car>.psh);
 * each texture is one 4bpp image through its own 16-entry BGR555 CLUT. Read only: the colours a texture uses are
 * counted, which is all the livery mapping needs (car_livery.h). */
#ifndef TNFS_PSX_BIGF_H
#define TNFS_PSX_BIGF_H

#include <stddef.h>
#include <stdint.h>

enum { PSX_COLOURS = 0x8000 };   /* BGR555 without the STP bit: 5 bits a channel */

/* The member `name` of a BIGF archive: its bytes within `file`. 0 when the archive does not parse or has no such member. */
int psx_bigf_member(const uint8_t *file, size_t size, const char *name, const uint8_t **data, size_t *length);

/* Adds to counts[c] the number of pixels of the textures in the SHPP texture directory `file` that show colour c, over
 * every texture but the one named "shad" (its shadow is not body). counts holds PSX_COLOURS entries. 0 when the
 * directory does not parse or has no 4bpp texture; textures that are not 4bpp are skipped. */
int psx_shpp_colours(const uint8_t *file, size_t size, uint32_t counts[PSX_COLOURS]);

#endif
