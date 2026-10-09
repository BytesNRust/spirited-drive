/* The original indexes several spec tables with the gear, so reverse and
 * the top gear read the words around a table. This reads a 32-bit word of
 * the specs by byte offset the same way (0 outside the record). */
#ifndef TNFS_CAR_SPECS_WORD_H
#define TNFS_CAR_SPECS_WORD_H

#include "game/race/car_specs.h"

#include <string.h>

static inline int32_t car_specs_word(const CarSpecs *specs, int32_t offset)
{
    int32_t v = 0;
    if (offset >= 0 && offset + 4 <= (int32_t)sizeof *specs) memcpy(&v, (const uint8_t *)specs + offset, sizeof v);
    return v;
}

/* The gear ratio of a gear (-2 reverse .. ), from spec +0x70 + 4 * gear. */
static inline int32_t car_gear_ratio(const CarSpecs *specs, int32_t gear)
{
    return car_specs_word(specs, (int32_t)offsetof(CarSpecs, gear_ratio) + (gear + 2) * 4);
}

#endif
