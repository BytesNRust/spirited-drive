/* The tyre smoke's race_rand log (draw_rands.h): the frame after a tick draws and logs, the others replay it; a
 * replayed value is always below the n asked, and only the logging frame takes from the race. */
#include "game/viewer/draw_rands.h"
#include "test.h"

static int32_t taken;

static int32_t source(void *user, int32_t n)
{
    (void)user;
    taken++;
    return n - 1;
}

int main(void)
{
    DrawRands d = { 0 };
    draw_rands_frame(&d, 1);
    CHECK_EQ_INT(draw_rands_take(&d, 3, source, NULL), 2);
    CHECK_EQ_INT(draw_rands_take(&d, 2, source, NULL), 1);
    CHECK_EQ_INT(taken, 2);
    /* the same calls replay the log */
    draw_rands_frame(&d, 0);
    CHECK_EQ_INT(draw_rands_take(&d, 3, source, NULL), 2);
    CHECK_EQ_INT(draw_rands_take(&d, 2, source, NULL), 1);
    CHECK_EQ_INT(taken, 2);
    /* another order: puff_smoke's rand_below(2) meets puff_dirt's 2 of 3 and must stay below 2 (dev1's UBSan) */
    draw_rands_frame(&d, 0);
    CHECK_EQ_INT(draw_rands_take(&d, 2, source, NULL), 0);
    CHECK_EQ_INT(draw_rands_take(&d, 3, source, NULL), 1);
    /* past the log on a replaying frame: in range, nothing taken from the race */
    CHECK_EQ_INT(draw_rands_take(&d, 5, source, NULL), 0);
    CHECK_EQ_INT(taken, 2);
    /* a new tick starts a new log */
    draw_rands_frame(&d, 1);
    CHECK_EQ_INT(draw_rands_take(&d, 4, source, NULL), 3);
    CHECK_EQ_INT(taken, 3);
    /* a full log: the logging frame still takes, a replay past it gives 0 */
    draw_rands_frame(&d, 1);
    for (int i = 0; i < DRAW_RANDS + 3; i++) draw_rands_take(&d, 7, source, NULL);
    CHECK_EQ_INT(taken, 3 + DRAW_RANDS + 3);
    draw_rands_frame(&d, 0);
    for (int i = 0; i < DRAW_RANDS; i++) CHECK_EQ_INT(draw_rands_take(&d, 7, source, NULL), 6);
    CHECK_EQ_INT(draw_rands_take(&d, 7, source, NULL), 0);
    CHECK_EQ_INT(taken, 3 + DRAW_RANDS + 3);
    return TEST_REPORT();
}
