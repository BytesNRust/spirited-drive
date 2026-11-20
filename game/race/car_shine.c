/* port (body shine): see car_shine.h. */
#include "game/race/car_shine.h"
#include "game/sys/palette.h"

void car_envmap_tables(const uint8_t palette[768], uint8_t paint[CAR_SHINE_LEVELS][256], uint8_t glass[CAR_SHINE_LEVELS][256])
{
    car_shine_tables(palette, palette_usable(), palette_emissive(), paint, glass);
}
