/* The stage gate's light at night (finish_gate_light.h): which track has the board, what the light does to it and to nothing else. */
#include "game/race/finish_gate_light.h"
#include "game/race/psx_light.h"
#include "tests/test.h"

static void test_the_stage_tracks_have_the_gate_and_no_other(void)
{
    const char *const stages[] = { "CY1", "CY2", "CY3", "CL1", "CL2", "CL3", "AL1", "AL2", "AL3", "cy1", "Al3" };
    for (size_t i = 0; i < sizeof stages / sizeof stages[0]; i++) CHECK_EQ_INT(finish_gate_board(stages[i]), 31);
    const char *const others[] = { "TR1", "TR4", "TR7", "CY", "CY4", "CY0", "CY10", "XL1", "", "A" };
    for (size_t i = 0; i < sizeof others / sizeof others[0]; i++) CHECK_EQ_INT(finish_gate_board(others[i]), -1);
    CHECK_EQ_INT(finish_gate_board(NULL), -1);
}

static void test_the_gate_keeps_full_light_and_the_rest_the_nodes(void)
{
    const int gate1 = 31 + 1;
    CHECK_EQ_INT(finish_gate_light_level(gate1, 31 * 4, 20), FINISH_GATE_LIGHT);        /* the gate: lit in the dark */
    CHECK_EQ_INT(finish_gate_light_level(gate1, 31 * 4 + 3, 20), FINISH_GATE_LIGHT);    /* the four resources of a board are one */
    CHECK_EQ_INT(finish_gate_light_level(gate1, 30 * 4, 20), 20);                       /* another board keeps the node's */
    CHECK_EQ_INT(finish_gate_light_level(gate1, 32 * 4, 20), 20);
    CHECK_EQ_INT(finish_gate_light_level(gate1, -1, 20), 20);                           /* a model has no board */
    CHECK_EQ_INT(finish_gate_light_level(0, 31 * 4, 20), 20);                           /* the flag off (no gate): as the exe has it */
    CHECK_EQ_INT(finish_gate_light_level(gate1, 31 * 4, 255), 255);                     /* never darker than the node */
}

static void test_a_light_has_no_gate_until_asked(void)
{
    PsxLight l = { 0 };
    CHECK_EQ_INT(l.gate_board, 0);
}

int main(void)
{
    test_the_stage_tracks_have_the_gate_and_no_other();
    test_the_gate_keeps_full_light_and_the_rest_the_nodes();
    test_a_light_has_no_gate_until_asked();
    return TEST_REPORT();
}
