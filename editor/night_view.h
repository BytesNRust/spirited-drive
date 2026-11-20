/* The car editor's night view settings (dep59 N1): the track whose light the car is seen in, and what each lamp switch does to the preview (core/formats/car_night.h).
 * Pure C, no window. */
#ifndef TNFS_TRACK_EDITOR_NIGHT_VIEW_H
#define TNFS_TRACK_EDITOR_NIGHT_VIEW_H

#include "formats/car_night.h"

enum { NIGHT_LIGHT_MIN = 10, NIGHT_LIGHT_MAX = 250 };   /* the "Light at car" slider, percent */

typedef struct {
    const char *label;   /* "Rusty Springs" */
    const char *stem;    /* "tr5": the stem the INFO table and the night tone know it by */
    float ratio[3];         /* the light at the car at night as the race has it at the node Race photo puts the car (game log "lamps: slot 0 node N ON: ... ratio r r r", night, light_effects): 1 is the colour as the file has it */
} NightRef;

/* The tracks the view can take its light from, the first the default (Rusty Springs, as Test in race and Race photo use); `night_ref_count` of them. */
const NightRef *night_refs(void);
int night_ref_count(void);

/* The index of `stem` (any case) in night_refs, -1 for none. */
int night_ref_find(const char *stem);

/* The switches of the view: day or night, and the lamps. */
typedef struct {
    int night;       /* 0 Day, 1 Night */
    int ref;         /* an index of night_refs */
    int light;       /* "Light at car", percent of the reference track's ratio: 100 is the race's at the photo's node */
    int lights;      /* head and side lights (they go together at night, as in the race) */
    int brake, reverse, police;
} NightSettings;

/* The defaults: Day, Rusty Springs, the light at the car at 100 %, the lights on (they show once Night is chosen), the others off. */
NightSettings night_settings_default(void);

/* The state of core/formats/car_night.h these settings give: no light by day (the palette as the Preview shows it), at night the car scaled by the reference track's light at the car times `light` (the race lights a car by its node's ratio, not by the world's tone),
 * the lamps' state by the rules of the race (the head and position lights only at night; the brake and the reversing light in any light). `police_tick` is the flash's tick. */
CarNightState night_settings_state(const NightSettings *s, int32_t police_tick);

/* 1 when these settings show the car exactly as the day preview does (no tint of its own, no lamp lit): the preview's own picture can be used. */
int night_settings_plain(const NightSettings *s);

#endif
