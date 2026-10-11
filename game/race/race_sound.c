/* The race's sound levels (race_sound.h). */
#include "game/race/race_sound.h"

enum {
    PAN_CENTRE = 0x40,            /* angle >> 16: 0x40 a quarter turn */
    PAN_BACK_FROM = 0xc0,         /* 0xc0..0x100: the last quarter, back to centre */
    PAN_BACK_TO = 0x100,
    PAN_BACK = 0x13f,
    PAN_MAX = 0x7f,
    HEARD_WITHIN = 0x200000,
    LOUDNESS_MAX = 0x7f,
    LOUDNESS_SHIFT = 8,
    LOUDNESS_DIVIDE = 0x2000      /* 0x200000 >> 8 */
};

int32_t race_sound_pan(int32_t angle)
{
    int32_t a = angle >> 16;
    int32_t pan;
    if (a <= PAN_CENTRE && a >= 0)
        pan = PAN_CENTRE - a;
    else if (a >= PAN_BACK_FROM && a <= PAN_BACK_TO)
        pan = PAN_BACK - a;
    else
        pan = a - PAN_CENTRE;
    if (pan < 0)
        pan = 0;
    if (pan > PAN_MAX)
        pan = PAN_MAX;
    return pan;
}

int32_t race_sound_loudness(int32_t distance)
{
    if (distance >= HEARD_WITHIN)
        return 0;
    int32_t left = (int32_t)((uint32_t)HEARD_WITHIN - (uint32_t)distance) >> LOUDNESS_SHIFT;
    return left * LOUDNESS_MAX / LOUDNESS_DIVIDE;
}
