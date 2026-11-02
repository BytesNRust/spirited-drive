/* port: the segment window's first call, from the "nothing loaded" head (-9999, 0043e4e0), loads the segments ahead of the camera wherever on the
 * ring the camera is (the exe's own loop only does at a start near segment 0, where a race begins) */
#include "game/race/track_cache.h"
#include "test.h"

#include <stdint.h>
#include <string.h>

enum { RING = 256, AHEAD = 30, BEHIND = 35 };

typedef struct {
    uint8_t loaded[RING];
} Seen;

static void note(void *ctx, int32_t segment)
{
    ((Seen *)ctx)->loaded[(uint32_t)segment & (RING - 1)] = 1;
}

static void first_call(Seen *seen, int32_t segment)
{
    TrackCacheWindow w = { TRACK_WINDOW_UNLOADED, 0, BEHIND, AHEAD, RING, RING - 1 };
    const TrackWindowOps ops = { seen, note };
    memset(seen, 0, sizeof *seen);
    track_cache_window(&ops, &w, segment);
}

/* the camera's segment and the 29 ahead of it are loaded, whichever segment the first call is for */
static void test_first_call_loads_the_forward_window(void)
{
    for (int32_t segment = 0; segment < RING; segment += 7) {
        Seen seen;
        first_call(&seen, segment);
        int missing = 0;
        for (int i = 0; i < AHEAD; i++) missing += !seen.loaded[(segment + i) & (RING - 1)];
        if (missing != 0) {
            CHECK_EQ_INT(segment, -1);   /* the segment that failed */
            CHECK_EQ_INT(missing, 0);
        }
    }
    Seen seen;
    first_call(&seen, 210);   /* TR3's high stretch: the viewer's node 840 */
    for (int i = 0; i < AHEAD; i++) CHECK(seen.loaded[(210 + i) & (RING - 1)]);
}

/* a race's start, near segment 0, still loads what it did: the 14 behind the line and the 30 ahead */
static void test_a_race_start_is_as_it_was(void)
{
    Seen seen;
    first_call(&seen, 0);
    for (int s = 242; s < RING; s++) CHECK(seen.loaded[s]);
    for (int s = 0; s < AHEAD; s++) CHECK(seen.loaded[s]);
    for (int s = 60; s < 200; s++) CHECK(!seen.loaded[s]);   /* nothing far from the camera */
}

int main(void)
{
    test_first_call_loads_the_forward_window();
    test_a_race_start_is_as_it_was();
    return TEST_REPORT();
}
