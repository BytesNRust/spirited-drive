/* the sky over the view, one band's rows of it (view_sky.c); ported_view.c calls this */
#ifndef TNFS_VIEW_SKY_H
#define TNFS_VIEW_SKY_H

#include "game/race/sky_ring.h"
#include "viewer/ported_view.h"
#include "viewer/view_shared.h"

#include <stdint.h>

/* port (render bands): the view's clear and the sky, one band's rows of them (everything they read is built before) */
typedef struct {
    uint8_t *pixels;
    const QuadTarget *quad;
    const RasterTarget *target;
    const View *view;
    const HorizonTables *horizon;
    const ClipRect *clip;
    const TrackArt *art;
    const TrackInfo *info;
    const TrackCamera *cam;
    const PortedCamera *pc;
    const HorizonSmooth *smooth;
    const FshEntry *horz;      /* the flat horizon's bitmap: the track's, or the console version sky's */
    const FshEntry *ring_horz; /* the ring's: the track's, or the alternative sky's */
    int32_t box_left, box_right, line;
    int ring, flat, banded;
} SkyStage;

extern SkyRing sky_ring;
void sky_stage_band(void *user, int top, int bottom);
int view_mirrored(void);
#endif
