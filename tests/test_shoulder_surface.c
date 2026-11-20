/* Step 2b of the editor's phase 2: a tarmac shoulder the generator writes is the road's own surface to the game. A car 1 m past that edge is on surface 0, which
 * has no off-road slow-down; 1 m past a verge edge it is on the verge's surface, which has it. The TRI is the generator's own (track_generate_nodes with the
 * editor's template), read by the race (car_walls) as a race reads it. */
#include "formats/track_gen.h"
#include "formats/track_template.h"
#include "formats/tri.h"
#include "game/race/car_walls.h"
#include "game/race/race_state.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

enum {
    SEGMENTS = 50, COUNT = SEGMENTS * 4 + 1,
    WIDTH = 40,                 /* the verge byte: a 10 m road, 5 m a side */
    VERGE_UNIT = 0x2000,        /* a verge byte in world 16.16 (car_walls.c) */
    METRE = 4 * VERGE_UNIT,     /* a verge byte is a quarter of a metre */
    STEP = 300000,              /* 16.16 between two nodes, along +z */
    AT = 100                    /* the node the car is at */
};

/* the surface a car at node AT, `across` (16.16, negative left) from the centre line, is on, and whether it slows off the road */
static int surface_at(RaceState *race, int32_t across, int *off_road)
{
    Car *car = calloc(1, sizeof *car);
    if (car == NULL) return -1;
    const TriNode n = race_node(race, AT);
    car->node = AT;
    car->x = n.position.x + across, car->y = n.position.y, car->z = n.position.z;   /* heading 0: across the road is along x */
    car->surface = -1;
    car_walls(car, race);
    const int s = car->surface;
    *off_road = s >= 0 && s < RACE_SURFACES ? race->surfaces[s].off_road != 0 : -1;
    free(car);
    return s;
}

/* a straight with the left shoulder tarmac (`left`) or the right one (`right`), the other the template's verge; checks the car past each edge and in the middle */
static void check(int left, int right)
{
    uint8_t *tmpl = NULL, *out = NULL;
    size_t tsize = 0, len = 0;
    ExchangeError err = { 0 };
    CHECK(track_template_generic(&tmpl, &tsize, &err));
    static TrackGenNode nodes[COUNT];
    memset(nodes, 0, sizeof nodes);
    for (int i = 0; i < COUNT; i++) {
        nodes[i].width = WIDTH;
        nodes[i].position[2] = i * STEP;
        nodes[i].side[TRACK_GEN_LEFT].has = left ? (unsigned)TRACK_GEN_SIDE_TARMAC : 0u, nodes[i].side[TRACK_GEN_LEFT].tarmac = left;
        nodes[i].side[TRACK_GEN_RIGHT].has = right ? (unsigned)TRACK_GEN_SIDE_TARMAC : 0u, nodes[i].side[TRACK_GEN_RIGHT].tarmac = right;
    }
    CHECK(tmpl != NULL && track_generate_nodes(nodes, COUNT, tmpl, tsize, &out, &len, &err));
    Tri tri;
    RaceState *race = calloc(1, sizeof *race);
    if (out != NULL && race != NULL && tri_open(&tri, out, len) == TRI_OK) {
        race->tri = &tri;
        race_surfaces_init(race);   /* the verge's surfaces off the road, the road's (row 0) not */
        const int32_t past = WIDTH * VERGE_UNIT + METRE;
        int slows = 0;
        CHECK_EQ_INT(surface_at(race, -past, &slows), left ? 0 : 2);   /* left: the high nibble of flags[2] */
        CHECK_EQ_INT(slows, left ? 0 : 1);
        CHECK_EQ_INT(surface_at(race, past, &slows), right ? 0 : 2);   /* right: the low nibble */
        CHECK_EQ_INT(slows, right ? 0 : 1);
        CHECK_EQ_INT(surface_at(race, 0, &slows), 0);                   /* on the road: the road's */
        CHECK_EQ_INT(slows, 0);
    } else {
        CHECK(0);
    }
    free(race);
    free(out);
    free(tmpl);
}

int main(void)
{
    check(0, 0);   /* the road of before: the verge both sides */
    check(1, 0);
    check(0, 1);
    check(1, 1);
    return TEST_REPORT();
}
