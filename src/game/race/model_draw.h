/* The race's 3D models in the scene: culled against the view (00458c04) and
 * handed to the 3D engine (00458b7c). Scenery models arrive here from the
 * track renderer's row walk (0044a4b0) and, later, the cars.
 *
 * Model units are world 16.16 >> 12 (1/16 m). */
#ifndef TNFS_MODEL_DRAW_H
#define TNFS_MODEL_DRAW_H

#include "game/gfx/object3d.h"
#include "game/race/view.h"

#include <stdint.h>

enum {
    MODEL_NEAR_Z = 30,          /* 004c7b1c, set by 004585cd */
    MODEL_NEAR_FLAG_Z = 0x219,  /* closer than this, polygons on the near plane drop */
    MODEL_MIN_Z = -0x40,
    MODEL_MAX_Z = 0x1f40,
    MODEL_SIDE_MARGIN = 0x500
};

typedef struct {
    const View *view;
    const int32_t *camera_position;  /* world 16.16 (camera struct 00511fe0 +0x10) */
    const int32_t *camera_matrix;    /* 00511fe4 */
    /* 004fb7c2 == 0 && 004fb7be == 2: the side test halves x.
     * TODO(004fb7be): name these two once the race setup is ported. */
    int half_side_test;
} ModelView;

/* heading: 24-bit. Returns 0 when culled, 1 when drawn. */
int model_draw_object(Object3D *obj, const int32_t position[3], int32_t heading, const ModelView *mv,
                      const Object3DSink *sink);

#endif
