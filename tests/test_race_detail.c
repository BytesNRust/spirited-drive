/* The draw distance's bounds (race_detail.h): 8..0x1c as the exe holds them, and the port's ceiling for "ultra". */
#include "game/race/race_detail.h"
#include "game/race/track_object_walk.h"
#include "test.h"

#include <string.h>

/* 0044a804's limit: a run past 0xb4 objects draws none, unless the ultra distance raised it */
static void walk_limit(void)
{
    enum { RECORDS = 256, RUN = 200 };
    static uint8_t records[RECORDS * TRACK_WALK_RECORD_SIZE], descriptors[TRACK_WALK_RECORD_SIZE];
    memset(records, 0, sizeof records);
    for (int k = 0; k < RECORDS; k++) records[k * TRACK_WALK_RECORD_SIZE] = (uint8_t)k;
    TrackObjectWalk w = { .descriptors = descriptors, .records = records, .record_count = RECORDS, .count = RUN,
                          .vertex_count = 7 };
    track_objects_from_last(&w);
    CHECK_EQ_INT(w.count, 0);
    CHECK_EQ_INT(w.node, -1);
    w = (TrackObjectWalk){ .descriptors = descriptors, .records = records, .record_count = RECORDS, .count = RUN,
                           .vertex_count = 7, .max_objects = TRACK_WALK_MAX_OBJECTS * 2 };
    track_objects_from_last(&w);
    CHECK_EQ_INT(w.count, RUN);
    CHECK_EQ_INT(w.cursor, RUN - 1);
    CHECK_EQ_INT(w.node, RUN - 1);
}

int main(void)
{
    walk_limit();
    int32_t p = -1;
    race_detail_request(100, &p);
    CHECK_EQ_INT(p, 0x1c);
    race_detail_request(3, &p);
    CHECK_EQ_INT(p, 8);
    race_detail_request(0xe, &p);
    CHECK_EQ_INT(p, 0xe);
    race_detail_step(1, 0x1c, &p);
    CHECK_EQ_INT(p, 0x1c);
    race_detail_step(-1, 8, &p);
    CHECK_EQ_INT(p, 8);

    race_detail_set_ceiling(DRAW_SEGMENTS_ULTRA);
    race_detail_request(100, &p);
    CHECK_EQ_INT(p, DRAW_SEGMENTS_ULTRA);
    race_detail_request(0x1d, &p);
    CHECK_EQ_INT(p, 0x1d);
    race_detail_step(1, 0x1c, &p);
    CHECK_EQ_INT(p, 0x1d);
    race_detail_step(1, DRAW_SEGMENTS_ULTRA, &p);
    CHECK_EQ_INT(p, DRAW_SEGMENTS_ULTRA);
    race_detail_request(3, &p);
    CHECK_EQ_INT(p, 8);

    /* a ceiling past what the track's cache window holds, or under the exe's, is the exe's */
    race_detail_set_ceiling(DRAW_SEGMENTS_ULTRA + 1);
    race_detail_request(100, &p);
    CHECK_EQ_INT(p, 0x1c);
    race_detail_set_ceiling(DRAW_SEGMENTS_ULTRA);
    race_detail_set_ceiling(0);
    race_detail_request(100, &p);
    CHECK_EQ_INT(p, 0x1c);
    return TEST_REPORT();
}
