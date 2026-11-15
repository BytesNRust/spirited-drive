/* port (dep59 step 3): config.dat's image never holds an added car (its reader takes 0..7 and 11 only, and a value outside drops the
 * whole read: config_dat.c). The menus' pick does: what goes into the image is the pick as the image takes it, an original as it is and
 * an added car as the last original held, and what comes back from the image is the added pick again while the image keeps that
 * original. A zeroed CarPick holds nothing added. */
#ifndef TNFS_CAR_PICK_H
#define TNFS_CAR_PICK_H

#include <stdint.h>

typedef struct {
    int16_t added;   /* the added car picked (CAR_ADDED_FIRST..), 0 none */
    int16_t stock;   /* the last car below CAR_ADDED_FIRST picked: what the image holds for `added` */
} CarPick;

/* `pick` as the image takes it */
int16_t car_pick_to_image(CarPick *p, int16_t pick);
/* the pick `image` stands for */
int16_t car_pick_from_image(const CarPick *p, int16_t image);

#endif
