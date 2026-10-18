#include "game/race/track_fam.h"

#include <stdio.h>

void track_fam_name(const TrackFamInputs *in, int32_t number, char *out, size_t size)
{
    if (in->time_of_day != 1)
        snprintf(out, size, "%s/%s_T%.2d.fam", in->dir, in->stem, (int)number);
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
