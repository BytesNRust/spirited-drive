/* port: test harness only (drive_autopilot.h). */
#include "viewer/drive_autopilot.h"

#include "game/race/car_control.h"

#include <stdlib.h>
#include <string.h>

enum {
    STEER_LEFT = 0x00, STEER_CENTRE = 0x20, STEER_RIGHT = 0x3f,  /* the word's low six bits */
    PEDAL_SHIFT = 6, PEDAL_THROTTLE = 0x00, PEDAL_BRAKE = 0x3f,    /* the next six */
    STEER_AT = 0x30000,   /* an aim past this (24-bit angle) is steered for */
    BRAKE_AT = 0x180000,  /* past this, at speed, braked into */
    BRAKE_SPEED = 0x120000,
    GEAR_UP = 0x1000, NEUTRAL = -1  /* the gear up key's bit; car->gear in neutral */
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

uint32_t drive_autopilot_word(const Car *car, const RaceState *race)
{
    const int32_t aim = car_road_aim(car, race);
    const uint32_t steer = aim > STEER_AT ? STEER_RIGHT : aim < -STEER_AT ? STEER_LEFT : STEER_CENTRE;
    const int sharp = (aim > BRAKE_AT || aim < -BRAKE_AT) && car->speed > BRAKE_SPEED;
    const uint32_t gear = car->gear == NEUTRAL && car->gear_key == 0 ? GEAR_UP : 0;  /* a press after a release */
    return steer | (uint32_t)(sharp ? PEDAL_BRAKE : PEDAL_THROTTLE) << PEDAL_SHIFT | gear;
}
