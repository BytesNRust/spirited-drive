/* tnfs-studio car wheels: a car's four tyre frames (tyr1..tyr4) made from one picture of the wheel (formats/car_wheels_gen.h).
 *
 *   tnfs-studio car wheels IN.png OUT_DIR [--centre X,Y] [--radius R] [--tyre F] [--slow-angle D] [--fast-arc D] [--force]
 *
 * OUT_DIR gets still.png (the picture), slow-1.png, slow-2.png and fast.png, RGBA. An indexed picture's index 255 is transparent, as
 * in the game's banks. The wheel's circle is found from the opaque pixels unless --centre and --radius give it. */
#ifndef TNFS_STUDIO_CAR_WHEELS_H
#define TNFS_STUDIO_CAR_WHEELS_H

#include "formats/exchange.h"

/* argv[0] is "car", argv[1] "wheels". 0 with `err` filled on failure. */
int car_wheels_command(int argc, char **argv, ExchangeError *err);

#endif
