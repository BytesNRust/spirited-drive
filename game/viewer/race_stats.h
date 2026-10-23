/* port: the race's stats overlay, F12 in a race (extras.cfg's stats_overlay, Off by default): frames a second, the frame
 * time, the sim's steps a second and clock level, the refresh and the render's size, in the top-left corner under the
 * HUD's top row. Presentation only: it is drawn into a copy of the frame just before the frame is presented, so the
 * race, its replays and the screenshots never see it. */
#ifndef TNFS_RACE_STATS_H
#define TNFS_RACE_STATS_H

#include <stddef.h>
#include <stdint.h>

enum {
    RACE_STATS_KEY = 0x58,           /* F12, set-1 code */
    RACE_STATS_WINDOW_MS = 500,      /* the figures are averaged over this much real time */
    RACE_STATS_LINES = 4,
    RACE_STATS_LINE_LEN = 64
};

/* The last complete window's figures; `ready` is 0 until the first window has run. */
typedef struct {
    int ready;
    double fps;                 /* frames presented a real second */
    double frame_avg_ms;        /* the mean gap between presented frames */
    uint32_t frame_max_ms;      /* the longest gap */
    double steps_per_s;         /* the race's steps (race ticks) a real second */
} RaceStatsFigures;

typedef struct {
    int primed;                 /* a window is running */
    uint32_t window_ms;         /* its first frame */
    uint32_t last_ms;           /* the last frame presented */
    int frames;                 /* the gaps taken in the window */
    uint32_t sum_ms, max_ms;    /* their sum and longest */
    int32_t window_tick;        /* the race's tick at the window's first frame */
    RaceStatsFigures figures;
} RaceStatsMeter;

typedef struct {
    int on;                     /* the overlay is shown */
    int key_down;               /* F12 held at the last frame: the toggle is on the press */
    RaceStatsMeter meter;
    uint8_t palette[768];       /* the palette the text styles were last built from */
    int palette_known;
    uint8_t dark;               /* the palette's nearest black, the backing's */
    int text_ready;             /* the font is loaded */
} RaceStats;

/* What the overlay says about the frame, from the figures and the race's settings at the frame. */
typedef struct {
    RaceStatsFigures figures;
    int clock_level;            /* drive_clock's level: 1 normal, 3 Turbo */
    int refresh_hz;             /* the frames a second drawn between steps */
    int interp;                 /* the poses are blended between steps */
    int width, height;          /* the frame drawn */
} RaceStatsView;

typedef struct {
    char line[RACE_STATS_LINES][RACE_STATS_LINE_LEN];
} RaceStatsText;

/* A race starts with the saved state (extras.cfg's stats_overlay): `on` */
void race_stats_start(RaceStats *s, int on);
/* 1 when F12 was pressed this frame (the edge, not the hold) and the overlay has been toggled */
int race_stats_key(RaceStats *s, int down);
/* race_stats_key, and on a toggle the new value is written to extras.cfg; 1 when it toggled */
int race_stats_key_save(RaceStats *s, int down);
/* One presented frame at `now_ms`, the race's tick then; every frame counts, shown or not */
void race_stats_sample(RaceStats *s, uint32_t now_ms, int32_t tick);
void race_stats_meter_frame(RaceStatsMeter *m, uint32_t now_ms, int32_t tick);
/* The lines the overlay shows for the view */
void race_stats_format(const RaceStatsView *v, RaceStatsText *out);
/* The frame with the overlay drawn on a copy of it when on; `frame` itself when off (or when there is no room or no
 * font). The copy is kept by the module, valid until the next call. */
const uint8_t *race_stats_compose(RaceStats *s, const uint8_t *frame, int width, int height,
                                  const uint8_t palette[768], const RaceStatsView *v);

#endif
