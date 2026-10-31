/* port (port_follow_camera, phase 2): the road behind the camera's node (follow_behind.h): the near plane's push along the
 * road direction turned about, and the rail panel that passes the lens clipped to the plane instead of dropped */#include "game/race/track_tables.h"
#include "game/viewer/follow_behind.h"
#include "test.h"

#include <string.h>

enum { NEAR = 62259, SHIFT = 0 };

/* the direction away from a camera that looks back: the lower node less the higher one through the matrix, z at least 0x1999 */
static void test_direction(void)
{
    const int32_t identity[9] = { 0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x10000 };
    int32_t dir[3];
    const int32_t lo[3] = { 100 << 16, 0, 0 }, hi[3] = { 100 << 16, 0, 40 << 16 };
    follow_behind_direction(lo, hi, identity, SHIFT, dir);
    CHECK_EQ_INT(dir[0], 0);
    CHECK_EQ_INT(dir[2], 0x1999);   /* the road runs toward the camera along -z: z is not a distance away, so the floor */
    const int32_t away_lo[3] = { 0, 0, 80 << 16 }, away_hi[3] = { 0, 0, 40 << 16 };
    follow_behind_direction(away_lo, away_hi, identity, SHIFT, dir);
    CHECK_EQ_INT(dir[2], 40 << 16);   /* lower nodes farther along the view: the road runs away from the camera */
    follow_behind_direction(away_lo, away_hi, identity, 1, dir);
    CHECK_EQ_INT(dir[2], 20 << 16);   /* the world's shift */
}

/* a rail panel that passes the lens (corners b top, a top, a, b): clipped to the plane with its corners slid along the
 * edges, so it ends on the plane at the screen's edge, where the exe drops it whole; one wholly short of the plane is
 * not drawn */
static void test_rail_panel(void)
{
    const int32_t far_top[3] = { 400, 300, NEAR * 8 }, far_foot[3] = { 400, 100, NEAR * 8 };
    const int32_t near_top[3] = { -200, 300, NEAR / 2 }, near_foot[3] = { -200, 100, NEAR / 2 };
    const int32_t *const across[4] = { near_top, far_top, far_foot, near_foot };   /* b top, a top, a, b: b is at the lens */
    int32_t out[4][3];
    CHECK(follow_arm_clip_quad(across, NEAR, out));
    for (int c = 0; c < 4; c++) CHECK(out[c][2] >= NEAR);
    CHECK_EQ_INT(out[1][2], far_top[2]);   /* the corners in front stay */
    CHECK_EQ_INT(out[0][2], NEAR);         /* b's slid along its edge to the plane */
    CHECK(out[0][0] > near_top[0] && out[0][0] < far_top[0]);   /* between the corner and the one it slides to */
    const int32_t behind_top[3] = { 5, 300, -NEAR }, behind_foot[3] = { 5, 100, -NEAR };
    const int32_t *const passing[4] = { behind_top, far_top, far_foot, behind_foot };
    CHECK(follow_arm_clip_quad(passing, NEAR, out));
    CHECK_EQ_INT(out[0][2], NEAR);
    CHECK_EQ_INT(out[3][2], NEAR);   /* both ends behind the lens end on the plane */
    const int32_t *const nothing[4] = { behind_top, behind_top, behind_foot, behind_foot };
    CHECK(!follow_arm_clip_quad(nothing, NEAR, out));
}

/* the segments behind the camera's the cache keeps: what the window holds behind its ahead, whatever more the arm asks for (the ring is 64 records: an
 * Ultra view's 58 ahead leave 5 behind, and the farthest of ten behind would read the records of the farthest ahead) */
static void test_kept_behind(void)
{
    TrackCacheWindow w = { 0 };
    w.ahead = 58, w.behind = 63;   /* Ultra: draw_segments 56 + 2 ahead, the exe's 5 behind */
    CHECK_EQ_INT(follow_arm_kept_behind(&w, FOLLOW_ARM_SEGMENTS), 5);
    w.ahead = 30, w.behind = 40;   /* follow_arm_window raised it to 10 behind (the exe's draw distance fits the ring) */
    CHECK_EQ_INT(follow_arm_kept_behind(&w, FOLLOW_ARM_SEGMENTS), 10);
    w.ahead = 30, w.behind = 35;   /* the exe's own */
    CHECK_EQ_INT(follow_arm_kept_behind(&w, FOLLOW_ARM_SEGMENTS), 5);
    CHECK_EQ_INT(follow_arm_kept_behind(&w, 3), 3);   /* asking for fewer than the cache keeps */
    CHECK_EQ_INT(follow_arm_kept_behind(&w, 0), 0);
    w.ahead = 70, w.behind = 60;   /* a window that holds less than its ahead: nothing, not a negative count */
    CHECK_EQ_INT(follow_arm_kept_behind(&w, FOLLOW_ARM_SEGMENTS), 0);
}

int main(void)
{
    test_kept_behind();
    test_direction();
    test_rail_panel();
    return TEST_REPORT();
}
