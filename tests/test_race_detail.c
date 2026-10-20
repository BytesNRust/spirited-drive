/* The draw distance's bounds (race_detail.h): 8..0x1c as the exe holds them, and the port's ceiling for "ultra". */
#include "game/race/race_detail.h"
#include "test.h"

int main(void)
{
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
