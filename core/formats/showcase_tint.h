/* port (dep59 step 5, T7d): the one tint of a showcase page's background. A track's page takes it from mod.ini's showcase_tint=#RRGGBB
 * (the sepia of the CD's pages when the key is not there); a car's page, its main colour. Pure: no SDL, no files. */
#ifndef TNFS_SHOWCASE_TINT_H
#define TNFS_SHOWCASE_TINT_H

#include <stddef.h>
#include <stdint.h>

/* the sepia of the pages, as RGB: what the background takes when the mod gives no tint */
extern const uint8_t SHOWCASE_TINT_SEPIA[3];

/* `text` exactly "#RRGGBB" (hex digits either case, nothing before or after) read into `rgb`: 1, or 0 and `rgb` untouched */
int showcase_tint_parse(const char *text, uint8_t rgb[3]);

/* the first `entries` RGB triples of `rgb` (8 bits a channel, the rest of the buffer left alone) turned to `tint` with each entry's
 * brightness kept (a channel that would pass 255 stops there); a black tint gives greys. Deterministic; NULL or no entries do nothing. */
void showcase_tint_apply(uint8_t *rgb, size_t entries, const uint8_t tint[3]);

#endif
