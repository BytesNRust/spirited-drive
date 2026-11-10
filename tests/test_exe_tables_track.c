/* The track renderer's tables read from the exe (game/exe/exe_tables_track.c) are the tables the port holds now. */
#include "tests/exe_tables_support.h"
#include "tests/exe_tables_family.h"
#include "tests/test.h"

static void test_real(void)
{
    size_t n = 0;
    uint8_t *b = real_exe(&n);
    if (b == NULL) { fprintf(stderr, "no original/NFS_WIN.EXE: the real exe's part skipped\n"); return; }
    ExeImage img;
    char err[256] = "";
    CHECK_EQ_INT(exe_image_open(&img, b, n, err, sizeof err), 0);
    ExeTables *t = malloc(sizeof *t);
    memset(t, 0xAA, sizeof *t);
    CHECK_EQ_INT(exe_tables_track(&img, t, err, sizeof err), 0);
    CHECK(family_ok(t, "track"));
    CHECK_MEM(t->track_lod_by_distance, TRACK_LOD_BY_DISTANCE, sizeof TRACK_LOD_BY_DISTANCE);
    for (int i = 0; i < TRACK_LOD_COUNT; i++) {
        const TrackLod *g = &t->track_lods[i], *w = &TRACK_LODS[i];
        CHECK(g->vertex_count == w->vertex_count && g->polygon_count == w->polygon_count && g->first_polygon == w->first_polygon);
        CHECK_MEM(g->fence, w->fence, sizeof w->fence);
        CHECK_MEM(g->select, w->select, sizeof w->select);
    }
    for (int i = 0; i < TRACK_UV_COUNT; i++)
        CHECK(t->track_uvs[i].u_level == TRACK_UVS[i].u_level && t->track_uvs[i].u_cell == TRACK_UVS[i].u_cell && t->track_uvs[i].variant == TRACK_UVS[i].variant);
    for (int i = 0; i < TRACK_POLYGON_COUNT; i++) {
        const TrackPolygon *g = &t->track_polygons[i], *w = &TRACK_POLYGONS[i];
        CHECK_MEM(g->corner, w->corner, sizeof w->corner);
        CHECK(g->strip == w->strip && g->flags == w->flags && g->uv == w->uv && g->rows_done == w->rows_done);
    }
    CHECK_MEM(t->track_variant_near, TRACK_VARIANT_NEAR, sizeof TRACK_VARIANT_NEAR);
    CHECK_MEM(t->track_variant_far, TRACK_VARIANT_FAR, sizeof TRACK_VARIANT_FAR);
    CHECK_MEM(t->track_subdivide_z, TRACK_SUBDIVIDE_Z, sizeof TRACK_SUBDIVIDE_Z);
    free(t), free(b);
}

static void test_missing_data(void)
{
    size_t n = 0;
    uint8_t *b = tiny_exe(&n);
    ExeImage img;
    char err[256] = "";
    CHECK_EQ_INT(exe_image_open(&img, b, n, err, sizeof err), 0);
    ExeTables *t = calloc(1, sizeof *t);
    CHECK_EQ_INT(exe_tables_track(&img, t, err, sizeof err), -1);
    CHECK(strstr(err, "004bba64") != NULL);
    free(t), free(b);
}

int main(void)
{
    test_missing_data();
    test_real();
    return TEST_REPORT();
}
