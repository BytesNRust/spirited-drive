/* TNFS_PERF=1: what the UI thread spends per phase, once a second on stderr (milliseconds per second and the frames drawn), for a person to read back when Studio is slow.
 * Off (nothing measured, one test of a flag per phase) unless the environment has TNFS_PERF. */
#ifndef TNFS_TRACK_EDITOR_PERF_H
#define TNFS_TRACK_EDITOR_PERF_H

#include <stdint.h>

typedef enum {
    PERF_DASH,      /* app_dash_poll: the dash PSD/PNG look (stale, manifest, hashes) */
    PERF_POLL,      /* the other jobs' polls: build, install, Convert PSD, Test drive, sheet */
    PERF_WATCH,     /* the file watches: card, masks, light */
    PERF_RECOMPUTE, /* app_recompute */
    PERF_MODEL,     /* the Model panel (the 3D picture) */
    PERF_MASKS,     /* the Masks panel and its texture */
    PERF_PREVIEW,   /* the Preview panel */
    PERF_UI,        /* the whole of ui_frame (the panels above are inside it) */
    PERF_PRESENT,   /* SDL_RenderPresent */
    PERF_PAUSE,     /* the pause between frames (main.c frame_pause) */
    PERF_SLOTS
} PerfSlot;

#ifdef __cplusplus
extern "C" {
#endif

/* 1 when TNFS_PERF is set (looked at once) */
int perf_on(void);
/* the time now to pass to perf_end; 0 when off */
uint64_t perf_begin(void);
/* adds the time since `t0` to `slot` */
void perf_end(PerfSlot slot, uint64_t t0);
/* a frame is done: prints the line when a second has gone by */
void perf_frame(void);

#ifdef __cplusplus
}
#endif

#endif
