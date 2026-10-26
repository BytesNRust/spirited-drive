/* port: EXTRAS' Texture Filter and Fog made into Modern 3D's style (modern_road.h) for a track's art. */
#ifndef TNFS_MODERN_LOOK_H
#define TNFS_MODERN_LOOK_H

#include "game/race/track_art.h"
#include "viewer/modern_road.h"

#include <stdint.h>

enum {
    MODERN_LOOK_FOOT_ROWS = 2,       /* the horizon's last rows, where it meets the land: the fog's colour */
    MODERN_LOOK_NO_HORIZON = 128,    /* the grey without a horizon */
    MODERN_LOOK_LIFT_PERCENT = 40,   /* the foot's colour this far to white: a haze is lighter than the land behind */
    MODERN_LOOK_GREY_PERCENT = 35,   /* then this far to its own grey: a haze keeps its tint, softened */
    MODERN_LOOK_NIGHT = 4,           /* a foot darker than this (Lost Vegas' black) is night: its fog stays dark */
    MODERN_LOOK_DARK = 6             /* the forced dark fog's grey (modern_look_set_dark) */
};

typedef struct {
    ModernStyle style;
    uint8_t palette[768];            /* the palette the tables were made from */
    uint8_t rgb[3];
    int made;
    int dark;                        /* port (the PS1 look): the fog's colour is MODERN_LOOK_DARK whatever the horizon's foot is */
    uint8_t tables[MODERN_FOG_LEVELS][256];
} ModernLook;

/* The average colour of the horizon's foot (its see-through 0xff left out) lifted MODERN_LOOK_LIFT_PERCENT of the
 * way to white and then MODERN_LOOK_GREY_PERCENT to its grey, in the palette's scale; a night foot (its grey under
 * MODERN_LOOK_NIGHT) as it is; MODERN_LOOK_NO_HORIZON grey without one. */
void modern_look_fog_colour(const TrackArt *art, uint8_t rgb[3]);

/* port (the PS1 look): the fog dark (MODERN_LOOK_DARK on each channel) whatever the horizon's foot says, 0 for the foot's own. */
void modern_look_set_dark(ModernLook *look, int dark);

/* The style for the switches over `art` with `segments` of track drawn: NULL with both off. The fog reaches its
 * whole by the farthest road and begins a tenth of the way there; its tables are remade when the palette or the
 * fog's colour changes. */
const ModernStyle *modern_look_style(ModernLook *look, const TrackArt *art, int filter, int fog, int32_t segments);

/* port (weather): the style with a fog of the caller's colour and range (camera depths, whole at `end`), whatever the track's
 * is, and the Texture Filter as asked; the tables are remade when the palette or the colour changes. */
const ModernStyle *modern_look_fixed_fog(ModernLook *look, const TrackArt *art, int filter, const uint8_t rgb[3], float start,
                                         float end);

#endif
