/* The race's heads-up display (hud.h). */
#include "game/race/hud.h"

#include "game/gfx/blit.h"
#include "game/sys/fixed_math.h"

enum {
    MPH = 0x23d70, KMH = 0x39999,  /* 16.16 from the speed's units */
    AI_GEARS = 6,
    YELLOW_BLANK = 14,
    YELLOW_FIRST_GEAR = 1, YELLOW_REVERSE = 10, YELLOW_NEUTRAL = 11,
    SECONDS_LIMIT = 1200,
    SECONDS_GAP_HIGH = 8, SECONDS_GAP_LOW = 2,
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

void hud_draw_number_blank(const QuadTarget *t, uint8_t *const *digit_shapes, int32_t value, int32_t x, int32_t y,
                           int32_t step, int32_t digits)
{
    if (value < 0 || digits > HUD_MAX_DIGITS) return;
    int32_t digit[HUD_MAX_DIGITS];
    for (int32_t i = 0; i < digits; i++) {
        digit[i] = value % 10;
        value /= 10;
    }
    int leading = 1;
    for (int32_t i = 0; i < digits; i++, x += step) {
        const int32_t d = digit[digits - 1 - i];
        if (d == 0 && i < digits - 1 && leading) {
            blit_shape(t, digit_shapes[YELLOW_BLANK], x, y, BLIT_OPAQUE);
        } else {
            blit_shape(t, digit_shapes[d], x, y, BLIT_OPAQUE);
            leading = 0;
        }
    }
}

void hud_draw_seconds(const QuadTarget *t, uint8_t *const *yellow, int32_t ticks, int32_t high_res, HudPlace high,
                      HudPlace low)
{
    if (ticks > SECONDS_LIMIT) ticks = 0;
    const HudPlace p = high_res != 0 ? high : low;
    const int32_t gap = high_res != 0 ? SECONDS_GAP_HIGH : SECONDS_GAP_LOW;
    hud_draw_number_blank(t, yellow, ticks / TICKS_A_SECOND, p.x, p.y, p.step, 2);
    hud_draw_number_blank(t, yellow, ticks % TICKS_A_SECOND / TICKS_A_TENTH, p.step * 2 + p.x + gap, p.y, p.step, 1);
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

static int32_t computer_gear(const Car *car)
{
    int32_t gear = 0;
    while (gear < AI_GEARS && car->ai_speed > car->pdn_1ac[gear]) gear++;
    return gear;
}

void hud_draw_gear(const QuadTarget *t, HudState *hud, const DashArt *dash, const Car *car, const RaceState *race,
                   int32_t x, int32_t y)
{
    const int32_t gear = race->viewed < race->humans ? car->gear : computer_gear(car);
    if (gear == hud->gear_shown) return;
    const int shape = gear >= 0 ? YELLOW_FIRST_GEAR + gear : gear == -1 ? YELLOW_NEUTRAL : YELLOW_REVERSE;
    if (shape >= DASH_YELLOW) return;  /* port guard: the original reads past the yellow shapes */
    blit_shape(t, dash->yellow[shape], x, y, BLIT_OPAQUE);
    hud->gear_shown = gear;
}
