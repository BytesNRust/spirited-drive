/* The scenery objects drawn as the original walks them, back from the
 * last placed one (track_object_walk.h): a model through the 3D engine, a
 * billboard or a two-sided board through the texture mapper's pointers.
 *
 *   track_object_draw  0044a4b0 (with 0044a450, track_quad_on_screen) */
#ifndef TNFS_TRACK_OBJECT_DRAW_H
#define TNFS_TRACK_OBJECT_DRAW_H

#include "game/race/track_object_walk.h"

#include <stdint.h>

enum {
    TRACK_OBJECT_BANDS = 0x78    /* 004c2da4 / 004c2e1c: one byte per band */
};

typedef struct {
    void *user;
    /* 00458c04: model 005150f0 + model x 0x138 at a world position, turned by angle */
    void (*model)(void *user, int32_t model, const int32_t position[3], int32_t angle);
    /* [004f9e9c]: the whole texture, the dword 005100c4[texture] */
    void (*full)(void *user, int32_t texture, const int32_t screen[8]);
    /* [004f9e90]: the same for the faces of two-sided boards */
    void (*full_alt)(void *user, int32_t texture, const int32_t screen[8]);
    /* port: Modern 3D's, in place of full and full_alt when set (NULL the exe's): the board's corners in camera
     * space, the points' rows of object_camera, in the order the screen corners would go */
    void (*full_camera)(void *user, int32_t texture, const int32_t *const cam[4]);
    /* port: a board with a point nearer than 0x86 (the exe drops it whole) through its camera-space corners, clipped
     * at the near plane: a wide field shows its far end at the screen's side; NULL the exe's */
    void (*port_near_camera)(void *user, int32_t texture, const int32_t *const cam[4]);
} TrackObjectDrawOps;

typedef struct {
    TrackObjectWalk *walk;
    const uint8_t *nodes;               /* 00505038 */
    const int32_t (*object_world)[3];   /* 004fd134 */
    const int32_t (*object_camera)[3];  /* 004ffb64 */
    const int32_t (*object_screen)[2];  /* 00502594 */
    int32_t object_capacity;
    int32_t tick;                       /* 00513e70: the animation counter */
    uint32_t side_face;                 /* 00511fac: 1 while a board's second face is drawn */
    TrackObjectDrawOps ops;
} TrackObjectDraw;

/* 0044a4b0 (watcall band, visible): the record under the walker. A model
 * (descriptor type 1): its point taken back off the object points and drawn
 * turned by the record's spin << 16 plus the walker's node heading << 10.
 * Else its type's points taken back; with `visible` and their first four
 * at least 0x86 deep: a billboard (4) with texture descriptor +2 plus the
 * band's level (004c2da4 when record +8 is set, else 004c2e1c), animated
 * (flag 4) by tick / +9 % +8 frames of 4; a two-sided board (6, points 4 and
 * 5 deep too) its front (+2, corners from 1 with flag 1) and, side_face set
 * for it, its side (+3; corners 1, 4, 5, 2, or 4, 5, 2, 1 with flag 2), each
 * when on screen. Then the walker one record back, wrapping to the last on
 * a circuit unless that is on the drawn record's node. */
void track_object_draw(TrackObjectDraw *d, int32_t band, int visible);

#endif
