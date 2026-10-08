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
} ObjectSink;

Object3DSink object_sink(ObjectSink *s);

#endif
