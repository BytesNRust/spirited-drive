/* tnfs-studio car view: a car's 3D view as the Studio's panel draws it (core/raster/car_scene.h), to a PNG; the same picture headless, for the
 * tests and for a look without the window.
 *
 *   tnfs-studio car view CAR.CFM --shot OUT.png [--view front|side|rear|top] [--orbit YAW,PITCH,DIST] [--size WxH]
 *
 * --view is a fixed view (F, S, R, T); --orbit the camera by hand: yaw and pitch in degrees (pitch -10 .. 90), DIST in model units from the eye to
 * the model's origin (the default is the home distance, which the command prints with the model's radius). The size is 640x400 by default. The
 * car is drawn unpainted: its bank's own colours, on a transparent background. */
#ifndef TNFS_STUDIO_CAR_VIEW_H
#define TNFS_STUDIO_CAR_VIEW_H

#include "formats/exchange.h"

/* argv[0] is "car", argv[1] "view". 0 with `err` filled on failure. */
int car_view_command(int argc, char **argv, ExchangeError *err);

#endif
