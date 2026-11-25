/* A track light file is written in the track's own (forward) direction; a circuit driven the other way round runs its nodes mirrored (node j is the file's (nodes - j) mod nodes, the two chains of a
 * row swapped: tri_reverse.h). track_light_file_mirror turns a read file into the direction the track runs: tunnel marks, the retouches' node ranges and sides, the gradients' steps, the lamps, the rows. */
#include "game/race/track_light_mirror.h"
#include "game/race/track_light_track.h"
#include "test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { NODES = 1000 };

uint8_t *platform_read_file(const char *path, size_t *size)
{
    (void)path;
    *size = 0;
    return NULL;
}
void platform_free_file(uint8_t *data) { free(data); }

static const char TEXT[] =
    "# tnfs track light 2\n"
    "track TR3\n"
    "tunnel 300 340 gradient 3\n"
    "time A\n"
    "  tunnel_look 80 ff8040\n"
    "time N\n"
    "  tunnel_look 80 ff8040\n"
    "  lamp 310 150 ff2000:80 8 left\n"
    "time B\n";

static int parse(char letter, TrackLightFile *out)
{
    memset(out, 0, sizeof *out);
    return track_light_track_parse(TEXT, strlen(TEXT), "TR3", letter, NODES, out, NULL);
}

static void test_the_nodes_and_lamps_mirror(void)
{
    TrackLightFile f;
    CHECK(parse('N', &f));
    CHECK(f.light.tunnel != NULL && f.light.tunnel[300] == 1 && f.light.tunnel[340] == 1 && f.light.tunnel[341] == 0);
    CHECK(track_light_file_mirror(&f, NODES));
    CHECK(f.light.tunnel[660] == 1 && f.light.tunnel[700] == 1 && f.light.tunnel[659] == 0 && f.light.tunnel[701] == 0 && f.light.tunnel[300] == 0 && f.light.tunnel[340] == 0);   /* 300..340 is the run's 660..700 */
    CHECK_EQ_INT(f.tweaks.gradient_count, 1);
    CHECK(f.tweaks.gradients[0].from == 660 && f.tweaks.gradients[0].to == 700 && f.tweaks.gradients[0].nodes == 3);
    CHECK(f.tweaks.step_count > 0);
    for (int i = 0; i < f.tweaks.step_count; i++) {   /* a gradient's step stands at the mirrored tunnel's ends, as it stood at the file's */
        const TrackLightZone *const s = &f.tweaks.steps[i];
        CHECK(s->from == s->to && ((s->from >= 655 && s->from <= 665) || (s->from >= 695 && s->from <= 705)));
    }
    CHECK_EQ_INT(f.tweaks.lamp_count, 1);
    CHECK_EQ_INT(f.tweaks.lamps[0].node, 690);        /* 1000 - 310 */
    CHECK_EQ_INT(f.tweaks.lamps[0].mask, 0x03e);      /* the file's left (points 6..10) is the run's right chain (1..5) */
    track_light_file_free(&f);
}

static void test_the_retouches_and_rows_mirror(void)
{
    static const char T[] = "track TR3\ntime A\n  brightness 100 120 60 p2 p4\n  brightness 130 140 50 left\n  tone 200 210 ffe080:50\n  rows 5 6 1 2 3 4 5 6 7 8 9 10 11\n";
    TrackLightFile f;
    memset(&f, 0, sizeof f);
    int bad = -1;
    CHECK(track_light_track_parse(T, strlen(T), "TR3", 'A', NODES, &f, &bad));
    CHECK_EQ_INT(bad, 0);
    CHECK(track_light_file_mirror(&f, NODES));
    CHECK_EQ_INT(f.tweaks.brightness_count, 2);
    CHECK(f.tweaks.brightness[0].from == 880 && f.tweaks.brightness[0].to == 900);
    CHECK_EQ_INT(f.tweaks.brightness[0].mask, 0x380);   /* p2 p4 (the run p2..p4) are the run's p7..p9 */
    CHECK(f.tweaks.brightness[1].from == 860 && f.tweaks.brightness[1].to == 870);
    CHECK_EQ_INT(f.tweaks.brightness[1].mask, 0x03e);   /* the left's points 6..10 are the run's 1..5 */
    CHECK_EQ_INT(f.tweaks.tone_run_count, 1);
    CHECK(f.tweaks.tone_runs[0].from == 790 && f.tweaks.tone_runs[0].to == 800);
    CHECK(f.row_set != NULL && f.row_set[5] == 0 && f.row_set[995] == 1);
    const uint8_t *const row = &f.light.data[(size_t)995 * TRACK_NODE_LIGHT_POINTS];
    CHECK_EQ_INT(row[0], 1);    /* the middle point stays */
    CHECK_EQ_INT(row[1], 7);    /* the chains swap: point 1 is the file's point 6 */
    CHECK_EQ_INT(row[6], 2);
    CHECK_EQ_INT(row[10], 6);
    track_light_file_free(&f);
}

/* mirroring twice is the file again; the sides and a node past the middle too */
static void test_twice_is_the_file(void)
{
    TrackLightFile a, b;
    CHECK(parse('N', &a) && parse('N', &b));
    CHECK(track_light_file_mirror(&b, NODES) && track_light_file_mirror(&b, NODES));
    CHECK(memcmp(a.light.tunnel, b.light.tunnel, NODES) == 0);
    CHECK_EQ_INT(a.tweaks.lamps[0].node, b.tweaks.lamps[0].node);
    CHECK_EQ_INT(a.tweaks.lamps[0].mask, b.tweaks.lamps[0].mask);
    CHECK_EQ_INT(a.tweaks.step_count, b.tweaks.step_count);
    for (int i = 0; i < a.tweaks.step_count; i++) CHECK(a.tweaks.steps[i].from == b.tweaks.steps[i].from && a.tweaks.steps[i].to == b.tweaks.steps[i].to);
    track_light_file_free(&a);
    track_light_file_free(&b);
    CHECK(!track_light_file_mirror(NULL, NODES));
}

int main(void)
{
    test_the_nodes_and_lamps_mirror();
    test_the_retouches_and_rows_mirror();
    test_twice_is_the_file();
    return TEST_REPORT();
}
