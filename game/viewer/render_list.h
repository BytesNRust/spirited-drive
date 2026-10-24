/* port (render bands P2, re/notes/render-bands-design.md): the road's draws recorded as they are called and replayed
 * per band of rows. Each command holds copies of everything its draw reads (the corners, the uv, the camera-space
 * points) and the texture by pointer; a band replays the commands whose rows touch it, in recorded order, with the
 * box narrowed to the band, so the pixels are those of the draws made at once (tests/test_band_exact.c). */
#ifndef TNFS_RENDER_LIST_H
#define TNFS_RENDER_LIST_H

#include "formats/fsh.h"
#include "game/gfx/quad.h"
#include "game/race/view.h"

#include <stdint.h>

typedef enum {
    RENDER_QUAD,         /* quad_draw: uv and screen corners */
    RENDER_FULL,         /* quad_draw_full: the whole texture on the screen corners */
    RENDER_FULL_TRI,     /* quad_draw_full_tri */
    RENDER_ROAD_CELL     /* modern_road_cell: uv and four camera-space corners */
} RenderKind;

typedef struct {
    RenderKind kind;
    int transparent;
    const FshEntry *texture;
    const View *view;     /* RENDER_ROAD_CELL */
    int32_t uv[8];
    int32_t screen[8];
    int32_t cam[4][3];
    int32_t top, bottom;  /* the rows it can touch: [top, bottom) */
} RenderCmd;

enum { RENDER_LIST_MAX = 4096 };   /* a full list is flushed before the next command is added */

typedef struct {
    const QuadTarget *target;   /* where the commands draw (set by render_list_begin) */
    int count;
    RenderCmd cmds[RENDER_LIST_MAX];
} RenderList;

/* Empty list drawing into `target` (which must outlive the list's commands). */
void render_list_begin(RenderList *l, const QuadTarget *target);

void render_list_add_quad(RenderList *l, const FshEntry *texture, const int32_t uv[8], const int32_t screen[8],
                          int transparent);
void render_list_add_full(RenderList *l, const FshEntry *texture, const int32_t screen[8], int transparent);
void render_list_add_full_tri(RenderList *l, const FshEntry *texture, const int32_t screen[8], int transparent);
void render_list_add_road_cell(RenderList *l, const View *view, const FshEntry *texture, const int32_t uv[8],
                               const int32_t *const cam[4], int transparent);

/* Replays the commands touching [top, bottom) into the target narrowed to those rows (and to its own box). */
void render_list_replay(const RenderList *l, int top, int bottom);

/* Replays the whole list on bands_threads() threads (one plain replay for a short list or with no threads), then
 * empties it. */
void render_list_flush(RenderList *l);

#endif
