/* port (light_effects): the night's flat tint of a track and the ceiling its light is held under (game/viewer/drive_light.c, and the car editor's night view: it cannot link the viewer).
 * One cool blue-grey tone; the City's and Lost Vegas' take a lift's setting (city_lift, vegas_lift: 0 or 1) that raises it. */
#ifndef TNFS_NIGHT_TONE_H
#define TNFS_NIGHT_TONE_H

typedef enum { NIGHT_TONE_OTHER = 0, NIGHT_TONE_CITY = 1, NIGHT_TONE_VEGAS = 2 } NightKind;

/* The kind of a track by its stem (any case): "CY1".."CY3" the City, "TR4" Lost Vegas, any other (and NULL) other. */
NightKind night_kind_of(const char *track);

/* The ceiling (at most 1 a channel) of the night's light of `kind` at `lift` (0 or 1); NIGHT_TONE_OTHER has no ceiling of its own: `fallback`. */
float night_ceiling(NightKind kind, int lift, float fallback);

/* The night's flat tint for a track of `kind`: the cool tone under the lift's ceiling (a share of the lifted one's: 1 for other tracks). */
void night_tone(NightKind kind, int lift, float out[3]);

#endif
