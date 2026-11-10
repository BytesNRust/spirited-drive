/* The tunnel lamps' places (tunnel_lamps.h): the lamp textures' polygons of a made-up segment give one lamp an even node, at the middle of the strips. */
#include "game/race/tunnel_lamps.h"
#include "tests/test.h"

#include <math.h>
#include <string.h>

/* a straight road along z, 10 m a node, the 11 points of a row across x: point 0 the centre, 1-5 to the right 1 m each, 6-10 to the left */
static void straight(TrackSegment *s, int first_node)
{
    memset(s, 0, sizeof *s);
    for (int r = 0; r < TRACK_SEGMENT_ROWS; r++)
        for (int p = 0; p < TRACK_ROW_POINTS; p++) {
            const int side = p == 0 ? 0 : p <= 5 ? 1 : -1, step = p == 0 ? 0 : (p - 1) % 5 + 1;
            s->rows[r][p].x = side * step * 65536;
            s->rows[r][p].z = (first_node + r) * 10 * 65536;
        }
}

static void test_a_lamp_strip_gives_a_lamp_each_even_node(void)
{
    TrackSegment seg;
    straight(&seg, 8);
    memset(seg.texture, 3, sizeof seg.texture);
    seg.texture[2] = 38;   /* one strip of the lamp's texture (CY1's byte) */
    TrackPoint next[TRACK_ROW_POINTS];
    memcpy(next, &seg.rows[3][0], sizeof next);
    for (int p = 0; p < TRACK_ROW_POINTS; p++) next[p].z += 10 * 65536;
    const uint8_t slots[1] = { 38 };
    TunnelLamp out[8];
    const size_t n = tunnel_lamps_in_segment(&seg, next, 2, slots, 1, out, 8);
    CHECK_EQ_INT((int)n, 2);   /* nodes 8 and 10 of the segment's 8..11 */
    CHECK_EQ_INT((int)out[0].node, 8);
    CHECK_EQ_INT((int)out[1].node, 10);
    CHECK(fabsf(out[0].x - 2.5f) < 0.01f);   /* strip 2 joins points 2 and 3: x 2..3 on both rows, the middle 2.5 */
    CHECK(fabsf(out[0].z - 85.0f) < 0.01f);   /* the band from z 80 to 90 */
    CHECK(fabsf(out[1].z - 105.0f) < 0.01f);
    CHECK(fabsf(out[0].spacing - 10.0f) < 0.01f);
}

static void test_two_strips_meet_in_the_middle_and_no_lamp_is_none(void)
{
    TrackSegment seg;
    straight(&seg, 0);
    memset(seg.texture, 3, sizeof seg.texture);
    seg.texture[2] = 20, seg.texture[7] = 20;   /* the strip at x 2..3 and the one at x -2..-3 */
    TrackPoint next[TRACK_ROW_POINTS];
    memcpy(next, &seg.rows[3][0], sizeof next);
    const uint8_t slots[1] = { 20 };
    TunnelLamp out[4];
    CHECK_EQ_INT((int)tunnel_lamps_in_segment(&seg, next, 0, slots, 1, out, 4), 2);
    CHECK(fabsf(out[0].x) < 0.01f);   /* both sides: the road's middle */
    const uint8_t other[1] = { 21 };
    CHECK_EQ_INT((int)tunnel_lamps_in_segment(&seg, next, 0, other, 1, out, 4), 0);
    CHECK_EQ_INT((int)tunnel_lamps_in_segment(&seg, next, 0, slots, 1, out, 1), 1);   /* the room given is the most */
}

static void test_a_track_with_no_lamp_textures_has_none(void)
{
    TunnelLamp out[4];
    CHECK_EQ_INT((int)tunnel_lamps_find(NULL, "CY1", out, 4), 0);
}

int main(void)
{
    test_a_lamp_strip_gives_a_lamp_each_even_node();
    test_two_strips_meet_in_the_middle_and_no_lamp_is_none();
    test_a_track_with_no_lamp_textures_has_none();
    return TEST_REPORT();
}
