/* dep59 step 5, T4 follow-up: the HUD map's scale for an added track is its nodes' spacing against the CD's (tri.h: about 0x60000).
 * Applied to the CD's own tracks it gives the original scale (16 nodes a step) and a spacing within 3% of the reference: the scale is
 * proved on real roads, straight or not. Skipped with no CD. */
#include "game/race/hud_map.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

enum { REFERENCE = 0x60000, STEP = 16 };

static uint8_t *slurp(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = n > 0 ? malloc((size_t)n) : NULL;
    if (data != NULL && fread(data, 1, (size_t)n, f) != (size_t)n) free(data), data = NULL;
    fclose(f);
    *size = data != NULL ? (size_t)n : 0;
    return data;
}

static void test_the_cds_tracks(void)
{
    static const char *const NAMES[] = { "CY1", "CY2", "CY3", "AL1", "AL2", "AL3", "AL22", "TR1", "TR2", "TR3", "TR4", "TR5", "TR6", "TR7", "CL1", "CL2", "CL3" };
    int checked = 0;
    for (size_t k = 0; k < sizeof NAMES / sizeof NAMES[0]; k++) {
        char path[512];
        snprintf(path, sizeof path, "%s/original/SIMDATA/MISC/%s.TRI", TNFS_SOURCE_DIR, NAMES[k]);
        size_t size;
        uint8_t *data = slurp(path, &size);
        if (data == NULL) continue;
        Tri tri;
        CHECK(tri_open(&tri, data, size) == TRI_OK);
        const int64_t spacing = hud_map_node_spacing(&tri);
        const int64_t off = spacing > REFERENCE ? spacing - REFERENCE : REFERENCE - spacing;
        if (off * 100 >= (int64_t)REFERENCE * 3) fprintf(stderr, "%s: spacing %#llx\n", NAMES[k], (long long)spacing);
        CHECK(spacing != 0 && off * 100 < (int64_t)REFERENCE * 3);   /* within 3% of the reference */
        CHECK_EQ_INT(hud_map_step_nodes(&tri), STEP);
        checked++;
        free(data);
    }
    if (checked == 0) printf("skip: no CD tracks\n");
}

static void test_nothing_measured(void)
{
    CHECK_EQ_INT(hud_map_step_nodes(NULL), STEP);   /* the CD's step */
    CHECK(hud_map_node_spacing(NULL) == 0);
}

int main(void)
{
    test_the_cds_tracks();
    test_nothing_measured();
    return TEST_REPORT();
}
