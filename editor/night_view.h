/* The car editor's night view settings (dep59 N1): the track whose light the car is seen in, and what each lamp switch does to the preview (core/formats/car_night.h).
 * Pure C, no window. */
#ifndef TNFS_TRACK_EDITOR_NIGHT_VIEW_H
#define TNFS_TRACK_EDITOR_NIGHT_VIEW_H

#include "formats/car_night.h"

typedef struct {
    const char *label;   /* "Rusty Springs" */
    const char *stem;    /* "tr5": the stem the INFO table and the night tone know it by */
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
    int lights;      /* head and side lights (they go together at night, as in the race) */
    int brake, reverse, police;
} NightSettings;

/* The defaults: Day, Rusty Springs, the lights on (they show once Night is chosen), the others off. */
NightSettings night_settings_default(void);

/* The state of core/formats/car_night.h these settings give: no tint by day (the palette as the Preview shows it), the port's night tone of the reference track at night (the cool blue-grey of drive_light.c),
 * the lamps' state by the rules of the race (lights and brake only at night; the reversing light in any light). `police_tick` is the flash's tick. */
CarNightState night_settings_state(const NightSettings *s, int32_t police_tick);

/* 1 when these settings show the car exactly as the day preview does (no tint of its own, no lamp lit): the preview's own picture can be used. */
int night_settings_plain(const NightSettings *s);

#endif
