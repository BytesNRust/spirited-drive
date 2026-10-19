/* port: test harness only (drive_autopilot.h). */
#include "viewer/drive_autopilot.h"

#include "game/race/car_control.h"
#include "game/race/car_gearbox.h"

#include <stdlib.h>
#include <string.h>

enum {
    STEER_LEFT = 0x00, STEER_CENTRE = 0x20, STEER_RIGHT = 0x3f,  /* the word's low six bits */
    PEDAL_SHIFT = 6, PEDAL_THROTTLE = 0x00, PEDAL_BRAKE = 0x3f,    /* the next six */
    STEER_AT = 0x30000,   /* an aim past this (24-bit angle) is steered for */
    BRAKE_AT = 0x180000,  /* past this, at speed, braked into */
    BRAKE_SPEED = 0x120000,
    GEAR_UP = 0x1000, GEAR_DOWN = 0x2000, NEUTRAL = -1  /* the gear keys' bits; car->gear in neutral */
};

int drive_autopilot_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("TNFS_AUTOPILOT");
        on = v != NULL && strcmp(v, "1") == 0;
    }
    return on;
}

/* The gear key to press this tick: up out of neutral, and for a manual the automatic's own shift (004098a0 on a
 * copy); none the tick after a press, so each is a press and a release */
static uint32_t gear_word(const Car *car, const CarSpecs *specs)
{
    if (car->gear_key != 0) return 0;
    if (car->gear == NEUTRAL) return GEAR_UP;
    if (car->transmission != 0 || specs == NULL || car->gear < 0) return 0;
    Car shifted = *car;
    car_auto_shift(&shifted, specs);
    return shifted.gear > car->gear ? GEAR_UP : shifted.gear < car->gear ? GEAR_DOWN : 0;
}

uint32_t drive_autopilot_word(const Car *car, const RaceState *race, const CarSpecs *specs)
{
    const int32_t aim = car_road_aim(car, race);
    const uint32_t steer = aim > STEER_AT ? STEER_RIGHT : aim < -STEER_AT ? STEER_LEFT : STEER_CENTRE;
    const int sharp = (aim > BRAKE_AT || aim < -BRAKE_AT) && car->speed > BRAKE_SPEED;
    return steer | (uint32_t)(sharp ? PEDAL_BRAKE : PEDAL_THROTTLE) << PEDAL_SHIFT | gear_word(car, specs);
}
