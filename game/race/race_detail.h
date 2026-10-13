/* The draw distance asked for during the race: a count of track segments
 * (00504220) taken at the next frame by 0044c118. */
#ifndef TNFS_RACE_DETAIL_H
#define TNFS_RACE_DETAIL_H

#include <stdint.h>

/* 0044c1bc (watcall segments): *pending (004c5360) = segments held to
 * 8..0x1c. 0044c118 sets 00504220 from it unless it is -1. */
void race_detail_request(int32_t segments, int32_t *pending);

#endif
