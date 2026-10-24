/* The mirror's road rows against the camera, over frames with interpolation (drive_interp): between two steps a frame
 * shows the older node, so a row set made on the step's flag and read a frame later is a segment behind the scene, and
 * the exe's (node + 3) / 4 ends the rows at the camera's own segment when node % 4 == 0, behind the eye. */
#include "game/race/mirror_scene.h"
#include "game/race/track_step.h"
#include "test.h"

enum { MASK = 255, ROWS = 5, FRAMES_PER_STEP = 3, STEPS = 238 };   /* 714 frames, as the capture that showed the holes */

typedef struct { int32_t first, made; } Rows;

static void step_none(void *ctx, int32_t segment) { (void)ctx, (void)segment; }
static void build_none(void *ctx, int32_t node, int32_t segment, int32_t count)
{
    (void)ctx, (void)node, (void)segment, (void)count;
}
static void row_op(void *ctx, int32_t segment) { ((Rows *)ctx)->made++, (void)segment; }

/* one 0043f540 for a frame whose camera is at `node`; `again` is the chase's flag, cleared by it */
static void step(Rows *r, int32_t node, int32_t *again, int eye_ahead)
{
    int32_t w4c53a4 = 0, count = 0;
    const TrackStepOps ops = { r, step_none, build_none, row_op };
    const TrackStepInputs in = { node, again, node, &r->first, &w4c53a4, &count, 1, MASK, 0, eye_ahead };
    track_stream_step(&ops, &in);
}

/* a hole: the rows' five segments do not reach past the camera's own, so road between the eye and the rows' end is
 * missing (the rows being first .. first + 4) */
static int hole(const Rows *r, int32_t node) { return ((r->first + ROWS - 1) & MASK) <= ((node / 4) & MASK); }

/* the scene's first segment (mirror_scene) is the rows' first for the same node: one function */
static void test_scene_and_rows_start_together(void)
{
    for (int32_t node = 0; node < 400; node++)
        for (int ahead = 0; ahead <= 1; ahead++) {
            Rows r = { -1, 0 };
            int32_t again = 1;
            step(&r, node, &again, ahead);
            CHECK_EQ_INT(r.first, track_mirror_first(node, MASK, ahead));
        }
}

/* the exe's: a camera on the first node of a segment (node % 4 == 0) has the rows end at its own segment */
static void test_exe_rows_end_at_the_eye_on_a_segment_start(void)
{
    for (int32_t node = 40; node < 200; node += 4) {
        Rows r = { -1, 0 };
        int32_t again = 1;
        step(&r, node, &again, 0);
        CHECK(hole(&r, node));
        CHECK_EQ_INT(*&again, 0);
    }
}

/* port: from the next node, every node has the rows past the eye */
static void test_port_rows_pass_the_eye_at_every_node(void)
{
    for (int32_t node = 40; node < 200; node++) {
        Rows r = { -1, 0 };
        int32_t again = 1;
        step(&r, node, &again, 1);
        CHECK(!hole(&r, node));
    }
}

/* The frames of a drive with interpolation: the sim's node steps once in FRAMES_PER_STEP frames and sets the rows' flag,
 * and the first frame of the step still draws the older node (drive_interp) while it takes the flag. The port's pattern
 * checks the rows every frame at the frame's own node; the exe's reads the flag once, at whatever node that frame has,
 * and the next frames (the new node, the scene from it) keep rows made for the older one. */
static int holes_over_frames(int eye_ahead, int every_frame)
{
    Rows r = { -1, 0 };
    int32_t again = 1, holes = 0;
    for (int32_t s = 0; s < STEPS; s++)
        for (int f = 0; f < FRAMES_PER_STEP; f++) {
            const int32_t node = 40 + s - (f == 0);   /* the frame's node, the scene's too */
            if (f == 0) again = 1;                    /* the step sets the flag */
            if (every_frame) again = 1;
            step(&r, node, &again, eye_ahead);
            if (r.first != track_mirror_first(node, MASK, eye_ahead) || hole(&r, node)) holes++;
        }
    return holes;
}

static void test_no_holes_over_714_interpolated_frames(void)
{
    CHECK_EQ_INT(holes_over_frames(1, 1), 0);   /* the port: rows every frame, one node further */
    CHECK(holes_over_frames(0, 0) > 0);         /* the exe's flag and node: holes (what the player saw) */
    CHECK(holes_over_frames(1, 0) > 0);         /* the further node alone does not mend the stale rows */
    CHECK(holes_over_frames(0, 1) > 0);         /* nor does the check every frame without it */
}

/* A car in the mirror over the frames of a tick: the tick moves the cars on and makes the rows for its camera node, the
 * frame after it draws from the older node (drive_interp). The rows kept from the tick are one off for that frame and the
 * car is not drawn; made again for the frame's node they find it. */
static int draws;
static void prepare_op(void *ctx, Car *car, int32_t pass) { (void)ctx, (void)car, (void)pass; }
static void draw_op(void *ctx, Car *car, int32_t pass) { (void)car, (void)pass, (*(int *)ctx)++; }

static int car_seen_from(const uint8_t rows[MIRROR_CAR_ROWS], Car *car, int32_t camera_node)
{
    Car *const list[1] = { car };
    const MirrorCarsOps ops = { &draws, prepare_op, draw_op };
    const MirrorCarsInputs in = { camera_node, 64, rows, list, 1, NULL, 1 };
    draws = 0;
    for (int32_t node = camera_node - 10; node <= camera_node; node++) mirror_cars(&ops, &in, node);
    return draws;
}

static void test_car_rows_follow_the_frames_camera(void)
{
    Car car = { 0 };
    Car *const list[1] = { &car };
    uint8_t tick_rows[MIRROR_CAR_ROWS], again[MIRROR_CAR_ROWS];
    car.draw_node2 = 100;   /* the tick's camera at node 103: three behind it */
    mirror_car_rows(list, 1, 64, 103, tick_rows);
    CHECK_EQ_INT(car_seen_from(tick_rows, &car, 103), 1);
    /* the next frame is drawn from the older node 102: the kept rows say two behind where the car is */
    CHECK_EQ_INT(car_seen_from(tick_rows, &car, 102), 0);
    mirror_car_rows_at(list, 1, 64, 102, again);
    CHECK_EQ_INT(car_seen_from(again, &car, 102), 1);
    CHECK_EQ_INT(car.draw_node2, 100);   /* nothing was moved on */
    mirror_car_rows_at(list, 1, 64, 103, again);
    CHECK_EQ_INT(car_seen_from(again, &car, 103), 1);
}

int main(void)
{
    test_car_rows_follow_the_frames_camera();
    test_scene_and_rows_start_together();
    test_exe_rows_end_at_the_eye_on_a_segment_start();
    test_port_rows_pass_the_eye_at_every_node();
    test_no_holes_over_714_interpolated_frames();
    return TEST_REPORT();
}
