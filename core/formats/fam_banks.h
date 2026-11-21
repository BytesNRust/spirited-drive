/* What a track's art file (FAM) holds, read back: its terrain banks (formats/fam_build.h: bank n fills the slots n*3 + L, L the letters A, B and C) and the
 * pictures in them, for the editor to list and to show as thumbnails (dep96 step 1, read only). Every reader is bounds-checked by chunk.h and fsh.h; nothing is
 * written and nothing kept: the FAM bytes must outlive the call. */
#ifndef TNFS_FORMATS_FAM_BANKS_H
#define TNFS_FORMATS_FAM_BANKS_H

#include "formats/exchange.h"
#include "formats/fam_spec.h"   /* FAM_LETTER_* */

#include <stddef.h>
#include <stdint.h>

/* the terrain banks of the FAM; 0 with `err` set when it is not a track art file */
int fam_terrain_banks(const uint8_t *fam, size_t size, uint32_t *count, ExchangeError *err);

typedef struct {
    unsigned letters;               /* the FAM_LETTER_ bits the bank has (its first variant "?000" is there) */
    int width[3], height[3];   /* per letter A, B, C: the first variant's size, 0 where the letter is missing */
} FamBankInfo;

/* bank `bank`'s letters and sizes; 0 with `err` set for a bank that is not there or is no picture bank */
int fam_bank_info(const uint8_t *fam, size_t size, uint32_t bank, FamBankInfo *out, ExchangeError *err);

/* the first variant of letter `letter` ('A', 'B' or 'C') of bank `bank` as RGBA (width x height x 4, malloc'd, free with free()) through the bank's own palette
 * ("!pal"); a see-through pixel (index 0xFF) has alpha 0. 0 with `err` set when the bank, the letter or the palette is missing. */
int fam_bank_rgba(const uint8_t *fam, size_t size, uint32_t bank, char letter, uint8_t **rgba, int *width, int *height, ExchangeError *err);

#endif
