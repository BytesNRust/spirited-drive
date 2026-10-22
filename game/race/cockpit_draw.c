/* The cockpit's per-frame pieces (0042270c's calls). */
#include "game/race/cockpit.h"

#include "game/gfx/blit.h"
#include "game/sys/fixed_math.h"
#include "game/race/car_struct.h"

#include <stddef.h>

enum {
    BIG_LIGHT = 2, FIRST_SMALL = 3, SMALL_LIGHTS = 5,
    GATE_ORIGIN = 1, GATE_STEPS = 4, SHAPE_W = 4, SHAPE_H = 6,
    TICKS_SHIFTED = 0xb, TICKS_DOWN = 0xc, TICKS_UP = 0xd, TICKS_FROM = 0xe
};

/* 00473764: opaque at (x, y); a NULL shape draws nothing (port guard). */
static void put(const QuadTarget *t, const uint8_t *shape, CockpitPoint at)
{
    if (shape != NULL) blit_shape(t, shape, at.x, at.y, BLIT_OPAQUE);
}

static void small(const QuadTarget *t, const Cockpit *c, const uint8_t *shape, int32_t i)
{
    if (FIRST_SMALL + i >= 0) put(t, shape, c->dash_points[FIRST_SMALL + i]);
}

void cockpit_lights(const QuadTarget *t, const Cockpit *c, int32_t count, int32_t big_lit, int32_t *shown)
{
    const int32_t lit = count > SMALL_LIGHTS ? SMALL_LIGHTS : count;
    if (count != 0) put(t, big_lit != 0 ? c->big_lit : c->big_dark, c->dash_points[BIG_LIGHT]);
    if (count != *shown) {
        if (count == 0) {
            put(t, c->big_dark, c->dash_points[BIG_LIGHT]);
            for (int32_t i = 0; i < SMALL_LIGHTS; i++) small(t, c, c->small_dark, i);
        } else {
            for (int32_t i = 0; i < lit; i++) small(t, c, c->small_lit, i);
            for (int32_t i = lit; i < SMALL_LIGHTS; i++) small(t, c, c->small_dark, i);
        }
    }
    *shown = count;
}

static CockpitPoint gate_point(const Cockpit *c, int32_t i)
{
    return i >= 0 && i < COCKPIT_GATE_POINTS ? c->gate_points[i] : (CockpitPoint){ 0, 0 };
}

static int32_t half(const uint8_t *shape, int at)
{
    return (int16_t)(shape[at] | shape[at + 1] << 8) / 2;
}

/* The light centred at (x, y) with 0xff skipped (00473a98). */
static void light(const QuadTarget *t, const uint8_t *shape, int32_t x, int32_t y)
{
    if (shape != NULL) blit_shape_at_transparent(t, shape, x - half(shape, SHAPE_W), y - half(shape, SHAPE_H));
}

/* The state 0041f5bc picks and its point. */
static int32_t gate_state(const Cockpit *c, const Car *car, CockpitPoint *at)
{
    const CockpitPoint a = gate_point(c, car->gear_before_last + 2), b = gate_point(c, car->gear + 2);
    const int32_t ticks = car->gear_change_ticks, origin_y = c->gate_points[GATE_ORIGIN].y;
    if (ticks < TICKS_SHIFTED && car->shift_timer < 1) return *at = b, 1;
    if (ticks >= TICKS_FROM) return *at = a, 2;
    if (ticks == TICKS_UP) return *at = (CockpitPoint){ a.x, origin_y }, 3;
    if (ticks == TICKS_DOWN) return *at = (CockpitPoint){ b.x, origin_y }, 4;
    if (ticks == TICKS_SHIFTED && car->shift_timer < 1) return *at = b, 5;
    return *at = (CockpitPoint){ 0, 0 }, 0;
}

void cockpit_gate(const CockpitSelectOps *ops, HudState *hud, const Cockpit *c, const Car *car,
                  const CockpitGateInputs *in, int32_t *state)
{
    QuadTarget *t = ops->select(ops->ctx, in->back_buffer != 0 ? HUD_SURFACE_SCREEN : HUD_SURFACE_DEFAULT);
    if (in->use_parts != 0) return;
    hud_view_clip(t, hud, &in->view_clip);
    if (in->gate_always == 0 && car->gear_change_ticks < 1) {
        if (*state != 0 && c->gate_back != NULL) blit_shape_placed(t, c->gate_back);
        *state = 0;
        return;
    }
    CockpitPoint at;
    const int32_t now = gate_state(c, car, &at);
    if (now != *state) {
        if (c->gate != NULL) blit_shape_placed(t, c->gate);
        if (c->light_count == COCKPIT_LIGHTS) {
            const CockpitPoint origin = c->gate_points[GATE_ORIGIN];
            const int32_t dx = (int32_t)((uint32_t)at.x - (uint32_t)origin.x) / GATE_STEPS;
            const int32_t dy = (int32_t)((uint32_t)at.y - (uint32_t)origin.y) / GATE_STEPS;
            for (int32_t k = COCKPIT_LIGHTS - 1, i = 0; k >= 0; k--, i++)
                light(t, c->lights[k], (int32_t)((uint32_t)origin.x + (uint32_t)(i * dx)),
                      (int32_t)((uint32_t)origin.y + (uint32_t)(i * dy)));
        } else {
            light(t, c->lights[0], at.x, at.y);
        }
    }
    *state = now;
}

/* The point at 004f97a4 + 8 k: dash points 0..9 below it, then its own
 * word pair, the tachometer's, the speedometer's and the gate's points, as
 * they follow each other in the original. Further: (0, 0) (port guard: the
 * original reads on). */
static CockpitPoint gauge_point(const Cockpit *c, int32_t k)
{
    if (k >= -COCKPIT_DASH_POINTS && k < 0) return c->dash_points[COCKPIT_DASH_POINTS + k];
    if (k == 0) return (CockpitPoint){ c->unknown_4f97a4[0], c->unknown_4f97a4[1] };
    if (k >= 1 && k <= COCKPIT_GAUGE_POINTS) return c->tach_points[k - 1];
    if (k > COCKPIT_GAUGE_POINTS && k <= 2 * COCKPIT_GAUGE_POINTS) return c->sped_points[k - 1 - COCKPIT_GAUGE_POINTS];
    if (k > 2 * COCKPIT_GAUGE_POINTS && k <= 2 * COCKPIT_GAUGE_POINTS + COCKPIT_GATE_POINTS)
        return c->gate_points[k - 1 - 2 * COCKPIT_GAUGE_POINTS];
    return (CockpitPoint){ 0, 0 };
}

static int32_t wrap_add(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
static int32_t wrap_sub(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }
static int32_t wrap_mul(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }
static int32_t wrap_shl16(int32_t a) { return (int32_t)((uint32_t)a << 16); }
static int32_t sar(int32_t a, int n) { return a >= 0 ? a >> n : ~(~a >> n); }
static int32_t div_trunc(int32_t a, int32_t b) { return a == INT32_MIN && b == -1 ? a : a / b; }

enum {
    TACH_BASE = 1, SPED_CENTRE = COCKPIT_GAUGE_POINTS, SPED_BASE = COCKPIT_GAUGE_POINTS + 1,
    REDLINE_PAST = 0x3d4, MPH = 0x23d70, KMH = 0x39999, BLANK = 10, DIGIT_PLACES = 3,
    MODEL_WIDE_DIGITS = 3, MODEL_CLEAR_DIGITS = 0xb, NEEDLE_WIDTH = 0x20000, LOW_THINNER = 0x10000
};

/* The needle from the point `centre` to `tip` (16.16) in 00482a36's
 * colour, both widths 004b91dc / 004b91e0's (0x20000 for every model, one
 * pixel less at low resolution). */
static void gauge_needle(const QuadTarget *t, NeedleLine *line, const CockpitGaugeInputs *in, CockpitPoint centre,
                         int32_t tip_x, int32_t tip_y)
{
    const int32_t width = in->high != 0 ? NEEDLE_WIDTH : NEEDLE_WIDTH - LOW_THINNER;
    needle_draw(line, t, wrap_shl16(centre.x), wrap_shl16(centre.y), tip_x, tip_y, in->fade, width, width);
}

static void gauge_digits(const QuadTarget *t, const Cockpit *c, const CockpitGaugeInputs *in, int32_t speed)
{
    int32_t x, y, step;
    if (in->high != 0) {
        if (in->model == MODEL_WIDE_DIGITS) x = 0x131, y = 0x16d, step = 9;
        else x = 0xb0, y = 0xeb, step = 0xe;
    } else {
        if (in->model == MODEL_WIDE_DIGITS) x = 0x99, y = 0x98, step = 4;
        else x = 0x58, y = 0x62, step = 7;
    }
    int32_t digit[DIGIT_PLACES];
    for (int32_t i = 0; i < DIGIT_PLACES; i++) {
        if (speed < 10) digit[i] = speed, speed = 0;
        else digit[i] = speed % 10, speed /= 10;
    }
    int leading = 1;
    for (int32_t i = 0; i < DIGIT_PLACES; i++, x = wrap_add(x, step)) {
        const int32_t d = digit[DIGIT_PLACES - 1 - i];
        const uint8_t *shape;
        if (d == 0 && i < 2 && leading) {
            shape = c->digits[BLANK];
        } else {
            leading = 0;
            shape = d >= 0 && d < BLANK ? c->digits[d] : NULL;
        }
        if (shape == NULL) continue;
        if (in->model == MODEL_CLEAR_DIGITS) blit_shape_at_transparent(t, shape, x, y);
        else blit_shape(t, shape, x, y, BLIT_OPAQUE);
    }
}

void cockpit_gauges(const QuadTarget *t, NeedleLine *line, const Cockpit *c, const CockpitGaugeInputs *in)
{
    const int32_t top = wrap_add(in->redline, REDLINE_PAST);
    const int32_t revs = in->rpm > top ? top : in->rpm;
    const int32_t step = sar(revs, 10), part = wrap_sub(revs, (int32_t)((uint32_t)step << 10));
    const CockpitPoint a = gauge_point(c, TACH_BASE + step), b = gauge_point(c, TACH_BASE + step + 1);
    gauge_needle(t, line, in, gauge_point(c, c->tach_count),
                 wrap_shl16(wrap_add(a.x, sar(wrap_mul(wrap_sub(b.x, a.x), part), 10))),
                 wrap_shl16(wrap_add(a.y, sar(wrap_mul(part, wrap_sub(b.y, a.y)), 10))));

    const int32_t speed_abs = in->speed > 0 ? in->speed : (int32_t)(0u - (uint32_t)in->speed);
    const int32_t speed = sar(fx_mul_round(speed_abs, in->kmh != 0 ? KMH : MPH), 16);
    if (c->digits_on != 0) {
        gauge_digits(t, c, in, speed);
        return;
    }
    const int32_t per = in->kmh != 0 ? 20 : 10;
    int32_t q = 0, r = 0;
    if (speed > 0) q = speed / per, r = speed - q * per;
    const CockpitPoint s = gauge_point(c, SPED_BASE + q), e = gauge_point(c, SPED_BASE + q + 1);
    gauge_needle(t, line, in, gauge_point(c, SPED_CENTRE + c->sped_count),
                 wrap_shl16(wrap_add(s.x, div_trunc(wrap_mul(r, wrap_sub(e.x, s.x)), per))),
                 wrap_shl16(wrap_add(s.y, div_trunc(wrap_mul(wrap_sub(e.y, s.y), r), per))));
}
