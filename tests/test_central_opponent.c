/* The head to head opponent's arrows (central_select.c): 0..7 as the original steps them, and the Warrior (11)
 * between 7 and 0 with the port's Warrior-as-a-rival extra. */
#include "game/frontend/central_run.h"
#include "test.h"

static int16_t race_type = 1, opponent, net_self, rival;

static int16_t up(int16_t from)
{
    ControlCentralState st = { 0 };
    st.race_type = &race_type, st.opponent = &opponent, st.net_self = &net_self, st.warrior_rival = &rival;
    CentralRun r = { 0 };
    r.st = &st;
    opponent = from;
    central_opponent_up(&r, 0);
    return opponent;
}

static int16_t down(int16_t from)
{
    ControlCentralState st = { 0 };
    st.race_type = &race_type, st.opponent = &opponent, st.net_self = &net_self, st.warrior_rival = &rival;
    CentralRun r = { 0 };
    r.st = &st;
    opponent = from;
    central_opponent_down(&r, 0);
    return opponent;
}

int main(void)
{
    rival = 0;
    CHECK_EQ_INT(up(6), 7);
    CHECK_EQ_INT(up(7), 0);
    CHECK_EQ_INT(down(0), 7);
    CHECK_EQ_INT(up(11), 0);      /* the original's own way back from 11 */
    CHECK_EQ_INT(down(11), 7);
    rival = 1;
    CHECK_EQ_INT(up(7), 11);
    CHECK_EQ_INT(up(11), 0);
    CHECK_EQ_INT(down(0), 11);
    CHECK_EQ_INT(down(11), 7);
    CHECK_EQ_INT(up(3), 4);
    /* a single race (type 2) keeps its 0..8 */
    race_type = 2;
    CHECK_EQ_INT(up(8), 0);
    CHECK_EQ_INT(down(0), 8);
    return TEST_REPORT();
}
