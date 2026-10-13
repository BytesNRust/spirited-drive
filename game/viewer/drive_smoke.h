/* --drive's tyre smoke: smoke.qfs and the puffs' blend, as the race's start
 * makes them (00456a7c from 00455a5c, then 00447098's banks and 00456948). */
#ifndef TNFS_DRIVE_SMOKE_H
#define TNFS_DRIVE_SMOKE_H

#include "game/race/puffs.h"

typedef struct {
    PuffArt art;
    PuffBlend blend;
    uint8_t table[PUFF_BLEND_SIZE];   /* 005146a8's 'lut' */
    char misc_dir[256];               /* 005246e8 */
    CrashTrail *trail;                /* 005146ac */
    int ready;                        /* every shape and the blend in */
} DriveSmoke;

/* Before the palette is built: where smoke.qfs lies and the race's trail. */
void drive_smoke_setup(DriveSmoke *s, const char *data_root, CrashTrail *trail);

/* At 00447098's place in the palette's build: 00456a7c, its bank remapped
 * (00446fd0; the port runs no queue loop, the entry stays), then 00456948. */
void drive_smoke_palette(void *ctx, RacePalette *p);

void drive_smoke_free(DriveSmoke *s);

#endif
