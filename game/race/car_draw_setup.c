/* The cars' drawing set up for the race (car_draw_setup.h), as 00457594 runs it. */
#include "game/race/car_draw_setup.h"

static const int32_t STEPS[CAR_DRAW_STEPS] = { 0x7a80, 0x2d00, 0x1900, 0xf00, 0x780 };
static const int32_t FAR_STEPS[CAR_DRAW_FAR_STEPS] = { 24000, 0x960, 800 };

void car_draw_setup(CarDrawSetup *s, SkidMarks *marks, uint8_t *(*alloc)(void *ctx, int32_t size), void *ctx)
{
    for (int i = 0; i < CAR_DRAW_STEPS; i++) s->steps[i] = STEPS[i];
    for (int i = 0; i < CAR_DRAW_FAR_STEPS; i++) s->far_steps[i] = FAR_STEPS[i];
    s->buffer = alloc(ctx, CAR_DRAW_BUFFER);
    for (int i = 0; i < CAR_DRAW_MODELS; i++) s->model_first[i] = 0;
    skid_reset(marks);
}
