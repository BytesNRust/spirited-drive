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
    MODERN_LOOK_DARK = 6,            /* the forced dark fog's grey (modern_look_set_dark) */
    MODERN_LOOK_BAND_ABOVE = 8,      /* port (fog_sample): the rows above the horizon's line the fog's colour is taken from */
    MODERN_LOOK_BAND_BELOW = 6       /* ... and the rows from the line down */
};

typedef struct {
    ModernStyle style;
    uint8_t palette[768];            /* the palette the tables were made from */
    uint8_t rgb[3];
    int made;
    int dark;                        /* port (the light effects): the fog's colour is MODERN_LOOK_DARK whatever the horizon's foot is */
    int sample;                      /* port (fog_sample): 1 takes the fog's colour from the band around the horizon's line (lift in lift) */
    int lift;                        /* the percent of the way to white that band is lifted */
    uint8_t tables[MODERN_FOG_LEVELS][256];
} ModernLook;

/* The average colour of the horizon's foot (its see-through 0xff left out) lifted MODERN_LOOK_LIFT_PERCENT of the
 * way to white and then MODERN_LOOK_GREY_PERCENT to its grey, in the palette's scale; a night foot (its grey under
 * MODERN_LOOK_NIGHT) as it is; MODERN_LOOK_NO_HORIZON grey without one. */
void modern_look_fog_colour(const TrackArt *art, uint8_t rgb[3]);

/* port (fog_sample): the same from the band of rows around the horizon's line (MODERN_LOOK_BAND_ABOVE above it,
 * MODERN_LOOK_BAND_BELOW from it down: the land meeting the sky, not the art's bottom fill, which is a flat colour
 * that can be a sky blue), lifted `lift_percent` of the way to white (0..100) and then MODERN_LOOK_GREY_PERCENT to
 * its grey; a night band as it is; MODERN_LOOK_NO_HORIZON grey without a horizon. */
void modern_look_fog_colour_sampled(const TrackArt *art, int lift_percent, uint8_t rgb[3]);

/* port (fog_sample): the style's fog colour from the band around the horizon's line (modern_look_fog_colour_sampled
 * with `lift_percent`) when `on`, the foot's own when not. */
void modern_look_set_sample(ModernLook *look, int on, int lift_percent);

/* port (the light effects): the fog dark (MODERN_LOOK_DARK on each channel) whatever the horizon's foot says, 0 for the foot's own. */
void modern_look_set_dark(ModernLook *look, int dark);

/* port (the light effects): the weather fog's colour `out` (may be `rgb`): MODERN_LOOK_DARK on each channel when the look is dark, the same
 * as the distance fog's, `rgb` as it is otherwise. */
void modern_look_weather_fog(const ModernLook *look, const uint8_t rgb[3], uint8_t out[3]);

/* The style for the switches over `art` with `segments` of track drawn: NULL with both off. The fog reaches its
 * whole by the farthest road and begins a tenth of the way there; its tables are remade when the palette or the
 * fog's colour changes. */
const ModernStyle *modern_look_style(ModernLook *look, const TrackArt *art, int filter, int fog, int32_t segments);

/* port (weather): the style with a fog of the caller's colour and range (camera depths, whole at `end`), whatever the track's
 * is, and the Texture Filter as asked; the tables are remade when the palette or the colour changes. */
const ModernStyle *modern_look_fixed_fog(ModernLook *look, const TrackArt *art, int filter, const uint8_t rgb[3], float start,
                                         float end);

#endif
