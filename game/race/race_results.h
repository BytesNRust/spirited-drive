/* The race's results: the finishing order, the records and the results
 * screen's words (0045af80, 0045b29c). */
#ifndef TNFS_RACE_RESULTS_H
#define TNFS_RACE_RESULTS_H

#include <stdint.h>

/* 0042700c: the short-name index (004c5c84) of car model `model`; the high
 * half of the result is the model's sign, as the original leaves it. */
int32_t race_short_name_index(int16_t model);

#endif
