/* The light editor's aim (view_pick.c) on a synthetic art: the polygon the aim point falls in is found by its texture (a road slot's variant or a scenery board), the last one
 * drawn over the point wins (the painter's order: the nearest), screen and camera-space quads alike, nothing when it is off or nothing covers the point. */
#include "game/viewer/view_pick.h"

#include "test.h"

#include <string.h>

static TrackArt art;

static void reset_art(void)
{
    memset(&art, 0, sizeof art);
    art.slots[38][0].present = 1, art.slots[38][2].present = 1, art.scenery[5].present = 1;
}

static View view_at(int32_t cx, int32_t cy)
{
    View v;
    memset(&v, 0, sizeof v);
    v.centre_x = cx, v.centre_y = cy, v.focal_x = 32, v.focal_y = -32, v.near_z = 1;
    return v;
}

static const int32_t AROUND[8] = { 20, 20, 40, 20, 40, 40, 20, 40 };   /* covers (32, 32) */
static const int32_t BESIDE[8] = { 50, 20, 60, 20, 60, 40, 50, 40 };   /* does not */

static void test_off_finds_nothing(void)
{
    reset_art();
    const View v = view_at(32, 32);
    view_pick_set(0);
    view_pick_frame_start(&v, &art, 64, 64);
    view_pick_note(&art.slots[38][0].bitmap, AROUND);
    int slot = -1, variant = -1;
    CHECK_EQ_INT(view_pick_found(&slot, &variant), VIEW_PICK_NONE);
}

static void test_the_last_polygon_over_the_aim_wins(void)
{
    reset_art();
    const View v = view_at(32, 32);
    view_pick_set(1);
    view_pick_frame_start(&v, &art, 64, 64);
    int slot = -1, variant = -1, x = 0, y = 0;
    CHECK_EQ_INT(view_pick_found(&slot, &variant), VIEW_PICK_NONE);   /* nothing drawn yet */
    CHECK(view_pick_aim(&x, &y) && x == 32 && y == 32);
    view_pick_note(&art.scenery[5].bitmap, AROUND);
    CHECK_EQ_INT(view_pick_found(&slot, &variant), VIEW_PICK_SCENERY);
    CHECK_EQ_INT(slot, 5);
    view_pick_note(&art.slots[38][2].bitmap, AROUND);   /* drawn after: nearer */
    CHECK_EQ_INT(view_pick_found(&slot, &variant), VIEW_PICK_SLOT);
    CHECK_EQ_INT(slot, 38);
    CHECK_EQ_INT(variant, 2);
    view_pick_note(&art.slots[38][0].bitmap, BESIDE);   /* not over the aim: kept as it was */
    CHECK_EQ_INT(view_pick_found(&slot, &variant), VIEW_PICK_SLOT);
    CHECK_EQ_INT(variant, 2);
    static const int32_t MIRRORED[8] = { 40, 20, 20, 20, 20, 40, 40, 40 };   /* the other winding */
    view_pick_note(&art.slots[38][0].bitmap, MIRRORED);
    CHECK_EQ_INT(view_pick_found(&slot, &variant), VIEW_PICK_SLOT);
    CHECK_EQ_INT(variant, 0);
    FshEntry stray;
    memset(&stray, 0, sizeof stray);
    view_pick_note(&stray, AROUND);   /* a texture of no slot (a car's): the aim is on something else */
    CHECK_EQ_INT(view_pick_found(&slot, &variant), VIEW_PICK_OTHER);
    view_pick_frame_start(&v, &art, 64, 64);   /* a new frame forgets */
    CHECK_EQ_INT(view_pick_found(&slot, &variant), VIEW_PICK_NONE);
    view_pick_set(0);
}

static void test_camera_quads_are_projected(void)
{
    reset_art();
    const View v = view_at(32, 32);
    view_pick_set(1);
    view_pick_frame_start(&v, &art, 64, 64);
    /* z 32, focal 32: screen x = 32 + x, y = 32 - y: AROUND */
    const int32_t c0[3] = { -12, 12, 32 }, c1[3] = { 8, 12, 32 }, c2[3] = { 8, -8, 32 }, c3[3] = { -12, -8, 32 };
    const int32_t *const cam[4] = { c0, c1, c2, c3 };
    view_pick_note_cam(&art.slots[38][0].bitmap, &v, cam);
    int slot = -1, variant = -1;
    CHECK_EQ_INT(view_pick_found(&slot, &variant), VIEW_PICK_SLOT);
    CHECK_EQ_INT(slot, 38);
    /* a corner behind the near plane: the near road the camera stands on; it covers the aim only if it is the only thing there, and it is not projected: left out */
    view_pick_frame_start(&v, &art, 64, 64);
    const int32_t behind[3] = { -12, -8, 0 };
    const int32_t *const cut[4] = { c0, c1, c2, behind };
    view_pick_note_cam(&art.slots[38][0].bitmap, &v, cut);
    CHECK_EQ_INT(view_pick_found(&slot, &variant), VIEW_PICK_NONE);
    view_pick_set(0);
}

/* the aim follows the mouse: the pointer on the shown frame (its own size) is mapped to the 3D's target at the next frame's start; none given, the view's centre */
static void test_the_aim_is_the_pointer(void)
{
    reset_art();
    const View v = view_at(32, 32);
    view_pick_set(1);
    view_pick_pointer(50, 30, 128, 64);   /* a frame twice the target's width: x 50 is the target's 25 */
    view_pick_frame_start(&v, &art, 64, 64);
    int x = 0, y = 0;
    CHECK(view_pick_aim(&x, &y) && x == 25 && y == 30);
    static const int32_t AT_POINTER[8] = { 20, 25, 30, 25, 30, 35, 20, 35 };   /* covers (25, 30), not the centre (32, 32) */
    view_pick_note(&art.slots[38][0].bitmap, AROUND);
    int slot = -1, variant = -1;
    CHECK_EQ_INT(view_pick_found(&slot, &variant), VIEW_PICK_SLOT);   /* AROUND covers both */
    view_pick_frame_start(&v, &art, 64, 64);
    view_pick_note(&art.scenery[5].bitmap, AT_POINTER);
    view_pick_note(&art.slots[38][0].bitmap, BESIDE);
    CHECK_EQ_INT(view_pick_found(&slot, &variant), VIEW_PICK_SCENERY);
    view_pick_pointer(0, 0, 0, 0);   /* no frame size: back to the centre */
    view_pick_frame_start(&v, &art, 64, 64);
    CHECK(view_pick_aim(&x, &y) && x == 32 && y == 32);
    view_pick_set(0);
}

int main(void)
{
    test_the_aim_is_the_pointer();
    test_off_finds_nothing();
    test_the_last_polygon_over_the_aim_wins();
    test_camera_quads_are_projected();
    return TEST_REPORT();
}
