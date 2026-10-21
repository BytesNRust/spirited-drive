/* port: the frames between the race's fixed steps (30 a second). The race is never changed: a frame drawn between
 * two steps puts the cars' and the eye's poses blended between them into the race, draws, and puts back what was
 * there. The HUD's values stay the last step's. */
#ifndef TNFS_DRIVE_INTERP_H
#define TNFS_DRIVE_INTERP_H

#include "game/race/race_sim.h"

#include <stdint.h>

enum {
    DRIVE_INTERP_ONE = 0x10000,         /* alpha at the newer step */
    DRIVE_INTERP_CAR_JUMP = 0x200000,   /* a car moved further in one step: a reset, not driving */
    DRIVE_INTERP_EYE_JUMP = 0x400000    /* the eye moved further in one step: it jumped to the car */
};

typedef struct {
    int32_t pos[RACE_CARS][3];      /* x, y, z */
    int32_t ang[RACE_CARS][5];      /* draw_pitch, heading, draw_roll, tilt_x, tilt_z: what the car is drawn by */
    int32_t steer[RACE_CARS];
    int32_t matrix[RACE_CARS][9];   /* blended only when both steps use it */
    int32_t use_matrix[RACE_CARS];
    int present[RACE_CARS];
    int32_t eye[3], eye_ang[3];     /* the chase record's position and angles */
    int32_t eye_node;               /* its node: between two steps the older one's, so the track drawn from it starts
                                     * at or behind the blended eye (the newer's segment may begin past it) */
    int32_t chase_kind, tick;
    int valid;
} DriveInterpPose;

void drive_interp_capture(DriveInterpPose *p, const RaceSim *sim);
/* 1 when a and b are not one step of driving apart: a car or the eye moved more than the jumps above, another
 * camera kind, the race ticks not 1 or 2 apart (a replay's seek, the start), either pose not valid */
int drive_interp_snap(const DriveInterpPose *a, const DriveInterpPose *b);
/* The pose a + (b - a) * alpha into sim (alpha 0..DRIVE_INTERP_ONE; 24-bit angles the short way), what was there
 * kept in saved for drive_interp_restore */
void drive_interp_apply(RaceSim *sim, const DriveInterpPose *a, const DriveInterpPose *b, int32_t alpha,
                        DriveInterpPose *saved);
void drive_interp_restore(RaceSim *sim, const DriveInterpPose *saved);
/* elapsed / step as 16.16, held to 0..DRIVE_INTERP_ONE */
int32_t drive_interp_alpha(uint64_t elapsed, uint64_t step);

#endif
