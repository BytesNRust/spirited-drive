/* Road profile's values (owner feedback T9): every row of the strip and of the Road profile tab says what it is for, in one line, on hover.
 * road_value_help(bit) is that line; each of the key's bits has one, and none is empty or the same as another's. */
#include "value_help.h"
#include "formats/track_trace.h"
#include "test.h"

#include <string.h>

int main(void)
{
    unsigned bits[32];
    int n = 0;
    bits[n++] = TRACE_KEY_WIDTH;
    bits[n++] = TRACE_KEY_BANK;
    bits[n++] = TRACE_KEY_LANES;
    bits[n++] = TRACE_KEY_SHOULDER_SURFACE;
    for (int field = 0; field < TRACE_SIDE_FIELDS; ++field)
        for (int side = 0; side < 2; ++side) bits[n++] = TRACE_KEY_SIDE(field, side);
    bits[n++] = TRACE_KEY_FENCE_HEIGHT;
    bits[n++] = TRACE_KEY_FENCE_ART;
    bits[n++] = TRACE_KEY_BARRIER_CUSTOM(0);
    bits[n++] = TRACE_KEY_BARRIER_CUSTOM(1);
    for (int i = 0; i < n; ++i) {
        const char *help = road_value_help(bits[i]);
        CHECK(help != NULL && strlen(help) > 20);
    }
    /* the two sides of one value read the same; different values read differently */
    CHECK(strcmp(road_value_help(TRACE_KEY_SIDE(TRACE_SIDE_FENCE, 0)), road_value_help(TRACE_KEY_SIDE(TRACE_SIDE_FENCE, 1))) == 0);
    CHECK(strcmp(road_value_help(TRACE_KEY_WIDTH), road_value_help(TRACE_KEY_BANK)) != 0);
    CHECK(strcmp(road_value_help(TRACE_KEY_SIDE(TRACE_SIDE_BARRIER, 0)), road_value_help(TRACE_KEY_SIDE(TRACE_SIDE_WALL, 0))) != 0);
    /* a bit that is not a road value has no help */
    CHECK(road_value_help(0) == NULL && road_value_help(1u << 30) == NULL);
    return TEST_REPORT();
}
