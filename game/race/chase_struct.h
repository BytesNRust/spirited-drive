/* The follow camera's records: the one in use (004f801c) and the view kinds
 * (004b6bb0). The port's layout differs where there are pointers;
 * tools/probe_car.c maps them back to the original's offsets. */
#ifndef TNFS_CHASE_STRUCT_H
#define TNFS_CHASE_STRUCT_H

#include "game/race/car.h"
#include "game/race/race_view.h"

#include <stdint.h>

enum {
    CHASE_KINDS = 14,           /* 004b6bb0: 0x58 bytes a row, up to 004b7080 */
    RACE_CAMERAS = 3            /* 004b78a8: 0x20 bytes each; the kinds use 0 and 1, camera2 is 2 */
};

/* ChaseKind.cycle's columns (0040594c) */
enum { CYCLE_RACING, CYCLE_IDLE, CYCLE_OWN };

/* What a kind's +0x10 points at. 004046b0 sets it before every read. */
enum {
    YAW_UNSET,                  /* NULL, as the exe starts */
    YAW_CAR,                    /* yaw_car's +0x10 */
    YAW_ROAD                    /* 004046b0's road heading, a local of that call */
};

/* A view kind, a row of 004b6bb0 (0x58 bytes). 004046b0 writes target and
 * yaw_source while the camera runs. */
typedef struct {
    int32_t camera;             /* +0: the 004b78a8 record */
    int32_t target[3];          /* +4: where the eye sits from the car */
    int32_t yaw_source;         /* +0x10: the yaw the camera turns to (YAW_*) */
    int32_t scale;              /* +0x14: how far ahead along the road the camera looks, 16.16 */
    int32_t mode;               /* +0x18: 0 still, 1 and 2 slide sideways with the car, 3 the car's yaw, 4 the road's */
    /* +0x1c: the kind 0040594c moves on to: [CYCLE_RACING] racing with a
     * replay or another car shown, [CYCLE_IDLE] not racing (phase not 1),
     * [CYCLE_OWN] racing, the player's own car live; [3] unknown */
    int32_t cycle[4];
    int32_t shift;              /* +0x2c: the glide's ticks (00404630) and the yaw's easing shift */
    int32_t divide;             /* +0x30: the glide divides by its ticks; the glide may end near the target */
    int32_t unknown_34;
    int32_t unknown_38;         /* +0x38: to the record's +0x68 */
    int32_t glide_ticks;        /* +0x3c: the record's gliding when the kind is taken */
    int32_t next;               /* +0x40: the kind taken when the glide ends, -1 none */
    int32_t z_limit;            /* +0x44: the glide ends once target z reaches it */
    int32_t alt_target[3];      /* +0x48: the target when the kind is first taken with alt_flag set */
    int32_t alt_flag;           /* +0x54 */
    const Car *yaw_car;         /* port: the car of YAW_CAR */
} ChaseKind;

/* The follow record (0x94 bytes at 004f801c; 00511fe0 points at it, the
 * only one). Offsets are the original's. */
typedef struct {
    int32_t kind;               /* +0: a 004b6bb0 row; above 1 the sounds pan by position */
    int32_t next_kind;          /* +4 (004f8020): 7 while the race is being set up, 5 attached to a car */
    int32_t unknown_08;
    int32_t gliding;            /* +0xc: ticks left while target moves by step (00404630) */
    int32_t position[3];        /* +0x10: the eye as last placed (004f802c, the sounds' listener) */
    int32_t eye[3];             /* +0x1c */
    const Car *base;            /* +0x28: offset is from this car's position (+0) */
    int32_t offset[3];          /* +0x2c */
    int32_t node;               /* +0x38: the eye's road node (004f8054) */
    int32_t flags;              /* +0x3c: bit 0 the scenery must be streamed again (004f8058) */
    int32_t angles[3];          /* +0x40: aim as last placed: pitch, yaw (004f8060), roll */
    int32_t aim[3];             /* +0x4c: the car's angles + turn, to 0x400 */
    const Car *car_angles;      /* +0x58: this car's angles (+0xc) */
    int32_t turn[3];            /* +0x5c */
    int32_t unknown_68;         /* +0x68: the kind's +0x38 */
    int32_t target[3];          /* +0x6c: where the eye sits from the car */
    int32_t yaw;                /* +0x78 */
    int32_t step[3];            /* +0x7c: added to target each tick while gliding */
    RaceCamera *camera;         /* +0x88: a 004b78a8 record */
    RaceCamera *camera2;        /* +0x8c: 004b78e8 */
    Car *car;                   /* +0x90: the car followed */
    const Car *port_follow_car; /* port (port_follow_camera): the car the yaw spring below was started for */
    int32_t port_follow_yaw, port_follow_velocity;   /* its state (follow_yaw.h) */
    int32_t port_look_fast;     /* the look back's quicker spring is on: while it is held and until the camera is back behind the car */
    int32_t port_look_back;     /* port (port_follow_camera): the look back key is held: the camera's target is the car's heading turned half a turn */
} RaceChase;

#endif
