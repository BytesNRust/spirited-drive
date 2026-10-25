/* port: the car variants the car showcase's box offers (race/car_variant.h): the PS1 livery first. re/notes/car-variants.md
 * says how to add one. */
#ifndef TNFS_SHOWCASE_VARIANTS_H
#define TNFS_SHOWCASE_VARIANTS_H

#include "game/race/car_variant.h"

extern const CarVariant showcase_variants[];
extern const int showcase_variant_count;

/* forgets what the entries read from the user's files (the showcase's start, so a changed setting is seen) */
void showcase_variants_reset(void);

#endif
