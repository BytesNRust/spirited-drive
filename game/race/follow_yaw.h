/* port (port_follow_camera): the follow camera's yaw, a critically damped spring toward the car's heading. 24-bit angles
 * (0x1000000 a turn), a step a race tick (30 a second). */
#ifndef TNFS_FOLLOW_YAW_H
#define TNFS_FOLLOW_YAW_H

#include <stdint.h>

enum {
    FOLLOW_YAW_STIFFNESS = 594,      /* 16.16 a tick squared: (1 / 0.35 s) squared at 30 a second */
    FOLLOW_YAW_DAMPING = 12483,      /* 16.16 a tick: twice that rate, critical damping */
    FOLLOW_YAW_MAX_VELOCITY = 400000 /* an angle a tick: above the game's top turn rate (0x960000 a second = 327680) */
};

typedef struct {
    int32_t yaw;        /* 0 .. 0xffffff */
    int32_t velocity;   /* an angle a tick */
} FollowYaw;

/* One tick toward `target` (any 24-bit angle) the shortest way, the target itself turning `target_rate` a tick: the spring's
 * damping is toward that rate, so a steady turn is followed without lag. Opposite the camera, the way the target turns. */
void follow_yaw_step(FollowYaw *s, int32_t target, int32_t target_rate);

/* The same with the spring `boost` times as fast (its rate: stiffness boost squared, damping boost, so it stays critically damped); 1 is follow_yaw_step. */
void follow_yaw_step_boost(FollowYaw *s, int32_t target, int32_t target_rate, int32_t boost);

#endif
