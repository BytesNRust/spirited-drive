/* what the files split out of ported_view.c (view_*.c) take from it: the race, the camera, the light, the cars drawn lately. Defined in ported_view.c. */
#ifndef TNFS_VIEW_SHARED_H
#define TNFS_VIEW_SHARED_H

#include <stdint.h>

enum {
    WORLD_SHIFT = 11,      /* 004c52bc */
    EYE_HEIGHT = 111411,   /* camera mode 2 (004b6bb0): 1.7 m */
    SKY_WORLD_SHIFT = 8,   /* 004bb8d4 */
    SKY_POSITION_SHIFT = 10  /* 0043af80: -position >> 10 */
};
enum { SKID_HIGH_RES = 1 };  /* 004fa0c0, as drive_hud's HIGH_RES */

#include "game/gfx/object3d.h"
#include "game/race/car_draw.h"
#include "game/race/car_model.h"
#include "game/race/car_struct.h"
#include "game/race/crash_trail.h"
#include "game/race/row_cars.h"
#include "viewer/ported_view.h"
#include "game/race/model_draw.h"
#include "game/race/psx_light.h"
#include "game/race/puffs.h"
#include "game/race/race_state.h"
#include "viewer/object_sink.h"

/* The cars on the road, each drawn with its row (0045a848). Slot 0 is
 * also the parked car of ported_view_render. */
typedef struct {
    Object3D objects[2];
    Object3D *high, *low;   /* objects, or the borrowed ones */
    CarModel model;
    int ok;
    int borrowed;
    CarPose pose;
    int32_t node;
    Car *car;
} ViewCar;

extern RaceState *shadow_race;
extern ModelView model_view;
extern ObjectSink object_pixels;
extern int32_t (*puff_rand)(void *ctx, int32_t n);
extern const PuffArt *puff_art;
extern const PuffBlend *puff_blend;
extern const PsxLight *psx_light;   /* the track's light, or NULL */
extern int32_t body_drawn_tick[RACE_CARS];   /* the tick each car's body was last drawn */
const Car *followed_car(void);
int world_screen(const int32_t place[3], int32_t camera[3], int32_t screen[2]);
PuffView puff_view(void);
int view_weather(void);
const uint8_t *road_tunnels(const RaceState *race, uint32_t nodes);   /* the nodes the road itself marks a tunnel */
const uint8_t *rain_cover(const RaceState *race, uint32_t nodes);     /* the nodes with a roof over the road for the rain */

int ratio_of_car(const Car *car, float out[3]);
void shadow_contact(void *ctx, Car *car);
void shadow_followed(void *ctx, Car *car, const int32_t f[3], const int32_t s[3], const int32_t p[3]);
extern const uint8_t *shadow_remap;
void smoke_draw(void *ctx, int in_car);
extern int weather;
extern int32_t last_near;
extern int car_count, car_ahead;
extern ViewCar cars[PORTED_VIEW_CARS];
extern int32_t last_near_dir[3];
extern int police_flash_on;
extern CrashTrail *puff_trail;
extern uint8_t ahead_rows[ROW_CARS_AHEAD];
extern int arm_drawing;
extern uint8_t behind_rows[ROW_CARS_BEHIND];
const uint8_t *car_light(int32_t node, const double *xz, int car, const uint8_t palette[768]);
const uint8_t *car_table(const Car *car, const uint8_t palette[768]);
const View *modern_view(const View *view);
extern float lit_tint[3];
extern int lit_tinted;
extern int reflect_lamps_on, reflect_dark;
extern double reflect_x, reflect_z, reflect_hx, reflect_hz;
#endif
