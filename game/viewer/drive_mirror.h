/* `--drive`'s rear-view mirror: the ported pieces (mirror_scene.h,
 * mirror_sky.h, shape_flip.h, race_camera_view, track_stream_step) run
 * the way 0044cc74 runs them, into a picture the cockpit places at dash
 * point 0. Glue, no exe function of its own. */
#ifndef TNFS_DRIVE_MIRROR_H
#define TNFS_DRIVE_MIRROR_H

#include "formats/tri.h"
#include "game/gfx/quad.h"
#include "game/race/cockpit.h"
#include "game/race/mirror_scene.h"
#include "game/race/model_draw.h"
#include "game/race/race_state.h"
#include "game/race/track_art.h"
#include "game/race/track_render.h"
#include "game/race/view.h"

#include <stdint.h>

typedef struct {
    uint8_t *picture;           /* 004c2d94's shape: 0x10 header bytes (type 0x7b), then width x height */
    int32_t width, height;      /* 00512034, 00512038 */
    int32_t state;              /* 00511ff4 */
    int32_t first_segment;      /* 0050421c (-1: made again) */
    int32_t count;              /* 00509784 */
    int32_t near;               /* the mirror camera's +4 (0044b970) */
    int32_t world[MIRROR_POINTS][3];        /* 0050909c */
    int32_t vertices[MIRROR_SCREEN_WORDS_AT + 2 * (MIRROR_POINTS + 11) + 2];  /* 00509330, 005095c4 */
    uint8_t *horz;              /* a one-shape bank holding the track's 'horz' (0051203c) */
    uint32_t enclosed_colour;   /* 0050b7c0 (00446bac: grey 0x20) */
    uint32_t property8_colour;  /* 0050b79c (00446bac: green 0x20) */
    uint8_t car_rows[MIRROR_CAR_ROWS];      /* 0051c7a1 */
    int32_t matrix[9];          /* the mirror camera's (chase +0xb8) */
    View view;
    int32_t extra;              /* port: the wide screen's columns added to the picture (drive_mirror_widen); 0 none */
    int32_t fov;                /* port: the field that keeps the mirror's focal length over the wider picture */
    ViewLayout wide_layout;     /* port: the mirror camera's layout as wide as the picture */
} DriveMirror;

typedef struct {
    const Tri *tri;
    const TrackArt *art;
    const RaceState *race;
    const TrackRender *track;   /* 0043f540's cache and list part already run this frame (ported_view_stream) */
    int32_t *rows_again;        /* chase +0x3c (004f8058): the mirror's rows to be made again; cleared */
    int32_t near_dir[3];        /* 00512040..: as the front view last left them */
    const Car *followed;        /* cars[004fbb08] */
    void *user;
    /* 004591d4's mirror pass for a car with the mirror's view into the picture */
    void (*draw_car)(void *user, Car *car, const ModelView *mv, const QuadTarget *target);
} DriveMirrorWorld;

/* One frame's mirror pass: the picture, the mirror camera's layout and angles, the renderer's cache. */
typedef struct {
    DriveMirror *m;
    const DriveMirrorWorld *w;
    QuadTarget target;          /* the picture */
    const ViewLayout *layout;   /* the mirror camera's */
    const uint8_t *cache;       /* 005041d8, the renderer's */
    int32_t segment_mask;       /* 00504210 */
    int32_t pitch, yaw, roll;
} DriveMirrorFrame;

/* 0044bd64 at the race's start, once the cockpit is loaded; 0 without one. */
int drive_mirror_start(DriveMirror *m, const Cockpit *c);

/* 0043f540's mirror rows from the cache w->track holds, the pass set up in `f` for 0044cc74's mirror block
 * (mirror_frame, with drive_mirror_ops); 0 without a picture. */
int drive_mirror_begin(DriveMirror *m, const DriveMirrorWorld *w, DriveMirrorFrame *f);

/* mirror_frame's ops on the pass `f` (the front view is the port's own: view(0) does nothing). */
MirrorFrameOps drive_mirror_ops(DriveMirrorFrame *f);

/* port: the wide screen's mirror: the picture half again as wide (a wider field at the same focal length) from now
 * on, `m`'s too when it has one; 0 back to the cockpit's own. The cockpit's frame keeps the original's width: only
 * the middle of the picture, the original's view, is placed on it (drive_mirror_place). */
void drive_mirror_widen(DriveMirror *m, int on);

/* 0044c060: the picture at x, y on the screen target (a widened one's middle, as wide as the original's). */
void drive_mirror_place(const DriveMirror *m, const QuadTarget *target, int32_t x, int32_t y);

/* 0044bdbc. */
void drive_mirror_free(DriveMirror *m);

#endif
