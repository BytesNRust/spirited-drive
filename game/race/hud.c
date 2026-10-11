/* The race's heads-up display (hud.h). */
#include "game/race/hud.h"

#include "game/gfx/blit.h"

enum {
    TICKS_A_MINUTE = 3600, TICKS_A_SECOND = 60, TICKS_A_TENTH = 6
};

void hud_clock_split(HudClock *clock, int32_t ticks)
{
    const int32_t t = ticks <= HUD_START_TICKS ? 0 : ticks - HUD_START_TICKS;
    clock->minutes = t / TICKS_A_MINUTE;
    clock->seconds = t % TICKS_A_MINUTE / TICKS_A_SECOND;
    clock->tenths = t % TICKS_A_MINUTE % TICKS_A_SECOND / TICKS_A_TENTH;
}

void hud_draw_number(const QuadTarget *t, uint8_t *const *digit_shapes, int32_t value, int32_t x, int32_t y,
                     int32_t step, int32_t digits)
{
    if (value < 0 || digits > HUD_MAX_DIGITS) return;
    int32_t digit[HUD_MAX_DIGITS];
    for (int32_t i = 0; i < digits; i++) {
        digit[i] = value % 10;
        value /= 10;
    }
    for (int32_t i = digits - 1; i >= 0; i--, x += step) blit_shape(t, digit_shapes[digit[i]], x, y, BLIT_OPAQUE);
}
