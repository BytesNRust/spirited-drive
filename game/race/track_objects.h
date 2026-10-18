/* Scenery placement: the world-space corners of track objects, as 0043eca4
 * computes them while it streams segments in (into 004fd134), and the origin
 * and heading of 3D objects for 00458c04 (via 0044a4b0).
 *
 * Descriptor types (TriDescriptor.type) are also the number of corners:
 *   1  3D object from the FAM objects child (resource_id), drawn rotated
 *   4  billboard: one quad facing the object's heading
 *   6  two-sided billboard: that quad plus a second one at 90 degrees along
 *      its right edge: corners 1, 4, 5, 2 (resource2_id textures it) */
#ifndef TNFS_TRACK_OBJECTS_H
#define TNFS_TRACK_OBJECTS_H

#include "formats/tri.h"
#include "game/race/track_geometry.h"

#include <stdint.h>

enum {
    TRACK_OBJECT_MODEL = 1,
    TRACK_OBJECT_BILLBOARD = 4,
    TRACK_OBJECT_TWO_SIDED = 6,
    TRACK_OBJECT_MAX_CORNERS = 6
};

typedef struct {
    uint32_t node;            /* road node, masked to the ring */
    TriDescriptor descriptor;
    int corner_count;         /* 0 for models */
    TrackPoint corners[TRACK_OBJECT_MAX_CORNERS];  /* top-left, top-right, bottom-right, bottom-left, ... */
    TrackPoint origin;        /* node position + offset << 8 */
    uint32_t heading;         /* models: 24-bit angle, 0x1000000 per turn */
} TrackObject;

/* Index into the scenery records (< tri->scenery_count), standing: the track
 * viewer's listing. The race's knocked-over objects (004b9098) lie flat in its
 * own draw path (0043eca4, track_build.c). */
TriResult track_object(const Tri *tri, uint32_t index, TrackObject *out);

#endif
