/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: the boot screen is composed in RGB (the relief, the banner with its soft shadow) and shown through the front end's one 256-colour palette, so it
 * is quantised once: median cut over a 5-bit-per-channel histogram (the box with the most pixels split at its median along its widest channel), each
 * colour the mean of its box, each pixel the nearest colour. The text's colours are not in it: the caller keeps the palette's last entries for them. */
#ifndef TNFS_BOOT_QUANT_H
#define TNFS_BOOT_QUANT_H

#include <stdint.h>

/* rgb: n pixels; colours: 1..256. Writes palette[0 .. 3 * colours) and each pixel's index (below colours). Returns how many colours it made (fewer when
 * the picture has fewer; the rest of those entries black), 0 when there is no memory. */
int boot_quant(const uint8_t *rgb, int n, int colours, uint8_t palette[768], uint8_t *indices);

#endif
