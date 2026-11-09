/* port: the race's clock. race->tick counts sixtieths of a second: the simulation steps 30 times a second and the counter goes up by two each step (race_frame.c TICK_STEP). Anything timed
 * in seconds (the weather's, the dust's, the wipers') is written as seconds times this, never as a bare tick count. */
#ifndef TNFS_TICK_RATE_H
#define TNFS_TICK_RATE_H

enum { RACE_TICKS_A_SECOND = 60 };

#endif
