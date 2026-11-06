#include "game/race/analog_pad.h"

enum { STEER_LEFT_KEY = 0, STEER_CENTRE = 0x20, STEER_RIGHT_KEY = 0x3c, PEDAL_REST = 0x1e, PEDAL_ACCELERATE_KEY = 5, PEDAL_BRAKE_KEY = 0x3c };

int analog_pad_steering(int on, uint32_t input)
{
    const uint32_t s = input & 0x3f;
    return on != 0 && s != STEER_LEFT_KEY && s != STEER_CENTRE && s != STEER_RIGHT_KEY;
}

int analog_pad_pedal(int on, uint32_t input)
{
    const uint32_t p = (input >> 6) & 0x3f;
    return on != 0 && p != PEDAL_REST && p != PEDAL_ACCELERATE_KEY && p != PEDAL_BRAKE_KEY;
}
