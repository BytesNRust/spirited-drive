/* The marks over the other players' cars (00459064, from 004591d4): one
 * triangle pointing down at each racer, a second above it for a human. */
#ifndef TNFS_CAR_TAG_H
#define TNFS_CAR_TAG_H

#include <stdint.h>

typedef struct {
    void *ctx;
    void (*polygon)(void *ctx, uint32_t colour, int count, const int32_t *points);  /* 00484389 */
    void (*line)(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t colour);  /* 00484254 */
} CarTagOps;

typedef struct {
    const uint8_t *colours;   /* 0050b7d4: a fill colour by slot */
    uint32_t outline;         /* 0050b7b4 */
    int32_t high;             /* 004fa0c0 */
    int32_t humans;           /* 004fb7e0 */
    uint8_t hidden;           /* 004fb78b: no marks while set */
} CarTagView;

/* 00459064 (eax x, edx y, ebx slot, ecx z): nothing while hidden or for a
 * negative slot. Half width w and height h: 4 and 9 at high resolution up
 * to z 1000, 3 and 8 to 2000, 2 and 5 to 3000; else 1 and 3. The triangle
 * (x - w, y - h), (x + w - 1, y - h), (x, y) filled in colours[slot], then
 * outlined: (x - w, y - h)-(x + w, y - h), (x + w, y - h)-(x, y),
 * (x - w, y - h)-(x, y). For slot < humans the same again h higher. */
void car_tag(const CarTagOps *ops, const CarTagView *v, int32_t x, int32_t y, int32_t slot, int32_t z);

#endif
