#include "game/race/finish_gate_light.h"

#include <ctype.h>
#include <stddef.h>

/* the gate is scenery texture 31 in the nine stages (CHECKPOINT on the first two of each track, FINISH on the third of CY, CL and AL; texture 12 of AL3 is a
 * park sign, not it) */
enum { GATE_BOARD = 31 };

int finish_gate_board(const char *track)
{
    if (track == NULL || track[0] == '\0' || track[1] == '\0' || track[2] < '1' || track[2] > '3') return -1;
    const int a = toupper((unsigned char)track[0]), b = toupper((unsigned char)track[1]);
    const int stage = (a == 'C' && (b == 'Y' || b == 'L')) || (a == 'A' && b == 'L');
    return stage && track[3] == '\0' ? GATE_BOARD : -1;
}

uint8_t finish_gate_light_level(int gate_board1, int32_t resource, uint8_t node_light)
{
    if (gate_board1 <= 0 || resource < 0 || resource / 4 != gate_board1 - 1) return node_light;
    return node_light > FINISH_GATE_LIGHT ? node_light : FINISH_GATE_LIGHT;
}
