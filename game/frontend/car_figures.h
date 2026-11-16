/* port (dep59 section 8, the owner's): a car's figures from its physics record (CARSPECS/<name>.PBS), for a gallery page that has no
 * author's figures (the Warrior's). Weight: the record's word at 0x008, kilograms in 16.16. Power: the torque curve's peak of torque x
 * rpm / 7121 (newton metres and rpm to metric horsepower), point i standing for start + 200 i + 100 rpm as car_torque reads it, over the
 * points whose second word is the curve's own rpm label (start + 200 (i + 1)) and up to the revs the car changes up at (the highest of
 * its forward gears' shift points, 0x288): past them the curve is never reached (the Viper's runs to 11000 rpm, it changes up at 5900). Before the
 * record's power scale: with it the eight cars' figures are far from theirs, without it within a few per cent (the RX-7 262 hp, the
 * Diablo 493). */
#ifndef TNFS_CAR_FIGURES_H
#define TNFS_CAR_FIGURES_H

#include "formats/pbs.h"

#include <stdint.h>

/* the power (hp) and weight (kg) of `record`; 1, or 0 when either cannot be read (no curve point, or a weight out of 1..9999) */
int car_figures_pbs(const uint8_t record[PBS_SIZE], int *hp, int *kg);

#endif
