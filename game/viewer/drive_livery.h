/* port: a rival's PS1 livery at load (extras.cfg psx_liveries, car_livery.h). The PS1 files are the user's own, read from
 * port.cfg's psx_dir (a folder of the CD's Z<CAR>.VSM and Z<CAR>.WSM); with the key unset or a file missing the car keeps
 * its normal colours. */
#ifndef TNFS_DRIVE_LIVERY_H
#define TNFS_DRIVE_LIVERY_H

#include "game/race/car_livery.h"

#include <stddef.h>
#include <stdint.h>

/* The map from the car's normal body colours to its alternate, read from `psx_dir`/Z<STEM>.VSM and .WSM (STEM as the
 * PC names the car, "P911", "TSupra"). Returns the pairs' count, 0 when a file is missing or there is nothing to map. */
int drive_livery_pairs(const char *psx_dir, const char *stem, CarLiveryPair pairs[CAR_LIVERY_PAIRS]);

/* Remaps the car's CFM in place (the bytes the loader reads): the palettes of its high and low detail banks and its
 * '!xxx' colours, as car_model.c reads them. The CFM buffer is the caller's, so the change is this car's alone. */
void drive_livery_apply(uint8_t *cfm, size_t size, const CarLiveryPair *pairs, int count);

#endif
