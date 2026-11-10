/* the scenery models, the car slots, the shadows and the skid marks (view_models.c); ported_view.c calls these */
#ifndef TNFS_VIEW_MODELS_H
#define TNFS_VIEW_MODELS_H

#include "viewer/view_shared.h"

#include <stdint.h>

void draw_model(void *user, int model, const int32_t position[3], uint32_t heading);
void draw_skids(void *user, int32_t row, const int32_t s[2], const int32_t t[2]);
extern SkidMarks skid_marks;
extern int32_t skid_node;
SkidRing skid_ring(void);
#endif
