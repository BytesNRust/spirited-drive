/* The race's views: the cameras' viewports (00405154, 00405210). */
#ifndef TNFS_RACE_VIEW_H
#define TNFS_RACE_VIEW_H

#include "game/race/horizon.h"
#include "game/race/view.h"

#include <stdint.h>

enum {
    VIEW_SCREEN_MODES = 2,      /* 00512018 */
    VIEW_LAYOUTS = 17,          /* 0x2a8 bytes a mode */
    VIEW_ROWS = 7,              /* 004b75d0: 0x16c bytes a mode */
    VIEW_SIZES = 4,             /* 00511ff0: the window size picks a row's layout */
    VIEW_SIZE_LARGEST = 2       /* 00405370 holds the window size to it */
};

/* A row of 004b7080 (0x28 bytes): where a viewport sits on the screen.
 * 004055d0 and 00405630 change some rows at the race's start. */
typedef struct {
    int32_t unknown_00;
    int32_t scale;              /* +4: the horizon's vertical scale, 16.16 (0043a028) */
    int32_t x, y;               /* +8, +0xc: added to left, top */
    int32_t width, height;      /* +0x10, +0x14 */
    int32_t left, top;          /* +0x18, +0x1c */
    int32_t unknown_20, unknown_24;
} ViewLayout;

/* A row of 004b75d0 (0x34 bytes): a view and its layout by window size. */
typedef struct {
    int32_t next;               /* +0: the view 00405324 moves on to, -1 none */
    int32_t layouts[VIEW_SIZES];  /* +4: the 004b7080 row for each window size (00511ff0) */
    int32_t unknown_14[8];
} ViewRow;

typedef struct {
    ViewLayout layouts[VIEW_SCREEN_MODES][VIEW_LAYOUTS];  /* 004b7080 */
    ViewRow rows[VIEW_SCREEN_MODES][VIEW_ROWS];           /* 004b75d0 */
    int32_t mode;               /* 00512018: the screen mode */
    int32_t changed;            /* 00511fec: set when a camera takes a view */
    View view;                  /* the projection (004c6224..), 00475e44's reciprocals */
    HorizonTables horizon;      /* 0043a028's */
} RaceViews;

/* 004b7080 and 004b75d0 as the exe starts (race_view_tables.c, generated). */
extern const ViewLayout race_view_layouts[VIEW_SCREEN_MODES][VIEW_LAYOUTS];
extern const ViewRow race_view_rows[VIEW_SCREEN_MODES][VIEW_ROWS];

/* A camera record (0x20 bytes: 004b78a8, 004b78c8, 004b78e8, 004f801c). */
typedef struct {
    int32_t fov;                /* +0: degrees, across and down */
    int32_t unknown_04, unknown_08;
    int32_t view;               /* +0xc: its 004b75d0 row (00405210) */
    int32_t layout;             /* +0x10: its 004b7080 row */
    int32_t unknown_14;         /* +0x14: the reciprocal table it projects with (00405630) */
    const ViewRow *view_row;    /* +0x18: its 004b75d0 row (00405210) */
    const ViewLayout *layout_row;  /* +0x1c */
} RaceCamera;

/* 00405154: `camera` takes `layout` (a row of views->layouts[mode]); the
 * horizon tables and the projection are set for it (0043a028, then 00405284
 * and 00475e44: the viewport at left + x, top + y, the camera's fov both
 * ways). */
void race_view_layout(RaceViews *views, RaceCamera *camera, int32_t layout);

/* 00405210: `camera` takes view row `view` and that row's layout for
 * window size `size` (00405154); views->changed is set. */
void race_view_select(RaceViews *views, RaceCamera *camera, int32_t view, int32_t size);

/* 00405568 (watcall (view, size), from 0044cc74), on the follow record's
 * camera (00511fe0 +0x88): view <= -1: 0, nothing done; else 00405210's
 * body (race_view_select) and 1. */
int32_t race_view_choose(RaceViews *views, RaceCamera *camera, int32_t view, int32_t size);

/* 00405260 (watcall ()), on the follow record's camera (00511fe0 +0x88) and
 * the window size 00511ff0: its own view selected again (00405210) unless
 * the view is negative. */
void race_view_reselect(RaceViews *views, RaceCamera *camera, int32_t size);

/* 00405324 (watcall ()), on the follow record's camera (00511fe0 +0x88) and
 * the window size 00511ff0: the next view of its view's row (row +0, read
 * by the camera's +0xc) selected (00405210) when it is not negative. */
void race_view_next(RaceViews *views, RaceCamera *camera, int32_t size);

/* 00405370 (watcall size), on the follow record's camera: size held to 2;
 * unless it is *window_size (00511ff0) already, *window_size = size and the
 * camera takes its view row's layout for that size (00405154). Its callers
 * (00438628, 00441117) pass a byte. */
void race_view_size(RaceViews *views, RaceCamera *camera, int32_t *window_size, int32_t size);

/* 004054cc (eax span, edx size; from 004279df, 00441091, 004410e5,
 * 0044110b, 00447c93): *size = the window size (00511ff0), then *span =
 * the second camera record's (004b78c8) +0xc - +8. */
void race_view_window(const RaceCamera *second, int32_t window_size, int32_t *span, int32_t *size);

/* The follow record's two cameras and their matrices: +0x88 and +0x94 ahead,
 * +0x8c and +0xb8 behind (the mirror). */
typedef struct {
    const RaceCamera *front, *rear;
    const int32_t *front_matrix, *rear_matrix;
} RaceCameraPair;

/* 0040586c (watcall (chase, rear)): the camera (into 00511fe8) and matrix
 * (00511fe4) for the side; the projection set from the camera's layout box
 * (+0x1c: x + left, y + top, width, height) and fov both ways (00475f68,
 * 00475fab; the camera's own reciprocal table, 00475e30, being the one
 * 00475e44 makes for it); ahead the centre moves by 004f8010 / 004f8014;
 * then both centres shift left by 004c2d84. */
void race_camera_view(View *view, const RaceCameraPair *pair, int rear, const int32_t shake[2], int32_t screen_shift,
                      const RaceCamera **camera, const int32_t **matrix);

typedef struct {
    void *ctx;
    void (*layout)(void *ctx, RaceCamera *camera, int32_t layout);  /* 00405154 */
} RaceViewScreenOps;

/* 0040540c (watcall (step, size), from 0044ca3c with the bytes 004fb784 and
 * 004fb789): size held to 2. The first camera (004b78a8): view (+0xc) =
 * its +8, *window_size (00511ff0) = size, its row (+0x18) that view's of
 * the mode, 00405154 on the row's layout for size. The second (004b78c8):
 * view = its +8 + step, its row, views->changed (00511fec) = 1, 00405154 on
 * the row's layout for *window_size; changed = 1 again. The rows are
 * indexed unchecked (the port's callers keep the views 0..6). */
void race_view_screen(const RaceViewScreenOps *ops, RaceViews *views, RaceCamera *first, RaceCamera *second,
                      int32_t step, int32_t size, int32_t *window_size);

#endif
