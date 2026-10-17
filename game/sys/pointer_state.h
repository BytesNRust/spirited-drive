/* The pointer's state as the original keeps it (00524790.., 004c6110..) and
 * the small functions over it; game/sys/pointer.c draws it and feeds it the
 * OS mouse. */
#ifndef TNFS_POINTER_STATE_H
#define TNFS_POINTER_STATE_H

#include <stdint.h>

typedef struct PointerState {
    int16_t active;          /* 00524790 */
    int16_t buttons;         /* 00524792 */
    int16_t mark_x, mark_y;  /* 00524794 / 00524796: where 00441858 last saw it */
    int32_t x, y;            /* 004c6110 / 004c6114 */
} PointerState;

/* 00463f80 (eax x0, edx x1, ebx y0, ecx y1, each as a short): 1 when
 * x0 <= x <= x1 and y0 <= y <= y1, else 0. */
int32_t pointer_state_in_rect(const PointerState *p, int32_t x0, int32_t x1, int32_t y0, int32_t y1);

#endif
