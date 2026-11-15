/* An author's PNG made a bitmap of a car's bank (dep59 tools B2; docs/modding/car-model.md, "A texture bank", and the owner's
 * decision 2 of the B2 plan). The race fits the bank's own colours 0x00..0xF9 to the screen and gives 0xFA..0xFF meanings of its
 * own (0xFE the car's colour, 0xFF transparent), so an ordinary picture is fitted to 0x00..0xF9, with 0xFF where it is
 * transparent; the other special indexes come only from an indexed PNG drawn with the bank's own palette (as `unpack` writes
 * them), whose indexes are kept as they are. */
#ifndef TNFS_CAR_TEXTURE_H
#define TNFS_CAR_TEXTURE_H

#include "formats/exchange.h"
#include "formats/shpi_build.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t pixels;        /* width x height */
    uint32_t transparent;   /* alpha < 128: 0xFF */
    uint32_t moved;         /* a colour the palette does not hold, given the nearest */
    uint32_t kept_special;  /* 0xFA..0xFF kept from an indexed PNG with the bank's palette */
    uint32_t colours;       /* distinct indexes in the bitmap */
} CarTextureStats;

/* a PNG as an 8-bit bitmap of a bank whose !PAL is `pal` (768 bytes of type `pal_type`, 0x22 6-bit or 0x24 8-bit): an indexed PNG
 * with that palette keeps its indexes (0x00..0xFF); any other: alpha < 128 -> 0xFF, else the nearest of 0x00..0xF9, compared in
 * 8-bit colour whatever the type. `pixels` malloc'd; 0 with the reason in `err` (unreadable, a side of 0 or past SHPI_MAX_SIDE, a
 * palette type other than 0x22 or 0x24) */
int car_texture_from_png(const uint8_t *png, size_t len, const uint8_t pal[SHPI_PALETTE_BYTES], uint8_t pal_type,
                         uint8_t **pixels, uint16_t *width, uint16_t *height, CarTextureStats *stats, ExchangeError *err);

/* `name` (any text) as a 4-byte entry name not in `taken`: its first 4 characters, those not a letter, digit, '_' or '-' made '_',
 * padded with '_', never '!' first (the bank's own entries); a name taken ends in a number instead. The same inputs, the same
 * name. */
void car_texture_entry_name(const char *name, const char (*taken)[4], size_t n_taken, char out[4]);

#endif
