/* port (body shine): what the drawing sets around a car's body (object_sink.h's object_sink_set_shine): the tables for the palette in use and
 * the world's up in the camera's space, the horizon the shine's reflections are looked up against. Off (the port flag body_shine 0) it sets
 * nothing. */
#ifndef TNFS_BODY_SHINE_H
#define TNFS_BODY_SHINE_H

#include "viewer/shine_draw.h"

#include <stdint.h>

void body_shine_enable(int strength);   /* the flag, read at the cars' load: 0 off, 1..3 the strength (car_shine.h), 4 the envmap (car_envmap.h) */
int body_shine_enabled(void);

/* Before a car's body is drawn with `palette` (768 bytes, 8 bits) under the camera `camera_matrix` (16.16, the one the weather's sky uses): the
 * sink's tables and up. A palette met in the frame keeps its tables until two others have come since, so the commands recorded for the render
 * bands stay valid until they are flushed. */
void body_shine_begin(const uint8_t palette[768], const int32_t camera_matrix[9]);
void body_shine_end(void);

/* what the last begin set (for the tests): the classic tables or NULL, the envmap's tables (strength 4) or NULL, and the world's up in the
 * camera's space (unit) */
ShineTables body_shine_tables(void);
const EnvShine *body_shine_env(void);
const float *body_shine_up(void);

#endif
