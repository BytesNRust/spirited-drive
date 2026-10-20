/* port: the frames between the race's fixed steps (drive_interp.h). */
#include "game/viewer/drive_interp.h"

#include <string.h>

static int32_t blend(int32_t a, int32_t b, int32_t alpha)
{
    return (int32_t)(a + (((int64_t)b - a) * alpha >> 16));
}

/* the short way round a 24-bit angle: the difference sign-extended from 24 bits */
static int32_t blend_angle(int32_t a, int32_t b, int32_t alpha)
{
    const int32_t d = (int32_t)((uint32_t)(b - a) << 8) >> 8;
    return (int32_t)(a + ((int64_t)d * alpha >> 16));
}

static int64_t gap(int32_t a, int32_t b)
{
    const int64_t d = (int64_t)b - a;
    return d < 0 ? -d : d;
}

static void car_get(DriveInterpPose *p, int i, const Car *c)
{
    p->pos[i][0] = c->x, p->pos[i][1] = c->y, p->pos[i][2] = c->z;
    p->ang[i][0] = c->draw_pitch, p->ang[i][1] = c->heading, p->ang[i][2] = c->draw_roll;
    p->ang[i][3] = c->tilt_x, p->ang[i][4] = c->tilt_z;
    p->steer[i] = c->steer;
    p->use_matrix[i] = c->use_matrix;
    memcpy(p->matrix[i], c->matrix, sizeof p->matrix[i]);
}

static void car_put(Car *c, const DriveInterpPose *p, int i)
{
    c->x = p->pos[i][0], c->y = p->pos[i][1], c->z = p->pos[i][2];
    c->draw_pitch = p->ang[i][0], c->heading = p->ang[i][1], c->draw_roll = p->ang[i][2];
    c->tilt_x = p->ang[i][3], c->tilt_z = p->ang[i][4];
    c->steer = p->steer[i];
    c->use_matrix = p->use_matrix[i];
    memcpy(c->matrix, p->matrix[i], sizeof c->matrix);
}

void drive_interp_capture(DriveInterpPose *p, const RaceSim *sim)
{
    memset(p, 0, sizeof *p);
    const RaceState *race = &sim->race;
    for (int i = 0; i < RACE_CARS; i++) {
        p->present[i] = race->cars[i] != NULL;
        if (p->present[i]) car_get(p, i, race->cars[i]);
    }
    memcpy(p->eye, race->chase.position, sizeof p->eye);
    memcpy(p->eye_ang, race->chase.angles, sizeof p->eye_ang);
    p->chase_kind = race->chase.kind;
    p->tick = race->tick;
    p->valid = 1;
}

int drive_interp_snap(const DriveInterpPose *a, const DriveInterpPose *b)
{
    if (!a->valid || !b->valid || a->chase_kind != b->chase_kind) return 1;
    const int64_t ticks = (int64_t)b->tick - a->tick;
    if (ticks < 1 || ticks > 2) return 1;
    for (int k = 0; k < 3; k++)
        if (gap(a->eye[k], b->eye[k]) > DRIVE_INTERP_EYE_JUMP) return 1;
    for (int i = 0; i < RACE_CARS; i++) {
        if (a->present[i] != b->present[i]) return 1;
        for (int k = 0; k < 3 && a->present[i]; k++)
            if (gap(a->pos[i][k], b->pos[i][k]) > DRIVE_INTERP_CAR_JUMP) return 1;
    }
    return 0;
}

void drive_interp_apply(RaceSim *sim, const DriveInterpPose *a, const DriveInterpPose *b, int32_t alpha,
                        DriveInterpPose *saved)
{
    drive_interp_capture(saved, sim);
    RaceState *race = &sim->race;
    for (int i = 0; i < RACE_CARS; i++) {
        if (race->cars[i] == NULL || !a->present[i] || !b->present[i]) continue;
        DriveInterpPose m;
        for (int k = 0; k < 3; k++) m.pos[i][k] = blend(a->pos[i][k], b->pos[i][k], alpha);
        for (int k = 0; k < 5; k++) m.ang[i][k] = blend_angle(a->ang[i][k], b->ang[i][k], alpha);
        m.steer[i] = blend(a->steer[i], b->steer[i], alpha);
        m.use_matrix[i] = b->use_matrix[i];
        for (int k = 0; k < 9; k++)
            m.matrix[i][k] = a->use_matrix[i] && b->use_matrix[i] ? blend(a->matrix[i][k], b->matrix[i][k], alpha)
                                                                   : b->matrix[i][k];
        car_put(race->cars[i], &m, i);
    }
    for (int k = 0; k < 3; k++) {
        race->chase.position[k] = blend(a->eye[k], b->eye[k], alpha);
        race->chase.angles[k] = blend_angle(a->eye_ang[k], b->eye_ang[k], alpha);
    }
}

void drive_interp_restore(RaceSim *sim, const DriveInterpPose *saved)
{
    RaceState *race = &sim->race;
    for (int i = 0; i < RACE_CARS; i++)
        if (race->cars[i] != NULL && saved->present[i]) car_put(race->cars[i], saved, i);
    memcpy(race->chase.position, saved->eye, sizeof saved->eye);
    memcpy(race->chase.angles, saved->eye_ang, sizeof saved->eye_ang);
}

int32_t drive_interp_alpha(uint64_t elapsed, uint64_t step)
{
    if (step == 0 || elapsed >= step) return DRIVE_INTERP_ONE;
    return (int32_t)((elapsed << 16) / step);
}
