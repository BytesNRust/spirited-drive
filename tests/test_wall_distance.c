/* Step 3 of the editor's phase 2: a road key's wall distance is where the race stops a car. The generator writes the wall 6, 8 and 12 m past the road edge
 * of a straight (each side), and the race (car_walls, as a race reads the TRI) first pushes a car driving out to it back where its side meets that wall:
 * the road edge, plus the distance, less the car's reach to the side. Nearer the road nothing touches it; the three walls are as far apart as their keys. */
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
    WIDTH = 40,                 /* the verge byte, 1/8 m from the centre: a 10 m road, its edges 5 m out */
    EIGHTH = 0x2000,            /* a verge or barrier byte in world 16.16 (car_walls.c VERGE_UNIT): 1/8 m */
    METRE = 8 * EIGHTH,
    STEP = 300000,              /* 16.16 between two nodes, along +z */
    AT = 100,                   /* the node the car is at */
    HALF_WIDTH = 2 * METRE,     /* the car's: car_walls reaches half of it to the side when the car runs along the road */
    REACH = HALF_WIDTH / 2,
    SCAN = METRE / 16           /* the car moves out a sixteenth of a metre at a time */
};

/* whether the wall pushes a car at node AT, `across` (16.16, negative left) from the centre line, heading along the road, back towards the road */
static int pushed_back(RaceState *race, int32_t across)
{
    Car *car = calloc(1, sizeof *car);
    if (car == NULL) return -1;
    const TriNode n = race_node(race, AT);
    car->node = AT;
    car->half_width = car->half_length = HALF_WIDTH;
    car->x = n.position.x + across, car->y = n.position.y, car->z = n.position.z;   /* heading 0: across the road is along x */
    car->velocity_z = 20 * 0x10000;
    car_walls(car, race);
    const int back = across > 0 ? car->x < n.position.x + across : car->x > n.position.x + across;
    free(car);
    return back;
}

/* the first distance past the road edge (16.16) where the wall on that side (`sign` -1 left, 1 right) pushes the car back; -1 when none within 40 m */
static int32_t first_push(RaceState *race, int sign)
{
    for (int32_t past = 0; past < 40 * METRE; past += SCAN)
        if (pushed_back(race, sign * (WIDTH * EIGHTH + past))) return past;
    return -1;
}

/* a straight whose walls are `metres` past the road edge on both sides: where each stops the car, 16.16 past the edge (-1 when the TRI is not made) */
static void wall_at(double metres, int32_t out[2])
{
    out[0] = out[1] = -1;
    uint8_t *tmpl = NULL, *tri_bytes = NULL;
    size_t tsize = 0, len = 0;
    ExchangeError err = { 0 };
    CHECK(track_template_generic(&tmpl, &tsize, &err));
    static TrackGenNode nodes[COUNT];
    memset(nodes, 0, sizeof nodes);
    for (int i = 0; i < COUNT; i++) {
        nodes[i].width = WIDTH;
        nodes[i].position[2] = i * STEP;
        for (int side = 0; side < 2; ++side) nodes[i].side[side].has = TRACK_GEN_SIDE_BARRIER, nodes[i].side[side].barrier = 8 * metres;
    }
    CHECK(tmpl != NULL && track_generate_nodes(nodes, COUNT, tmpl, tsize, &tri_bytes, &len, &err));
    Tri tri;
    RaceState *race = calloc(1, sizeof *race);
    if (tri_bytes != NULL && race != NULL && tri_open(&tri, tri_bytes, len) == TRI_OK) {
        race->tri = &tri;
        race_surfaces_init(race);
        const TriNode n = race_node(race, AT);
        CHECK_EQ_INT(n.left_barrier - n.left_verge, (int)(8 * metres));    /* the TRI has the key's wall */
        CHECK_EQ_INT(n.right_barrier - n.right_verge, (int)(8 * metres));
        out[0] = first_push(race, -1);
        out[1] = first_push(race, 1);
    } else {
        CHECK(0);
    }
    free(race);
    free(tri_bytes);
    free(tmpl);
}

int main(void)
{
    static const double WALLS[] = { 6, 8, 12 };
    int32_t at[3][2];
    for (int w = 0; w < 3; ++w) {
        wall_at(WALLS[w], at[w]);
        const int32_t want = (int32_t)(WALLS[w] * METRE) - REACH;   /* the car's side at the wall */
        for (int side = 0; side < 2; ++side) {
            CHECK(at[w][side] >= 0);
            CHECK(at[w][side] > want - SCAN && at[w][side] <= want + SCAN);
            if (!(at[w][side] > want - SCAN && at[w][side] <= want + SCAN))
                fprintf(stderr, "wall %g m, %s: the car is stopped %.3f m past the edge, not %.3f\n", WALLS[w], side ? "right" : "left", at[w][side] / 65536.0,
                        want / 65536.0);
        }
    }
    for (int side = 0; side < 2; ++side) {   /* as far apart as the keys: 2 m from 6 to 8, 4 m from 8 to 12 */
        CHECK(abs((at[1][side] - at[0][side]) - 2 * METRE) <= SCAN);
        CHECK(abs((at[2][side] - at[1][side]) - 4 * METRE) <= SCAN);
    }
    return TEST_REPORT();
}
