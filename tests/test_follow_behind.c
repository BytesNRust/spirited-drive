/* port (port_follow_camera, phase 2): the road behind the camera's node (follow_behind.h): the near plane's push along the
 * road direction turned about, and the rail that reaches the camera landing on the plane instead of being dropped */
#include "game/race/track_fence.h"
#include "game/race/track_tables.h"
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

/* a point in front of the lens but nearer than the plane lands on it, its x and y kept along the direction; a point behind
 * the lens stays for the rails (which then drop it) */
static void test_near_points(void)
{
    const int32_t dir[3] = { 0, 0, 0x10000 };
    int32_t pushed[3], rail[3];
    const int32_t near_point[3] = { 100, -50, NEAR / 2 };
    follow_behind_near_points(near_point, NEAR, dir, pushed, rail);
    CHECK_EQ_INT(pushed[2], NEAR);
    CHECK_EQ_INT(pushed[0], 100);
    CHECK_EQ_INT(pushed[1], -50);
    CHECK_EQ_INT(rail[2], NEAR);
    const int32_t in_front[3] = { 7, 8, NEAR * 3 };
    follow_behind_near_points(in_front, NEAR, dir, pushed, rail);
    CHECK(memcmp(pushed, in_front, sizeof pushed) == 0);
    CHECK(memcmp(rail, in_front, sizeof rail) == 0);
    const int32_t behind[3] = { 3, 4, -500 };
    follow_behind_near_points(behind, NEAR, dir, pushed, rail);
    CHECK_EQ_INT(pushed[2], NEAR);
    CHECK_EQ_INT(rail[2], -500);   /* the rail's check sees where it is */
}

/* the rails' stand-ins: the ops count what the fence draws */
static int drawn;
static FshEntry texture;
static const FshEntry *f_texture(void *ctx, int32_t flat) { (void)ctx, (void)flat; return &texture; }
static void f_point(void *ctx, const int32_t world[3], int32_t camera[3], int32_t screen[2])
{
    (void)ctx, (void)world;
    memset(camera, 0, 3 * sizeof camera[0]);
    screen[0] = screen[1] = 10;
}
static void f_sub(void *ctx, const FshEntry *t, int32_t u_level, int32_t u_cell, const int32_t *const cam[4], const int32_t s[8], int32_t near)
{
    (void)ctx, (void)t, (void)u_level, (void)u_cell, (void)cam, (void)s, (void)near;
    drawn++;
}
static void f_full(void *ctx, const FshEntry *t, const int32_t s[8]) { (void)ctx, (void)t, (void)s; drawn++; }

/* a rail whose far end b is nearer than the plane but in front of the lens: dropped with the camera's own points (the
 * exe's), drawn with the rail points; one whose end is behind the lens is dropped either way */
static void test_rail_at_the_camera(void)
{
    const TrackFenceOps ops = { NULL, f_texture, f_point, f_sub, f_full, NULL };
    const TrackUv uv = { 0, 0, 1 };   /* variant 1: drawn whole */
    const uint8_t pair[2] = { 0, 1 };
    const int32_t world[2][3] = { { 0, 0, 0 }, { 0, 0, 0 } };
    const int32_t screen[2][2] = { { 10, 10 }, { 12, 12 } };
    const int32_t dir[3] = { 0, 0, 0x10000 };
    const int32_t raw[2][3] = { { 0, 0, NEAR * 4 }, { 0, 0, NEAR / 2 } };
    int32_t pushed[2][3], rail[2][3];
    for (int i = 0; i < 2; i++) follow_behind_near_points(raw[i], NEAR, dir, pushed[i], rail[i]);
    TrackFenceInputs in = { world, raw, screen, NEAR, { 0, 0, 1000, 1000 } };
    drawn = 0;
    track_fence_draw(&ops, &in, pair, 0, &uv, 0, 1, 1);
    CHECK_EQ_INT(drawn, 0);   /* the exe: dropped */
    in.camera = rail;
    track_fence_draw(&ops, &in, pair, 0, &uv, 0, 1, 1);
    CHECK_EQ_INT(drawn, 1);   /* the rail point just in front of the lens is on the plane: drawn */
    const int32_t behind[2][3] = { { 0, 0, NEAR * 4 }, { 0, 0, -NEAR } };
    int32_t b_pushed[2][3], b_rail[2][3];
    for (int i = 0; i < 2; i++) follow_behind_near_points(behind[i], NEAR, dir, b_pushed[i], b_rail[i]);
    in.camera = b_rail;
    track_fence_draw(&ops, &in, pair, 0, &uv, 0, 1, 1);
    CHECK_EQ_INT(drawn, 1);   /* behind the lens: dropped */
}

int main(void)
{
    test_direction();
    test_near_points();
    test_rail_at_the_camera();
    return TEST_REPORT();
}
