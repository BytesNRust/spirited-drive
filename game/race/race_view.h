/* The race's views: the cameras' viewports (00405154, 00405210). */
#ifndef TNFS_RACE_VIEW_H
#define TNFS_RACE_VIEW_H

#include "game/race/horizon.h"
#include "game/race/view.h"

#include <stdint.h>

enum {
    VIEW_SCREEN_MODES = 2,      /* 00512018 */
    VIEW_LAYOUTS = 17           /* 0x2a8 bytes a mode */
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

typedef ViewLayout ViewLayouts[VIEW_SCREEN_MODES][VIEW_LAYOUTS];

/* A camera record (0x20 bytes: 004b78a8, 004b78c8, 004b78e8, 004f801c). */
typedef struct {
    int32_t fov;                /* +0: degrees, across and down */
    int32_t unknown_04, unknown_08;
    int32_t view;               /* +0xc: its 004b75d0 row (00405210) */
    int32_t layout;             /* +0x10: its 004b7080 row */
    int32_t unknown_14;         /* +0x14: the reciprocal table it projects with (00405630) */
    int32_t unknown_18;         /* +0x18: its 004b75d0 row's address (00405210) */
    const ViewLayout *layout_row;  /* +0x1c */
} RaceCamera;

/* 00405154: `camera` takes `layout` (a row of `layouts`[mode]); the horizon
 * tables and the projection are set for it (0043a028, then 00405284 and
 * 00475e44: the viewport at left + x, top + y, the camera's fov both ways). */
void race_view_layout(RaceCamera *camera, int32_t layout, const ViewLayouts *layouts, int32_t mode, View *view,
                      HorizonTables *horizon);

#endif
