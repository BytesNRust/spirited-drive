/* port: EXTRAS' Texture Filter and Fog made into Modern 3D's style (modern_road.h) for a track's art. */
#ifndef TNFS_MODERN_LOOK_H
#define TNFS_MODERN_LOOK_H

#include "game/race/track_art.h"
#include "viewer/modern_road.h"

#include <stdint.h>

enum {
    MODERN_LOOK_FOOT_ROWS = 2,       /* the horizon's last rows, where it meets the land: the fog's colour */
    MODERN_LOOK_NO_HORIZON = 128     /* the grey without a horizon */
};

typedef struct {
    ModernStyle style;
    uint8_t palette[768];            /* the palette the tables were made from */
    uint8_t rgb[3];
    int made;
    uint8_t tables[MODERN_FOG_LEVELS][256];
} ModernLook;

/* The average colour of the horizon's foot (its see-through 0xff left out), in the palette's scale. */
void modern_look_fog_colour(const TrackArt *art, uint8_t rgb[3]);

/* The style for the switches over `art` with `segments` of track drawn: NULL with both off. The fog reaches its
 * whole by the farthest road and begins a tenth of the way there; its tables are remade when the palette or the
 * fog's colour changes. */
const ModernStyle *modern_look_style(ModernLook *look, const TrackArt *art, int filter, int fog, int32_t segments);

#endif
