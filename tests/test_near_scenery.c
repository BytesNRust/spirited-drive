/* port: the scenery the wide view shows past the exe's culls. A model whose centre is off the exe's 90 degree cone
 * (00458c04) and a board with a point nearer than 0x86 (0044a4b0) still reach the screen's sides on a wide field;
 * with the port's flags they are drawn (the board through its camera-space corners, clipped at the near plane),
 * without them the exe's culls stand. */
#include "game/gfx/object3d.h"
#include "game/race/model_draw.h"
#include "game/race/track_object_draw.h"
#include "test.h"

#include <string.h>

/* --- models --- */

static int model_drawn(int all_sides, int32_t x, int32_t z)
{
    static View view;
    view_init(&view, 0, 0, 1120, 480, 102, 70);
    static Object3D obj;
    memset(&obj, 0, sizeof obj);   /* no points, no polygons: only the cull is under test */
    const int32_t cam[3] = { 0, 0, 0 }, pos[3] = { x * 4096, 0, z * 4096 };
    const int32_t m[9] = { 0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x10000 };
    const ModelView mv = { &view, cam, m, 0, all_sides };
    const Object3DSink sink = { NULL, NULL, NULL, NULL, NULL };
    return model_draw_object(&obj, pos, 0, &mv, &sink);
}

/* a centre 2 z out to the side (63 degrees: on a 21:9 screen's edge with the field widened) and past the exe's
 * margin: the exe culls it, the port draws it; behind the eye or past the far depth both cull */
static void test_model_sides(void)
{
    CHECK_EQ_INT(model_drawn(0, 0xa00, 0x500), 0);
    CHECK_EQ_INT(model_drawn(1, 0xa00, 0x500), 1);
    CHECK_EQ_INT(model_drawn(0, 0x100, 0x500), 1);
    CHECK_EQ_INT(model_drawn(1, 0, MODEL_MIN_Z - 1), 0);
    CHECK_EQ_INT(model_drawn(1, 0, MODEL_MAX_Z + 1), 0);
}

/* --- boards --- */

typedef struct {
    int full, alt, camera, near;
    const int32_t *cam[4];
} Boards;

static void b_model(void *u, int32_t m, const int32_t p[3], int32_t a) { (void)u, (void)m, (void)p, (void)a; }
static void b_full(void *u, int32_t t, const int32_t s[8]) { (void)t, (void)s, ((Boards *)u)->full++; }
static void b_alt(void *u, int32_t t, const int32_t s[8]) { (void)t, (void)s, ((Boards *)u)->alt++; }
static void b_camera(void *u, int32_t t, const int32_t *const cam[4]) { (void)t, (void)cam, ((Boards *)u)->camera++; }
static void b_near(void *u, int32_t t, const int32_t *const cam[4])
{
    (void)t;
    Boards *b = u;
    b->near++;
    for (int i = 0; i < 4; i++) b->cam[i] = cam[i];
}

/* a sign along the road on the right: its near edge 0x40 deep (under 0x86, its screen point lost), its far edge on
 * the screen */
static const int32_t SIGN[4][3] = { { 676, 50, 373 }, { 676, 50, 0x40 }, { 676, -50, 0x40 }, { 676, -50, 373 } };
static const int32_t SIGN_SCREEN[4][2] = { { 1078, 100 }, { INT32_MIN, INT32_MIN }, { INT32_MIN, INT32_MIN }, { 1078, 200 } };

static Boards billboard(int modern, int near)
{
    uint8_t descriptor[TRACK_WALK_RECORD_SIZE] = { 0 }, record[TRACK_WALK_RECORD_SIZE] = { 0 };
    descriptor[1] = 4, descriptor[2] = 9;
    TrackObjectWalk walk = { descriptor, record, 1, 0, 1, 4, 4, 0, 0, 4, 0, 0, 0 };
    Boards b = { 0 };
    TrackObjectDraw d = { &walk, NULL, NULL, SIGN, SIGN_SCREEN, 4, 0, 0,
                          { &b, b_model, b_full, b_alt, modern ? b_camera : NULL, near ? b_near : NULL, NULL, NULL } };
    track_object_draw(&d, 10, 1);
    return b;
}

static void test_near_billboard(void)
{
    for (int modern = 0; modern < 2; modern++) {
        const Boards exe = billboard(modern, 0);
        CHECK_EQ_INT(exe.full + exe.alt + exe.camera + exe.near, 0);   /* 0044a4b0: a point under 0x86, no board */
        const Boards port = billboard(modern, 1);
        CHECK_EQ_INT(port.near, 1);
        CHECK_EQ_INT(port.full + port.alt + port.camera, 0);
        for (int i = 0; i < 4; i++) CHECK(port.cam[i] == SIGN[i]);
    }
}

/* a two-sided board (a building's front and side) with its side's far points shallow: both faces through the near
 * call, the side's corners 1, 4, 5, 2; with every point deep the exe's path as before */
static void test_near_two_sided(void)
{
    uint8_t two[TRACK_WALK_RECORD_SIZE] = { 0 }, record[TRACK_WALK_RECORD_SIZE] = { 0 };
    two[1] = 6, two[2] = 3, two[3] = 5;
    static const int32_t cam6[6][3] = { { -100, 50, 400 }, { 100, 50, 400 }, { 100, -50, 400 }, { -100, -50, 400 },
                                        { 100, 50, 0x40 }, { 100, -50, 0x40 } };
    static const int32_t scr6[6][2] = { { 100, 100 }, { 200, 100 }, { 200, 200 }, { 100, 200 },
                                        { INT32_MIN, INT32_MIN }, { INT32_MIN, INT32_MIN } };
    for (int near = 0; near < 2; near++) {
        TrackObjectWalk walk = { two, record, 1, 0, 1, 6, 6, 0, 0, 6, 0, 0, 0 };
        Boards b = { 0 };
        TrackObjectDraw d = { &walk, NULL, NULL, cam6, scr6, 6, 0, 0,
                              { &b, b_model, b_full, b_alt, NULL, near ? b_near : NULL, NULL, NULL } };
        track_object_draw(&d, 10, 1);
        CHECK_EQ_INT(b.near, near ? 2 : 0);
        CHECK_EQ_INT(b.alt, 0);
        if (near) CHECK(b.cam[0] == cam6[1] && b.cam[1] == cam6[4] && b.cam[2] == cam6[5] && b.cam[3] == cam6[2]);
        CHECK_EQ_INT(d.side_face, (uint32_t)near);
    }
    static const int32_t deep6[6][3] = { { -100, 50, 400 }, { 100, 50, 400 }, { 100, -50, 400 }, { -100, -50, 400 },
                                         { 100, 50, 600 }, { 100, -50, 600 } };
    static const int32_t deep_scr[6][2] = { { 100, 100 }, { 200, 100 }, { 200, 200 }, { 100, 200 }, { 220, 110 }, { 220, 190 } };
    TrackObjectWalk walk = { two, record, 1, 0, 1, 6, 6, 0, 0, 6, 0, 0, 0 };
    Boards b = { 0 };
    TrackObjectDraw d = { &walk, NULL, NULL, deep6, deep_scr, 6, 0, 0, { &b, b_model, b_full, b_alt, NULL, b_near, NULL, NULL } };
    track_object_draw(&d, 10, 1);
    CHECK_EQ_INT(b.alt, 2);
    CHECK_EQ_INT(b.near, 0);
}

/* a big tree's board: every corner deep (past 0x86) but one's screen x past the exe's +-0x800 limit: the exe drops it whole; with the
 * port's wide-board flag the near call draws it (clipped at the near plane), as long as there is one */
static void test_wide_board(void)
{
    static const int32_t cam4[4][3] = { { -9000, 400, 500 }, { 300, 400, 900 }, { 300, -400, 900 }, { -9000, -400, 500 } };
    static const int32_t scr4[4][2] = { { -3000, 100 }, { 600, 100 }, { 600, 300 }, { -3000, 300 } };
    uint8_t descriptor[TRACK_WALK_RECORD_SIZE] = { 0 }, record[TRACK_WALK_RECORD_SIZE] = { 0 };
    descriptor[1] = 4, descriptor[2] = 9;
    for (int flag = 0; flag < 2; flag++)
        for (int near = 0; near < 2; near++) {
            track_port_wide_board = flag;
            TrackObjectWalk walk = { descriptor, record, 1, 0, 1, 4, 4, 0, 0, 4, 0, 0, 0 };
            Boards b = { 0 };
            TrackObjectDraw d = { &walk, NULL, NULL, cam4, scr4, 4, 0, 0, { &b, b_model, b_full, b_alt, NULL, near ? b_near : NULL, NULL, NULL } };
            track_object_draw(&d, 10, 1);
            CHECK_EQ_INT(b.near, flag && near ? 1 : 0);
            CHECK_EQ_INT(b.full + b.alt + b.camera, 0);
            if (flag && near) CHECK(b.cam[0] == cam4[0] && b.cam[2] == cam4[2]);
        }
    track_port_wide_board = 0;
}

int main(void)
{
    test_model_sides();
    test_near_billboard();
    test_near_two_sided();
    test_wide_board();
    return TEST_REPORT();
}
