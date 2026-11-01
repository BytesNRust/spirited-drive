/* port (port_follow_camera): the arm asks the sink for the cars of every row once, none skipped. The pass draws the rows behind the
 * camera's segment with the cars lagging five rows after their polygons and the rows from it on as the exe's, and a row at the join
 * of the two was in neither: a car standing on it was not drawn, and with the camera across the road it passed the lens in view. */
#include "formats/tri.h"
#include "game/race/track_cache.h"
#include "game/race/track_render.h"
#include "game/viewer/follow_arm.h"
#include "game/viewer/follow_behind.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { ONE = 0x10000, DRAW_SEGMENTS = 28, MAX_CALLS = 1024, CAMERA_SEGMENT = 14 };

static int32_t called[MAX_CALLS];
static int calls;

static void draw_cars(void *user, int32_t node)
{
    (void)user;
    if (calls < MAX_CALLS) called[calls++] = node;
}

static int32_t drawn_polygons, drawn_objects, drawn_models;
static void nothing_uv(void *u, const FshEntry *t, const int32_t uv[8], const int32_t s[8]) { (void)u, (void)t, (void)uv, (void)s; drawn_polygons++; }
static void nothing_full(void *u, const FshEntry *t, const int32_t s[8]) { (void)u, (void)t, (void)s; drawn_objects++; }
static void nothing_model(void *u, int m, const int32_t p[3], uint32_t h) { (void)u, (void)m, (void)p, (void)h; drawn_models++; }
static void nothing_skid(void *u, int32_t r, const int32_t s[2], const int32_t t[2]) { (void)u, (void)r, (void)s, (void)t; }

static uint8_t *read_file(const char *path, size_t *size)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) return NULL;
    uint8_t *data = NULL;
    long length = fseek(fp, 0, SEEK_END) == 0 ? ftell(fp) : -1;
    if (length > 0 && fseek(fp, 0, SEEK_SET) == 0 && (data = malloc((size_t)length)) != NULL &&
        fread(data, 1, (size_t)length, fp) != (size_t)length) {
        free(data);
        data = NULL;
    }
    fclose(fp);
    *size = data != NULL ? (size_t)length : 0;
    return data;
}

static TrackRender render;
static TrackArt art;

/* how many times `row` was asked for */
static int times(int32_t row)
{
    int n = 0;
    for (int i = 0; i < calls; i++) n += called[i] == row;
    return n;
}

/* the arm from camera node `node`, the camera facing `yaw` (0: along the road's way in this matrix, 1 turned about) */
static void pass(const Tri *tri, int32_t node, int turned)
{
    View view;
    view_init(&view, 0, 0, 320, 200, 0x100, 0x100);
    TrackCamera cam = { { 0, 0, 0 }, { ONE, 0, 0, 0, ONE, 0, 0, 0, ONE }, node };
    if (turned) cam.matrix[0] = cam.matrix[8] = -ONE;
    DrawContext ctx = { &view, { 0, 0, 320, 200 }, 0, { 0, 0, 0x1999 },
                        { NULL, nothing_uv, nothing_full, nothing_full, nothing_model, draw_cars, nothing_skid, NULL, NULL }, 0, 0 };
    track_render_set_reach(&render, DRAW_SEGMENTS);
    track_render_build(&render, tri, &art, node);
    follow_arm_window(&render, follow_arm_segments());
    track_render_build(&render, tri, &art, node);
    track_render_transform(&render, &cam, &ctx);
    const FollowArm arm = { &render, &cam, &ctx, tri_is_circuit(tri), 0, 0, 0 };
    calls = 0;
    drawn_polygons = drawn_objects = drawn_models = 0;
    follow_behind_draw(&arm, follow_arm_segments(), render.draw_segments - 2);
}

static void test_every_row_near_the_camera_once(const Tri *tri)
{
    for (int turned = 0; turned < 2; turned++)
        for (int node = 4 * CAMERA_SEGMENT; node < 4 * CAMERA_SEGMENT + 4; node++) {
            pass(tri, node, turned);
            CHECK(calls > 40);
            for (int32_t row = 4 * CAMERA_SEGMENT - 8; row <= 4 * CAMERA_SEGMENT + 12; row++) {
                if (times(row) != 1) fprintf(stderr, "node %d turned %d: row %d asked for %d times\n", node, turned, row, times(row));
                CHECK_EQ_INT(times(row), 1);
            }
        }
}

/* the scenery on the rows behind the camera, which cross the lap seam when the camera is near the start of a circuit: the run of records goes
 * round the ring (..., 510, 511, 0, 1, ...) and every one of those in the rows drawn is drawn; the records past the seam were skipped (a fifth of them lie on rows the pass does not reach, at the far end) */
static void test_the_scenery_across_the_seam(const Tri *tri)
{
    for (int node = 0; node < 40; node += 3) {
        pass(tri, node, 1);
        int32_t runs, drawn;
        follow_behind_runs(&runs, &drawn);
        if (drawn * 5 < runs * 4) fprintf(stderr, "node %d: %d of %d scenery records drawn\n", node, drawn, runs);
        CHECK(runs > 20);
        CHECK(drawn * 5 >= runs * 4);
    }
}

/* the key puts a node by its distance on from the range's first, round the ring */
static void test_run_key(void)
{
    CHECK_EQ_INT(follow_behind_run_key(508, 508, 511), 0);
    CHECK_EQ_INT(follow_behind_run_key(511, 508, 511), 3);
    CHECK_EQ_INT(follow_behind_run_key(0, 508, 511), 4);
    CHECK_EQ_INT(follow_behind_run_key(35, 508, 511), 39);
    CHECK_EQ_INT(follow_behind_run_key(500, 508, 511), 504);   /* the record before the range's first: the far end of the ring */
}

/* Rusty Springs and Burnt Sienna: the circuits, whose lap seam the arm crosses */
static const char *const TRACKS[] = { "TR1", "TR6" };

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: test_follow_behind_rows SIMDATA_DIR\n");
        return 2;
    }
    test_run_key();
    for (size_t t = 0; t < sizeof TRACKS / sizeof *TRACKS; t++) {
        char path[512];
        snprintf(path, sizeof path, "%s/MISC/%s.TRI", argv[1], TRACKS[t]);
        size_t size = 0;
        uint8_t *file = read_file(path, &size);
        Tri tri;
        if (file == NULL || tri_open(&tri, file, size) != TRI_OK) {
            printf("skip follow_behind_rows (no %s)\n", path);
            free(file);
            continue;
        }
        memset(&render, 0, sizeof render);
        test_every_row_near_the_camera_once(&tri);
        test_the_scenery_across_the_seam(&tri);
        free(file);
    }
    return TEST_REPORT();
}
