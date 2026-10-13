/* The race's main loop and the words it keeps (0044e8f8 and its neighbours). */
#ifndef TNFS_RACE_LOOP_H
#define TNFS_RACE_LOOP_H

#include <stdint.h>

/* 0044f9ac (): 0051371c = 0. The loop tests it at 0044edad, after 00444394
 * or 0044435c; 004381f8 clears it when the camera goes back to the
 * player's own car. */
void race_loop_clear_51371c(int32_t *w51371c);

#endif
