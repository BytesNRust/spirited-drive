/* A stage's race as the game runs it, the numbers the Studio needs too (it says when a road is too short for a stage): the game's race reads them
 * from here (game/race/race_sim.c, game/race/ai_setup.c), so the two cannot drift apart. */
#ifndef TNFS_RACE_STAGE_H
#define TNFS_RACE_STAGE_H

enum {
    RACE_STAGE_END = 0xb5,   /* nodes a stage's race stops short of its last (00455a5c -> 0043fbac): about 1.1 km of road that is not raced */
    RACE_GRID_FRONT = 0x20   /* the node of the grid's front row (ai_grid_place): a stage's finish must lie past it */
};

#endif
