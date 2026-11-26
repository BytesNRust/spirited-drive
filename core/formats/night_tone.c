#include "formats/night_tone.h"

#include <stddef.h>
#include <strings.h>

static const float CITY_CEILING[2] = { 0.35f, 0.42f };   /* lifted: with the beams, the tunnels and the lamps standing out of it */
static const float VEGAS_CEILING[2] = { 0.35f, 0.80f };
static const float NIGHT_TONE_COOL[3] = { 0.40f, 0.44f, 0.52f };   /* one neutral cool blue-grey */

static int lift_of(int lift) { return lift != 0; }

NightKind night_kind_of(const char *track)
{
    if (track == NULL) return NIGHT_TONE_OTHER;
    if (strncasecmp(track, "CY", 2) == 0) return NIGHT_TONE_CITY;
    return strcasecmp(track, "TR4") == 0 ? NIGHT_TONE_VEGAS : NIGHT_TONE_OTHER;
}

float night_ceiling(NightKind kind, int lift, float fallback)
{
    return kind == NIGHT_TONE_CITY ? CITY_CEILING[lift_of(lift)] : kind == NIGHT_TONE_VEGAS ? VEGAS_CEILING[lift_of(lift)] : fallback;
}

void night_tone(NightKind kind, int lift, float out[3])
{
    const float scale = kind == NIGHT_TONE_CITY ? CITY_CEILING[lift_of(lift)] / CITY_CEILING[1] : kind == NIGHT_TONE_VEGAS ? VEGAS_CEILING[lift_of(lift)] / VEGAS_CEILING[1] : 1.0f;
    for (int k = 0; k < 3; k++) out[k] = NIGHT_TONE_COOL[k] * scale;
}
