/* The track drawn by the ported renderer (game/race/track_render.h), for the
 * viewer. The texture mapper is still the viewer's affine stand-in. */
#ifndef TNFS_PORTED_VIEW_H
#define TNFS_PORTED_VIEW_H

#include "formats/tri.h"
#include "game/race/track_art.h"
#include "game/race/track_info.h"

#include <stdint.h>

/* yaw: extra turn, 24-bit angle. pixels: 640 x 480. */
void ported_view_render(const Tri *tri, const TrackArt *art, const TrackInfo *info, uint32_t node, int32_t yaw,
                        uint8_t *pixels);

#endif
