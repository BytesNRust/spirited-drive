/* what the files split out of ported_view.c (view_*.c) take from it: the race, the camera, the light, the cars drawn lately. Defined in ported_view.c. */
#ifndef TNFS_VIEW_SHARED_H
#define TNFS_VIEW_SHARED_H

#include <stdint.h>

#include "game/race/car_struct.h"
#include "game/race/model_draw.h"
#include "game/race/psx_light.h"
#include "game/race/puffs.h"
#include "game/race/race_state.h"
#include "viewer/object_sink.h"

extern RaceState *shadow_race;
extern ModelView model_view;
extern ObjectSink object_pixels;
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

#endif
