/* The cars' drawing set up for the race (00457594, from 00455a5c): the
 * distance steps, the buffer 0045a3e8 projects into, the models' first
 * word, the skid marks cleared. */
#ifndef TNFS_CAR_DRAW_SETUP_H
#define TNFS_CAR_DRAW_SETUP_H

#include "game/race/skid_marks.h"

#include <stdint.h>

enum {
    CAR_DRAW_STEPS = 5,         /* 00515018..00515028 */
    CAR_DRAW_FAR_STEPS = 3,     /* 005175a8..005175b0 */
    CAR_DRAW_MODELS = 30,       /* 005150f0's records (0x138 bytes) whose first word is cleared */
    CAR_DRAW_BUFFER = 0x7d0     /* 0051c770's allocation */
};

typedef struct {
    int32_t steps[CAR_DRAW_STEPS];          /* 00515018 */
    int32_t far_steps[CAR_DRAW_FAR_STEPS];  /* 005175a8 */
    uint8_t *buffer;                        /* 0051c770 (00474c8c("..." 004b1168, 0x7d0, 0)) */
    int32_t model_first[CAR_DRAW_MODELS];   /* 005150f0 + 0x138 i */
    uint32_t model_object[CAR_DRAW_MODELS]; /* 005150f0 + 0x138 i + 0x10: the model's 3D object (0045aa04 releases it) */
} CarDrawSetup;

/* 00457594 (watcall ()): steps 0x7a80, 0x2d00, 0x1900, 0xf00, 0x780; far
 * steps 24000, 0x960, 800; the buffer allocated (0x7d0); the 30 models'
 * first words 0; skid_reset (00457560). */
void car_draw_setup(CarDrawSetup *s, SkidMarks *marks, uint8_t *(*alloc)(void *ctx, int32_t size), void *ctx);

/* 0045aa04 (watcall ()): each of the 30 models with its first word set:
 * release (0048da33) its object (+0x10). */
void car_draw_models_free(const CarDrawSetup *s, void (*release)(void *ctx, uint32_t object), void *ctx);

#endif
