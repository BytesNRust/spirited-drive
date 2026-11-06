/* The track light file (track_light_file.h): a made light written and read back is the same light, the header keeps a file to its track, time, nodes and flags, a bad line
 * is refused, and the mod's folder is looked in first. Made-up rows, no game data. */
#include "game/race/track_light_file.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

enum { NODES = 40 };

static PsxLight make(void)
{
    PsxLight l = { 0 };
    l.nodes = NODES;
    l.data = malloc((size_t)NODES * PSX_LIGHT_POINTS);
    for (int n = 0; n < NODES; n++)
        for (int p = 0; p < PSX_LIGHT_POINTS; p++) l.data[n * PSX_LIGHT_POINTS + p] = (uint8_t)(n < 10 ? 255 : n % 2 ? 60 + p : 90 - p);   /* a run, then an alternation */
    l.tunnel = calloc(NODES, 1);
    memset(l.tunnel + 12, 1, 8);
    l.hall = calloc(NODES, 1);                       /* there, and empty */
    l.canyon = calloc(NODES, 1);
    l.canyon[3] = l.canyon[39] = 1;
    l.pool_class = calloc(NODES, 1);
    memset(l.pool_class + 20, 3, 4);
    l.pool_class[30] = 4;
    l.car_disc = 1;
    for (int i = 0; i < 3; i++) l.car_even[i] = 0.625f + (float)i / 7.0f, l.car_odd[i] = 0.1f * (float)(i + 1);
    l.neutral_cast = 1, l.city_tunnel = 2;
    return l;
}

static void test_round_trip(void)
{
    PsxLight a = make(), b = { 0 };
    char *text = NULL;
    size_t size = 0;
    CHECK(track_light_format(&a, "CY1", 'N', 0x3fff, &text, &size));
    int bad = -1;
    CHECK(track_light_parse(text, size, "CY1", 'N', NODES, 0x3fff, &b, &bad));
    CHECK(memcmp(a.data, b.data, (size_t)NODES * PSX_LIGHT_POINTS) == 0);
    CHECK(b.tunnel != NULL && memcmp(a.tunnel, b.tunnel, NODES) == 0);
    CHECK(b.hall != NULL && memcmp(a.hall, b.hall, NODES) == 0);   /* present and empty stays present */
    CHECK(b.car_dim == NULL && b.car_pool == NULL);                /* absent stays absent */
    CHECK(b.canyon != NULL && memcmp(a.canyon, b.canyon, NODES) == 0);
    CHECK(b.pool_class != NULL && memcmp(a.pool_class, b.pool_class, NODES) == 0);
    CHECK(b.car_disc == 1 && memcmp(a.car_even, b.car_even, sizeof a.car_even) == 0 && memcmp(a.car_odd, b.car_odd, sizeof a.car_odd) == 0);   /* the floats exactly */
    CHECK(b.neutral_cast == 1 && b.city_tunnel == 2);
    CHECK(b.nodes == NODES);
    /* the file is by runs: the first ten identical rows are one line */
    CHECK(strstr(text, "filas 0 9 ") != NULL);
    psx_light_free(&a), psx_light_free(&b);
    free(text);
}

static void test_header_and_bad_lines(void)
{
    PsxLight a = make(), b = { 0 };
    char *text = NULL;
    size_t size = 0;
    CHECK(track_light_format(&a, "CY1", 'N', 0x3fff, &text, &size));
    CHECK(!track_light_parse(text, size, "CY2", 'N', NODES, 0x3fff, &b, NULL));   /* another track */
    CHECK(!track_light_parse(text, size, "CY1", 'B', NODES, 0x3fff, &b, NULL));   /* another time */
    CHECK(!track_light_parse(text, size, "CY1", 'N', NODES + 1, 0x3fff, &b, NULL));   /* another node count */
    CHECK(!track_light_parse(text, size, "CY1", 'N', NODES, 0x3ffe, &b, NULL));   /* made with other flags */
    CHECK(b.data == NULL);
    char *bad_text = malloc(size + 32);
    memcpy(bad_text, text, size);
    memcpy(bad_text + size, "tunel 5 99\n", 12);   /* past the track */
    int bad = 0;
    CHECK(!track_light_parse(bad_text, size + 11, "CY1", 'N', NODES, 0x3fff, &b, &bad));
    CHECK(bad > 2);
    char *missing = strdup("pista CY1 hora N nodos 40 banderas 3fff\nfilas 0 9 1 1 1 1 1 1 1 1 1 1 1\n");
    CHECK(!track_light_parse(missing, strlen(missing), "CY1", 'N', NODES, 0x3fff, &b, NULL));   /* nodes without a row */
    free(missing), free(bad_text), free(text);
    psx_light_free(&a);
}

int main(void)
{
    test_round_trip();
    test_header_and_bad_lines();
    return TEST_REPORT();
}
