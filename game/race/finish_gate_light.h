/* port (finish_gate_light): the stage tracks' (CY, CL and AL, three stages each) gate that ends a stage, a scenery board with a CHECKPOINT banner (a FINISH
 * one with chequered flags on the last stage) hung over the road. By night the board takes the node's dark like the rest of the scenery; with the flag on it
 * keeps its daylight light, so the banner stands out against the dark. Only that board is touched; no PS1 data is used. */
#ifndef TNFS_FINISH_GATE_LIGHT_H
#define TNFS_FINISH_GATE_LIGHT_H

#include <stdint.h>

enum { FINISH_GATE_LIGHT = 255 };   /* the light (of 255) the board keeps */

/* The scenery board of the track's gate (any case: CY1..CY3, CL1..CL3, AL1..AL3); -1 for any other track and for NULL. */
int finish_gate_board(const char *track);

/* The light a 3D object takes at a node: `node_light` unless `resource` (a descriptor's resource, four to a board) is the gate's board `gate_board1` (the
 * board's id plus one, 0 for none), which keeps FINISH_GATE_LIGHT when that is more. */
uint8_t finish_gate_light_level(int gate_board1, int32_t resource, uint8_t node_light);

#endif
