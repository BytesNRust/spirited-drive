/* The car editor's night view settings (dep59 N1): the light and tint the car is seen in at night, and what each lamp switch does to the preview (core/formats/car_night.h).
 * Pure C, no window. */
#ifndef TNFS_TRACK_EDITOR_NIGHT_VIEW_H
#define TNFS_TRACK_EDITOR_NIGHT_VIEW_H

#include "formats/car_night.h"

enum { NIGHT_LIGHT_MIN = 10, NIGHT_LIGHT_MAX = 250, NIGHT_LIGHT_DEFAULT = 100 };   /* the "Light at car" slider, percent */
#define NIGHT_TINT_DEFAULT 0.72f   /* the tint's default: the light at the car on Rusty Springs at the Race photo's node (game log "lamps: slot 0 node 5 ON: ... ratio 0.72 0.72 0.72", night), a neutral grey */

/* The switches of the view: day or night, the light and its tint, and the lamps. The car at night is its palette times the tint times the light: a quick look, not a track's
 * real light (that is Drive's In-game photo). */
typedef struct {
    int night;       /* 0 Day, 1 Night */
    int light;       /* "Light at car", percent of the tint: 100 leaves the tint as it is */
    float tint[3];   /* the colour the car is lit with at night, each channel 0..1 (NIGHT_TINT_DEFAULT grey by default) */
    int lights;      /* head and side lights (they go together at night, as in the race) */
    int brake, reverse, police;
} NightSettings;

/* The defaults: Day, the light at 100 %, the neutral tint, the lights on (they show once Night is chosen), the others off. When the editor's ini (or the user) has set a light and
 * a tint (night_pref_set) the defaults of a new car are those. */
NightSettings night_settings_default(void);

/* The Reset buttons: the light back to 100 %, the tint back to the neutral grey. */
void night_settings_reset_light(NightSettings *s);
void night_settings_reset_tint(NightSettings *s);

/* The light and tint the editor remembers between runs (the ini's [StudioSettings][Editor] NightLight= and NightTint=): night_pref_set keeps them (clamped: the light to its slider's
 * range, a channel to 0..1); night_pref_get gives them, 0 when none was set. */
void night_pref_set(int light, const float tint[3]);
int night_pref_get(int *light, float tint[3]);

/* The state of core/formats/car_night.h these settings give: no light by day (the palette as the Preview shows it), at night the car scaled by the tint times `light` (the race lights a car by its place's ratio, not by the world's tone),
 * the lamps' state by the rules of the race (the head and position lights only at night; the brake and the reversing light in any light). `police_tick` is the flash's tick. */
CarNightState night_settings_state(const NightSettings *s, int32_t police_tick);

/* 1 when these settings show the car exactly as the day preview does (no tint of its own, no lamp lit): the preview's own picture can be used. */
int night_settings_plain(const NightSettings *s);

#endif
