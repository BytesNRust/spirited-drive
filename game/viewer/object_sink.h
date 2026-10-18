/* Viewer pixels for the 3D engine (game/gfx/object3d.h): the ported mappers
 * where they exist, stand-ins for the rest. */
#ifndef TNFS_OBJECT_SINK_H
#define TNFS_OBJECT_SINK_H

#include "game/gfx/object3d.h"
#include "game/gfx/quad.h"

#include <stdint.h>

typedef struct {
    const QuadTarget *target;
    const uint8_t *palette;  /* 768 bytes, 8-bit RGB */
    const uint8_t *shade;    /* 256 bytes: 004c6604, or object_sink_shade */
} ObjectSink;

Object3DSink object_sink(ObjectSink *s);
void object_sink_shade(const uint8_t palette[768], uint8_t shade[256]);

#endif
