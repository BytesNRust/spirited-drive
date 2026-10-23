/* The head to head opponent's arrows (central_select.c): 0..7 as the original steps them, and the Warrior (11)
 * between 7 and 0 with the port's Warrior-as-a-rival extra. */
#include "game/frontend/central_run.h"
#include "test.h"

#include "game/platform/platform.h"

/* central_draw.c reaches variant_rule.c, whose rule reads the Mirror and Rally switches through
 * port_extras_load, which reads user/extras.cfg through the platform; the test has no file */
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}
void platform_free_file(uint8_t *data) { (void)data; }

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

/* The CD's files are not read by this test (the tables start as the built-in defaults). */
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path; (void)size; return NULL; }

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
