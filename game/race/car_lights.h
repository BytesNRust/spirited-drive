/* A car's lights in the palette as it is drawn (00458d4c, 00458db8..00458e9c):
 * a racer's brake light entry, or the police car's flashing lights while a
 * stop notice is up. */
#ifndef TNFS_CAR_LIGHTS_H
#define TNFS_CAR_LIGHTS_H

#include "game/race/car_struct.h"

#include <stdint.h>

typedef struct {
    void *ctx;
    void (*write)(void *ctx, int32_t first, int32_t count, const uint8_t *colours);  /* 00476460, 6-bit colours */
} CarLightOps;

typedef struct {
    int32_t notice[2];            /* 004b92dc, 004b92e0: the police stop's notices */
    const uint8_t *slot_colours;  /* 0051c798: a slot's own palette entry */
    int32_t finish_cars;          /* 00509d80 */
    int32_t tick;                 /* 00513e70 */
    int32_t *flash_tick;          /* 0051c7f8: the tick the flash last turned */
    int32_t *flash;               /* 004c5820: the flash's phase, 1 or 0 */
} CarLightInputs;

/* With no notice up: a car whose slot has its own entry (0051c798) and is
 * below finish_cars gets that entry from 004c578c's model row, or 004c57b3's
 * (13 rows on, the lit one) when its brake (+0x3b9) is above 10. With a
 * notice: the police car (slot 2) chasing or stopping (+0x176 & 0xc) turns
 * its flash when more than 15 ticks have passed since the last turn, and
 * writes entries 0xfa..0xfd from 004c5610 (flash 1) or 004c561c (0). */
void car_lights(const CarLightOps *ops, const CarLightInputs *in, const Car *car);

#endif
