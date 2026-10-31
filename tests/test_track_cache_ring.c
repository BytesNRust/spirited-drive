/* port (port_follow_ring): the segment cache's ring is 64 records of 0x21c as the exe's, or more (track_cache.h): where a segment's record is */
#include "game/race/mirror_scene.h"
#include "game/race/track_cache.h"
#include "test.h"

#include <stdint.h>

enum { RECORD = 0x21c, EXE_SIZE = 0x8700 };

/* the exe's ring: a segment's record at ((segment & mask) x 0x21c) mod 0x8700, a segment 64 on shares it */
static void test_the_exes_ring(void)
{
    CHECK_EQ_INT(track_cache_records(), 64);
    CHECK_EQ_INT(track_cache_size(), EXE_SIZE);
    for (int32_t s = 0; s < 300; s++) {
        CHECK_EQ_INT((int32_t)mirror_segment_offset(s, 0x1ff), (int32_t)(((uint32_t)(s & 0x1ff) * RECORD) % EXE_SIZE));
        CHECK_EQ_INT((int32_t)mirror_segment_offset(s, 0x1ff), (int32_t)mirror_segment_offset(s + 64, 0x1ff));
    }
    CHECK_EQ_INT((int32_t)mirror_segment_offset(-3, -1), (int32_t)((((uint32_t)-3) * RECORD) % EXE_SIZE));   /* a stage's mask is -1 */
}

/* a bigger ring keeps the segments 64 apart in their own records, and wraps at its size */
static void test_a_bigger_ring(void)
{
    track_cache_set_records(128);
    CHECK_EQ_INT(track_cache_records(), 128);
    CHECK_EQ_INT(track_cache_size(), 128 * RECORD);
    for (int32_t s = 0; s < 300; s++) {
        CHECK_EQ_INT((int32_t)mirror_segment_offset(s, 0x1ff), (int32_t)((uint32_t)(s % 128) * RECORD));
        CHECK(mirror_segment_offset(s, 0x1ff) != mirror_segment_offset(s + 64, 0x1ff));
        CHECK_EQ_INT((int32_t)mirror_segment_offset(s, 0x1ff), (int32_t)mirror_segment_offset(s + 128, 0x1ff));
    }
    track_cache_set_records(64);
    CHECK_EQ_INT(track_cache_size(), EXE_SIZE);
}

/* a size the arrays do not hold, or smaller than the exe's, is the exe's */
static void test_other_sizes_are_the_exes(void)
{
    track_cache_set_records(200);
    CHECK_EQ_INT(track_cache_records(), 64);
    track_cache_set_records(10);
    CHECK_EQ_INT(track_cache_records(), 64);
    track_cache_set_records(TRACK_CACHE_RECORDS_MAX);
    CHECK_EQ_INT(track_cache_records(), TRACK_CACHE_RECORDS_MAX);
    track_cache_set_records(64);
}

int main(void)
{
    test_the_exes_ring();
    test_a_bigger_ring();
    test_other_sizes_are_the_exes();
    return TEST_REPORT();
}
