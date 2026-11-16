/* port (dep59 3b): the pace and coast an added car is driven at, from its own PDN (its sixth gear limit), or from one of four named
 * levels (ai_level= in its card). The originals keep their tables (MODEL_PACE and COAST_TICKS, ai_manager.c). */
#ifndef TNFS_CAR_AI_H
#define TNFS_CAR_AI_H

#include "formats/pdn.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int32_t pace;   /* 16.16, as MODEL_PACE (ai_manager.c) */
    int32_t coast;  /* ticks, as COAST_TICKS */
} CarAiTuning;

enum { CAR_AI_PACE_MIN = 0xe000, CAR_AI_PACE_MAX = 0x10000, CAR_AI_COAST_DEFAULT = 4 };

/* 1 for NULL or "" (no ai_level) and for slow, medium, fast, top; 0 otherwise */
int car_ai_level_ok(const char *level);

/* The tuning of the PDN `pdn` (PDN_SIZE bytes): pace = 39478 + ((top * 289) >> 16), top the sixth gear limit (PDN_FACTORS + 20),
 * clamped to [CAR_AI_PACE_MIN, CAR_AI_PACE_MAX]; coast CAR_AI_COAST_DEFAULT. A level (car_ai_level_ok) sets both to its car's pair.
 * 0, with the reason in `why` (cut to `why_cap`), when the level is none of the four. */
int car_ai_tuning(const uint8_t pdn[PDN_SIZE], const char *level, CarAiTuning *out, char *why, size_t why_cap);

#endif
