/* A car in the scene (004591d4 -> 00458d4c): culled, given its body matrix
 * and drawn with the 3D engine, near cars section by section ("inside",
 * "surface", "outside", "convex"), far cars as their low detail model.
 *
 * Car models are in world 16.16 >> 9 units (0045a3e8), not the >> 12 of
 * scenery. The rest of 00458d4c / 004591d4 lives beside it: the lights
 * (car_lights.c), the wheels (car_wheels.c), the tyre smoke (through
 * car_draw_smoking), the crash smoke (crash_trail.c) and the other racers'
 * marks (car_tag.c), wired in the viewer's row_draw. */
#ifndef TNFS_CAR_DRAW_H
#define TNFS_CAR_DRAW_H

#include "game/gfx/object3d.h"
#include "game/race/model_draw.h"

#include <stdint.h>

enum {
    CAR_SECTIONS = 4,
    CAR_MIN_Z = 0x20,
    CAR_MAX_Z = 0x5a00,
    CAR_FAR_Z = 0x1900,          /* past this, the low detail model */
    CAR_MIRROR_MAX_Z = 0x2100,   /* the rear view mirror pass */
    CAR_MIRROR_FAR_Z = 0xa00
};

typedef struct {
    Object3D *body;   /* car +0x52e */
    Object3D *far;    /* car +0x666 */
    /* 0x5be, 0x5d6, 0x5ca, 0x5e2: drawn in this order; count 0 when the
     * mesh has no such section (or its index is 8 or more) */
    Object3DList sections[CAR_SECTIONS];
} CarModel;

typedef struct {
    int32_t position[3];    /* world 16.16 (car +0) */
    /* 24-bit angles, suspension included: about x (+0xc, +0x369, 0042ecd0),
     * about z (+0x14, +0x365, 0042ecd0) and the heading (+0x351). */
    int32_t angle_x, angle_z, heading;
    const int32_t *matrix;  /* car +0x24 when +0x20 is set; NULL uses the angles */
    int always_near;        /* car +0x516 == 2 */
} CarPose;

/* Finds the sections of body (0045817b..004582b0). */
int car_model_init(CarModel *car, Object3D *body, Object3D *far);
void car_model_free(CarModel *car);

/* 004591d4 from the car's camera-space position (car +0x391, 0045a3e8). */
int car_draw_at(CarModel *car, const CarPose *pose, const int32_t cam[3], const ModelView *mv, int mirror,
                const Object3DSink *sink);

/* 00458d4c's 00458988 calls for cars[004fbb08]: in_car 1 once the near body
 * is placed, in_car 0 after its sections (none for the far model). */
typedef struct {
    void *ctx;
    void (*draw)(void *ctx, int in_car);
} CarSmokeHook;

/* car_draw with the smoke drawn about the body; smoke NULL draws none. */
int car_draw_smoking(CarModel *car, const CarPose *pose, const ModelView *mv, int mirror, const Object3DSink *sink,
                     const CarSmokeHook *smoke);

/* mirror: the second pass of 004591d4 (param_2). Returns 1 when drawn. */
int car_draw(CarModel *car, const CarPose *pose, const ModelView *mv, int mirror, const Object3DSink *sink);

/* 0042ecd0: a suspension wobble: `amplitude` x 004bad44[step] / 16 (the
 * steps 0..33 the table holds; a car's +0x427 / +0x428 count down from
 * 0x1e at most). */
int32_t car_suspension_wobble(int32_t amplitude, int32_t step);

#endif
