/* The race's heads-up display (hud.h). */
#include "game/race/hud.h"

#include "game/gfx/blit.h"
#include "game/sys/fixed_math.h"

enum {
    MPH = 0x23d70, KMH = 0x39999,  /* 16.16 from the speed's units */
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

void hud_draw_speed(const QuadTarget *t, HudState *hud, const DashArt *dash, const Car *car, const RaceState *race,
                    int16_t german, int32_t x, int32_t y, int32_t step)
{
    int32_t speed = race->viewed < race->humans ? car->local_velocity_z : car->ai_speed;
    if (speed < 1) speed = (int32_t)(0u - (uint32_t)speed);
    const int32_t units = fx_mul_round(speed, german != 0 ? KMH : MPH) >> 16;
    if (units < 0) return;
    int32_t digit[HUD_SPEED_DIGITS];
    int32_t value = units;
    for (int i = 0; i < HUD_SPEED_DIGITS; i++) {
        digit[i] = value % 10;
        value /= 10;
    }
    int leading = 1;
    for (int i = 0; i < HUD_SPEED_DIGITS; i++, x += step) {
        const int32_t d = digit[HUD_SPEED_DIGITS - 1 - i];
        const int32_t shown = d == 0 && i < HUD_SPEED_DIGITS - 1 && leading ? HUD_BLANK : d;
        if (shown != HUD_BLANK) leading = 0;
        if (hud->speed_shown[i] != shown) {
            blit_shape(t, dash->red[shown], x, y, BLIT_OPAQUE);
            hud->speed_shown[i] = shown;
        }
    }
}
