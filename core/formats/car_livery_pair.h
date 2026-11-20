/* port: one body colour of the console version livery's map, the type the repaint code (formats/car_repaint.h) reads; the map itself is made by the game's
 * car_livery.h. */
#ifndef TNFS_CAR_LIVERY_PAIR_H
#define TNFS_CAR_LIVERY_PAIR_H

#include <stdint.h>

/* one body colour: `from` is the normal livery's colour, `to` the alternate's, both BGR555 (5 bits a channel) */
typedef struct {
    uint16_t from, to;
} CarLiveryPair;

#endif
