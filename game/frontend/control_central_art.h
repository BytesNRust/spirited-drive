/* Control Central's art: what its screens load from the QFS files. */
#ifndef TNFS_CONTROL_CENTRAL_ART_H
#define TNFS_CONTROL_CENTRAL_ART_H

#include "game/frontend/race_settings.h"

/* The seven dwords at 004bafc0..004bafd8, each one of EA's allocations (00475260 frees it). */
typedef struct ControlCentralArt {
    void *art_4bafc0;
    void *art_4bafc4;
    void *art_4bafc8;
    void *art_4bafcc;
    void *art_4bafd0;  /* freed but left set, as the original leaves it */
    void *art_4bafd4;  /* loaded only by some screens: freed when set */
    void *art_4bafd8;
} ControlCentralArt;

typedef struct ControlCentralArtOps {
    void (*free)(void *ctx, void *art);  /* 00475260, given NULL too (the first five) */
    void (*free_inverse)(void *ctx);     /* 00474558: 004c5ff4 freed and cleared when set */
    void *ctx;
} ControlCentralArtOps;

/* 00432884: frees 004bafcc, 004bafc0, 004bafc4, 004bafc8 and clears them,
 * frees 004bafd0 and keeps it, frees and clears 004bafd4 / 004bafd8 only
 * when set; the six shorts 004bafb4..004bafbe become 0x14; then
 * 00474558. */
void control_central_art_free(ControlCentralArt *a, FrontendSession *f, const ControlCentralArtOps *ops);

#endif
