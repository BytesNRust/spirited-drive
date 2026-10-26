/* port (body shine): what the drawing sets around a car's body (object_sink.h's object_sink_set_shine): the tables for the palette in use and the
 * scroll of the streaks, which follows how far the car has travelled. Off (the port flag body_shine 0) it sets nothing. */
#ifndef TNFS_BODY_SHINE_H
#define TNFS_BODY_SHINE_H

#include "game/race/car_struct.h"
#include "viewer/shine_draw.h"

#include <stdint.h>

void body_shine_enable(int on);   /* the flag, read at the cars' load */
int body_shine_enabled(void);

/* Before the body of `car` (the race's slot) is drawn with `palette` (768 bytes, 8 bits): the sink's tables and scroll. A palette met in the frame
 * keeps its tables until two others have come since, so the commands recorded for the render bands stay valid until they are flushed. */
void body_shine_begin(int slot, const Car *car, const uint8_t palette[768]);
void body_shine_end(void);

/* what the last begin set (for the tests): the tables or NULL, and a slot's scroll in turns */
ShineTables body_shine_tables(void);
float body_shine_phase(int slot);

#endif
