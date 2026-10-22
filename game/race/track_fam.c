#include "game/race/track_fam.h"

#include <stdio.h>

void track_fam_name(const TrackFamInputs *in, int32_t number, char *out, size_t size)
{
    /* port: with both variants on, the rally's "r" is the art the race runs on (the geometry is the mirror's,
     * read from 004fbad8); the exe never reaches its "r" branch, so it stays behind the flag. */
    const int rally_first = in->port_both_variants != 0 && in->variant_m != 0 && in->variant_r != 0;
    if (in->time_of_day != 1)
        snprintf(out, size, "%s/%s_T%.2d.fam", in->dir, in->stem, (int)number);
    else if (in->variant_r != 0 && in->track != 3 && (rally_first || in->variant_m == 0))
        snprintf(out, size, "%s/%s_R%.2d.fam", in->dir, in->stem, (int)number);
    else if (in->variant_m != 0)
        snprintf(out, size, "%s/%s_M%.2d.fam", in->dir, in->stem, (int)number);
    else if (in->variant_r != 0 && in->track != 3)
        snprintf(out, size, "%s/%s_R%.2d.fam", in->dir, in->stem, (int)number);
    else
        snprintf(out, size, "%s/%s_%.3d.fam", in->dir_midday, in->stem, (int)number);
}

void track_fam_load(const TrackFamInputs *in, int32_t number, void *buffer, const TrackFamOps *ops)
{
    char name[TRACK_FAM_NAME];
    track_fam_name(in, number, name, sizeof name);
    const int32_t file = ops->open(ops->ctx, name);
    ops->read(ops->ctx, file, buffer, TRACK_FAM_SIZE);
    ops->close(ops->ctx, file);
}

int track_fam_time_offered(int track, int variant_m, int variant_r, int variant_time)
{
    enum { RUSTY_SPRINGS = 3, LOST_VEGAS = 8 };
    if (track == LOST_VEGAS) return 0;
    if (!variant_m && !variant_r) return 1;
    return variant_time && !(track == RUSTY_SPRINGS && variant_r);
}
