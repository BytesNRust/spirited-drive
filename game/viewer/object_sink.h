/* Viewer pixels for the 3D engine (game/gfx/object3d.h): the ported mappers
 * where they exist, stand-ins for the rest. */
#ifndef TNFS_OBJECT_SINK_H
#define TNFS_OBJECT_SINK_H

#include "game/gfx/object3d.h"
#include "game/gfx/quad.h"
#include "game/race/view.h"
#include "viewer/shine_draw.h"

#include <stdint.h>

typedef struct {
    const QuadTarget *target;
    const uint8_t *palette;  /* 768 bytes, 8-bit RGB */
    const uint8_t *shade;    /* 256 bytes: 004c6604, or object_sink_shade */
    const View *view;        /* port: Modern 3D's, the polygons clipped and perspective-correct (modern_road.h); NULL the exe's */
} ObjectSink;

Object3DSink object_sink(ObjectSink *s);

/* port (the light effects' light): a 256-byte table the objects drawn from now on go through (every texel and flat colour), or
 * NULL for none; the shadow's shade mode is left as it is. */
void object_sink_set_light(const uint8_t *table);
/* port (body shine): the world's up in the camera's space, the fixed horizon the envmap's sphere map is looked up against (NULL: the camera's y) */
void object_sink_set_shine_up(const float *up);
/* port (body_shine 4): while set, a textured polygon the class maps know that carries vertex normals takes the envmap pass (shine_draw.h's
 * EnvShine, kept by pointer until the frame is flushed); NULL for none, and the draws are the exe's. */
void object_sink_set_envmap(const EnvShine *env);
void object_sink_shade(const uint8_t palette[768], uint8_t shade[256]);

#endif
