/* port (car_colours): the shade table (the HUD's remap: skid marks, shadow quads) is matched before the fit gives the cars' slots entries of the palette, so a
 * colour whose darker one is an entry the fit then rewrote to car paint was drawn in that paint (a blue or cream skid mark on dark road). shade_refit
 * points those colours at the nearest usable entry to the colour the entry had before the fit. */
#ifndef TNFS_SHADE_FIT_H
#define TNFS_SHADE_FIT_H

#include <stdint.h>

/* shade[256]: the table, changed in place. before_rgbx: the palette before the fit (4 bytes per entry). hidden[256]: the entries the fit rewrote. enabled[256]:
 * the entries a search may pick. A row whose own entry is hidden is left alone. Returns how many rows were changed. */
int shade_refit(uint8_t shade[256], const uint8_t *before_rgbx, const uint8_t hidden[256], const uint8_t enabled[256]);

#endif
