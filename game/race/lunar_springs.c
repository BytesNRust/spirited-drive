/* port: see lunar_springs.h. */
#include "game/race/lunar_springs.h"

#include <stdio.h>

/* the moon's rally FAM (the CD's name, in NTRACKFM as the rally art of a track with no "r" art) and its picture.
 * MOON.QFS sits in the German art directory only; the art loader prefixes frontend/art/ (English) or frontend/gart/
 * (German) to a picture's name, and "../gart/" from either reaches it. */
static const char MOON_FAM[] = "SIMDATA/NTRACKFM/TR8_R01.FAM";
static const char MOON_PICTURE[] = "../gart/control/moon.qfs";
const char LUNAR_SIGNS[LUNAR_SIGN_COUNT][4] = { { '0', '5', '0', '0' } };
const char LUNAR_FAR_ROAD[4] = { '0', '1', 'A', '4' }, LUNAR_NEAR_ROAD[4] = { '0', '1', 'A', '0' };

/* writes "<root>/<rest>" to out; 0 when it does not fit, so the caller keeps its own name */
static int join(char *out, size_t size, const char *root, const char *rest)
{
    return snprintf(out, size, "%s/%s", root, rest) < (int)size;
}

int lunar_springs_active(int track, int variant_r, int lunar)
{
    return lunar != 0 && variant_r != 0 && track == LUNAR_TRACK_RUSTY;
}

int lunar_gravity_active(int track, int variant_r, int lunar, int lunar_gravity)
{
    return lunar_gravity != 0 && lunar_springs_active(track, variant_r, lunar);
}

int lunar_springs_fam(const char *data_root, int track, int variant_r, int lunar, char *out, size_t size)
{
    if (!lunar_springs_active(track, variant_r, lunar)) return 0;
    return join(out, size, data_root, MOON_FAM);
}

int lunar_springs_picture(int art, int lunar, char *out, size_t size)
{
    if (!lunar || art != LUNAR_ART_SPRINGSR) return 0;
    return snprintf(out, size, "%s", MOON_PICTURE) < (int)size;
}
