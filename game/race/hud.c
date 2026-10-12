/* The race's heads-up display (hud.h). */
#include "game/race/hud.h"

#include "game/gfx/blit.h"
#include "game/gfx/clip.h"
#include "game/gfx/flat.h"
#include "game/sys/fixed_math.h"
#include "game/sys/shape_bank.h"

#include <stddef.h>
#include <string.h>

enum {
    MPH = 0x23d70, KMH = 0x39999,  /* 16.16 from the speed's units */
    AI_GEARS = 6,
    YELLOW_BLANK = 14, YELLOW_PLUS = 12, YELLOW_MINUS = 13,
    YELLOW_FIRST_GEAR = 1, YELLOW_REVERSE = 10, YELLOW_NEUTRAL = 11,
    SECONDS_LIMIT = 1200,
    DISTANCE_SCALE = 0x30, MILE_NODES = 1600, KM_NODES = 1000,
    YELLOW_ONE = 1,
    LIGHTS_START = 300, LIGHT_TICKS = 60, RED_LIGHTS = 3,
    LIGHT_STEP_LOW = 0x41, LIGHT_STEP_HIGH = 0x82,
    STRIP_W_LOW = 0x140, STRIP_H_LOW = 0x10, STRIP_W_HIGH = 0x280, STRIP_H_HIGH = 0x20,
    LIGHTS_HOLD = 10, LIGHTS_RESET = 0x7d000,
    VIEW_H_HIGH = 0x1c0, VIEW_H_LOW = 0xb4,  /* 0041e3f4's box height and top */
    LABELS_FROM = 0x168, RACE_TIME_FROM = 0x186,
    START_NODES = 12,
    TOP_ROW_FROM = 0x169, LAST_LAP_SHOWN = 0xf0,
    RECORDS_TRACK = 0xa6b, RECORDS_VARIANT = 0x27, RECORDS_CIRCUIT = 0xea, RECORDS_SLOT = 0x186,
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

/* 0041e9a0's places: the three times (high, low), then the counts. */
static const HudPlace SPLIT_HIGH[3] = { { 0x25, 6, 0x13 }, { 0x8d, 6, 0x13 }, { 0xf5, 6, 0x13 } };
static const HudPlace SPLIT_LOW[3] = { { 0x14, 1, 8 }, { 0x45, 1, 8 }, { 0x76, 1, 8 } };
static const int SPLIT_COUNT_ORDER[3] = { 3, 5, 4 };
static const HudPlace COUNT_HIGH[3] = { { 0x14f, 6, 0x13 }, { 0x234, 6, 0x13 }, { 0x1cc, 6, 0x13 } };
static const HudPlace COUNT_LOW[3] = { { 0xaa, 1, 8 }, { 0x11f, 1, 8 }, { 0xee, 1, 8 } };

void hud_draw_splits(const QuadTarget *t, HudState *hud, uint8_t *const *yellow, int32_t high_res)
{
    for (int k = 0; k < 3; k++) {
        if (hud->split_shown[k] == hud->split[k]) continue;
        hud_draw_seconds(t, yellow, hud->split[k], high_res, SPLIT_HIGH[k], SPLIT_LOW[k]);
        hud->split_shown[k] = hud->split[k];
    }
    for (int i = 0; i < 3; i++) {
        const int k = SPLIT_COUNT_ORDER[i];
        if (hud->split_shown[k] == hud->split[k]) continue;
        const HudPlace p = high_res != 0 ? COUNT_HIGH[i] : COUNT_LOW[i];
        hud_draw_number_blank(t, yellow, hud->split[k], p.x, p.y, p.step, 3);
        hud->split_shown[k] = hud->split[k];
    }
}

static uint8_t *gap_sign(const DashArt *dash, int32_t gap)
{
    return dash->yellow[gap < 0 ? YELLOW_PLUS : YELLOW_MINUS];
}

void hud_draw_gap(const QuadTarget *t, HudState *hud, const DashArt *dash, int32_t gap, int32_t x, int32_t y,
                  int32_t step, int32_t space)
{
    if (hud->gap_shown == HUD_GAP_UNSET) {
        const uint8_t *split = shape_bank_find(dash->bank, (const uint8_t *)"splt");
        if (split == NULL) return;  /* port guard: the original aborts */
        blit_shape_placed(t, split);
        blit_shape_placed(t, gap_sign(dash, gap));
    }
    if (gap == hud->gap_shown) return;
    if ((gap >= 0) != (hud->gap_shown >= 0)) blit_shape_placed(t, gap_sign(dash, gap));
    hud->gap_shown = gap;
    const int32_t v = gap < 1 ? (int32_t)(0u - (uint32_t)gap) : gap;
    hud->clock.minutes = v / TICKS_A_MINUTE;
    hud->clock.seconds = v % TICKS_A_MINUTE / TICKS_A_SECOND;
    hud->clock.tenths = v % TICKS_A_MINUTE % TICKS_A_SECOND / TICKS_A_TENTH;
    uint8_t *const *yellow = dash->yellow;
    hud_draw_number(t, yellow, hud->clock.minutes, x, y, step, 1);
    const int32_t seconds_x = x + step + space;
    hud_draw_number(t, yellow, hud->clock.seconds, seconds_x, y, step, 2);
    hud_draw_number(t, yellow, hud->clock.tenths, space + seconds_x + step * 2, y, step, 1);
    hud->lap_shown = HUD_GAP_UNSET;
    hud->distance_tens_shown = HUD_GAP_UNSET;
    hud->distance_ones_shown = HUD_GAP_UNSET;
}

/* A shape of the mask bank at its own place; nothing when the bank lacks
 * it (port guard: the original aborts). */
static void draw_named(const QuadTarget *t, const DashArt *dash, const char name[5])
{
    const uint8_t *shape = shape_bank_find(dash->bank, (const uint8_t *)name);
    if (shape != NULL) blit_shape_placed(t, shape);
}

static void draw_yellow(const QuadTarget *t, const DashArt *dash, int32_t index, int32_t x, int32_t y)
{
    if (index >= 0 && index < DASH_YELLOW)  /* port guard: the original reads the next globals */
        blit_shape(t, dash->yellow[index], x, y, BLIT_OPAQUE);
}

void hud_draw_distance(const QuadTarget *t, HudState *hud, const DashArt *dash, const Car *car, const RaceState *race,
                       int16_t german, int32_t x, int32_t y, int32_t step, int32_t space)
{
    if (hud->distance_tens_shown == HUD_GAP_UNSET) draw_named(t, dash, "mile");
    const int32_t nodes = german != 0 ? KM_NODES : MILE_NODES;
    const int32_t left = car->node < race->loop_nodes
        ? (int32_t)((uint32_t)(race->loop_nodes - car->node) * DISTANCE_SCALE) / nodes + 1 : 0;
    const int32_t tens = left / 10, ones = left % 10;
    const int32_t next = step + x;
    if (tens == hud->distance_tens_shown) {
        if (german != 0) x = next;
    } else {
        hud->distance_tens_shown = tens;
        if (german == 0) {
            draw_yellow(t, dash, tens, x, y);
        } else if (tens < 10) {
            draw_yellow(t, dash, YELLOW_BLANK, x, y);
            draw_yellow(t, dash, tens, next, y);
            x = next;
        } else {
            draw_yellow(t, dash, YELLOW_ONE, x, y);
            draw_yellow(t, dash, 0, next, y);
            x = next;
        }
    }
    if (ones != hud->distance_ones_shown) {
        hud->distance_ones_shown = ones;
        draw_yellow(t, dash, ones, x + step + space, y);
    }
    hud->gap_shown = HUD_GAP_UNSET;
}

void hud_draw_lap(const QuadTarget *t, HudState *hud, const DashArt *dash, const Car *car, const RaceState *race,
                  int32_t x, int32_t y, int32_t step, int32_t laps_x, int32_t laps_y)
{
    if (hud->lap_shown == HUD_GAP_UNSET) draw_named(t, dash, "lap#");
    const int32_t lap = race->laps_setting < car->lap ? race->laps_setting : car->lap;
    if (lap != hud->lap_shown) {
        hud->lap_shown = lap;
        if (lap < 10) {
            hud_draw_number(t, dash->yellow, lap, step + x, y, step, 1);
            draw_yellow(t, dash, YELLOW_BLANK, x, y);
        } else {
            hud_draw_number(t, dash->yellow, lap, x, y, step, 2);
        }
        hud_draw_number(t, dash->yellow, race->laps, laps_x, laps_y, step, race->laps < 10 ? 1 : 2);
    }
    hud->gap_shown = HUD_GAP_UNSET;
}

/* Which beep word goes with each light (00513e7e, 7c, 82, 7a, 80), and the
 * green one (00513e78). */
static const int LIGHT_BEEP[HUD_LIGHTS] = { 3, 2, 5, 1, 4 };
enum { GREEN_BEEP = 0 };

void hud_draw_start_lights(const QuadTarget *t, HudState *hud, const DashArt *dash, int16_t beeps[HUD_BEEPS],
                           int32_t tick, int32_t high_res, uint32_t strip_colour)
{
    if (hud->strip_dirty != 0) {
        if (high_res != 0) flat_fill_rect(t, 0, 0, STRIP_W_HIGH, STRIP_H_HIGH, strip_colour);
        else flat_fill_rect(t, 0, 0, STRIP_W_LOW, STRIP_H_LOW, strip_colour);
        hud->strip_dirty = 0;
    }
    const int32_t y = high_res != 0;
    const int32_t step = high_res != 0 ? LIGHT_STEP_HIGH : LIGHT_STEP_LOW;
    int32_t x = 0;
    if (tick < LIGHTS_START) {
        const int32_t lit = tick / LIGHT_TICKS;
        for (int32_t i = 0; i < HUD_LIGHTS; i++, x += step) {
            const int red = i < RED_LIGHTS;
            const uint8_t *shape;
            if (i > lit) {
                shape = red ? dash->srdd : dash->syld;
            } else {
                beeps[LIGHT_BEEP[i]] = 0;
                shape = red ? dash->srdl : dash->syll;
            }
            blit_shape(t, shape, x, y, BLIT_OPAQUE);
        }
    } else {
        for (int32_t i = 0; i < HUD_LIGHTS; i++, x += step) {
            beeps[GREEN_BEEP] = 0;
            blit_shape(t, dash->green, x, y, BLIT_OPAQUE);
        }
    }
    hud->lights_until = tick + LIGHTS_HOLD;
}

void hud_reset(HudState *hud)
{
    int32_t *const unset[] = {
        &hud->gear_shown, &hud->unknown_4f9a80[0], &hud->unknown_4f9a80[1], &hud->unknown_4f9a80[2],
        &hud->place_shown, &hud->lap_shown, &hud->gap_shown,
        &hud->unknown_4f9a50[2], &hud->unknown_4f9a50[3], &hud->unknown_4f9a50[4], &hud->unknown_4f9a50[5],
        &hud->distance_tens_shown, &hud->distance_ones_shown, &hud->unknown_4f9a74,
        &hud->speed_shown[0], &hud->speed_shown[1], &hud->speed_shown[2], &hud->unknown_4f9a40,
        &hud->tenths_shown, &hud->seconds_shown, &hud->minutes_shown,
        &hud->unknown_4f9a50[0], &hud->unknown_4f9a50[1]
    };
    hud->unknown_4f9a20[3] = 0;
    for (size_t i = 0; i < sizeof unset / sizeof unset[0]; i++) *unset[i] = HUD_GAP_UNSET;
    hud->lights_until = LIGHTS_RESET;
    hud->waiting = 0;
    for (int k = 0; k < HUD_SPLITS; k++) hud->split_shown[k] = 0;
}

void hud_view_clip(QuadTarget *t, HudState *hud, const HudViewClipInputs *in)
{
    const int32_t view_h = in->high_res != 0 ? VIEW_H_HIGH : VIEW_H_LOW;
    if (in->unknown_4c5390 != 1) {
        clip_box(t, 0, 0, in->width, view_h, in->surface_w, in->surface_h);
        return;
    }
    if (hud->unknown_4f9a20[1] != 0) {
        clip_box(t, 0, 0, in->width, in->height, in->surface_w, in->surface_h);
        return;
    }
    if (hud->unknown_4f9a20[2] == 0) return;
    clip_box(t, 0, view_h, in->width, in->height, in->surface_w, in->surface_h);
    hud->unknown_4f9a20[2] = 0;
}

void hud_draw_time(const QuadTarget *t, HudState *hud, uint8_t *const *grey, int32_t ticks, int32_t x, int32_t y,
                   int32_t step, int32_t space)
{
    hud_clock_split(&hud->clock, ticks);
    hud_draw_number(t, grey, hud->clock.minutes, x, y, step, 2);
    const int32_t seconds_x = x + step * 2 + space;
    hud_draw_number(t, grey, hud->clock.seconds, seconds_x, y, step, 2);
    hud_draw_number(t, grey, hud->clock.tenths, seconds_x + step * 2 + space, y, step, 1);
}

static void fill_strip(const QuadTarget *t, int32_t high_res, uint32_t colour)
{
    if (high_res != 0) flat_fill_rect(t, 0, 0, STRIP_W_HIGH, STRIP_H_HIGH, colour);
    else flat_fill_rect(t, 0, 0, STRIP_W_LOW, STRIP_H_LOW, colour);
}

/* A word of the saved records (00524ec0); 0 without them. */
static int32_t record_word(const RaceState *race, int32_t offset)
{
    if (race->records == NULL) return 0;
    const uint8_t *b = race->records + offset;
    return (int32_t)((uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24);
}

/* The record time's place: (x, y, step, space) low and high. */
static void draw_record(const QuadTarget *t, HudState *hud, const DashArt *dash, int32_t ticks, int32_t high_res)
{
    if (high_res != 0) hud_draw_time(t, hud, dash->grey_shown, ticks + HUD_START_TICKS, 0x1f, 8, 0xd, 7);
    else hud_draw_time(t, hud, dash->grey_shown, ticks + HUD_START_TICKS, 0x11, 3, 6, 2);
}

static uint8_t *label_sign(const DashArt *dash, int32_t gap)
{
    return dash->yellow[gap < 0 ? YELLOW_MINUS : YELLOW_PLUS];
}

void hud_draw_labels(const QuadTarget *t, HudState *hud, const DashArt *dash, const RaceState *race, int32_t gap,
                     int32_t gap_ticks, int32_t high_res, uint32_t strip_colour)
{
    if (hud->redraw == 0 || race->tick < LABELS_FROM) {
        fill_strip(t, high_res, strip_colour);
        return;
    }
    draw_named(t, dash, "stat");
    const int32_t track = race->track * RECORDS_TRACK;
    if (hud->mode == 1) {
        draw_named(t, dash, "rtet");
        const int32_t slot = race->circuit != 0 ? race->record_slot * RECORDS_SLOT : 0;
        draw_record(t, hud, dash, record_word(race, RECORDS_CIRCUIT + track + slot), high_res);
        draw_named(t, dash, "splt");
        blit_shape_placed(t, label_sign(dash, gap));
    } else if (hud->mode == 0) {
        draw_named(t, dash, "segt");
        if (gap_ticks < 1) {
            draw_named(t, dash, race->circuit != 0 ? "lap#" : "mile");
        } else {
            draw_named(t, dash, "splt");
            blit_shape_placed(t, label_sign(dash, gap));
        }
        const int32_t record = race->circuit != 0 ? race->best_lap
                                                  : record_word(race, track + race->track_variant * RECORDS_VARIANT);
        draw_record(t, hud, dash, record, high_res);
    } else if (hud->mode == 2) {
        blit_shape_placed(t, dash->acct);
        hud_draw_splits(t, hud, dash->yellow, high_res);
    }
    if (hud->mode < 2) draw_named(t, dash, race->race_type == 0 || race->race_type == 1 ? "head" : "sing");
    hud_reset(hud);
    hud->strip_dirty = 1;
}

/* A part of the race time, drawn when it changed. */
static void race_time_part(const QuadTarget *t, uint8_t *const *yellow, int32_t value, int32_t *shown, int32_t x,
                           int32_t y, int32_t step, int32_t digits)
{
    if (value == *shown) return;
    hud_draw_number(t, yellow, value, x, y, step, digits);
    *shown = value;
}

void hud_draw_race_time(const QuadTarget *t, HudState *hud, const DashArt *dash, RaceState *race, int32_t gap,
                        int32_t gap_ticks, int32_t high_res, uint32_t strip_colour, int32_t ticks, int32_t x,
                        int32_t y, int32_t step, int32_t space)
{
    if (race->tick < RACE_TIME_FROM) return;
    hud_clock_split(&hud->clock, ticks);
    race_time_part(t, dash->yellow, hud->clock.minutes, &hud->minutes_shown, x, y, step, 2);
    x += step * 2 + space;
    race_time_part(t, dash->yellow, hud->clock.seconds, &hud->seconds_shown, x, y, step, 2);
    x += step * 2 + space;
    race_time_part(t, dash->yellow, hud->clock.tenths, &hud->tenths_shown, x, y, step, 1);
    if (race->best_lap_set == 0) return;
    hud->redraw = 1;
    hud_draw_labels(t, hud, dash, race, gap, gap_ticks, high_res, strip_colour);
    race->best_lap_set = 0;
}

int32_t hud_race_stage(const RaceState *race, const Car *car)
{
    if (race->circuit == 0) {
        if (car->node < START_NODES) return 1;
        if (race->loop_nodes < car->node) return 2;
    } else {
        if (car->lap < 1) return 1;
        if (race->laps < car->lap) return 2;
    }
    return 0;
}

/* players[slot + 1] +0: the original keeps a slot's place one record on. */
static void keep_place(RaceState *race, int32_t slot, int32_t place)
{
    if (slot >= 0 && slot + 1 <= RACE_PLAYERS) race->players[slot + 1].unknown_000 = place;  /* port guard */
}

static int32_t finished_place(const RaceState *race)
{
    int32_t place = 1;
    const int32_t mine = race->players[race->viewed].finish;
    for (int32_t i = 0; i < race->racers && i <= RACE_PLAYERS; i++)
        if (race->players[i].finish != 0 && race->players[i].finish < mine) place++;
    return place;
}

void hud_draw_place(const QuadTarget *t, HudState *hud, const DashArt *dash, RaceState *race, int32_t gap, int32_t x,
                    int32_t y, int32_t step)
{
    const Car *car = race->cars[race_slot(race->viewed)];
    int32_t place = hud->place;
    if (car != NULL && hud_race_stage(race, car) == 2) {
        place = finished_place(race);
    } else {
        if (race->race_type == 0) {
            place = gap < 1 ? 2 : 1;
        } else {
            int32_t rival_at = 0;
            const int32_t rival = race->viewed == 0;
            for (int32_t i = 0; i < race->car_count && i < RACE_CARS; i++) {
                const int32_t slot = race->by_progress[i];
                if ((uint32_t)slot < RACE_CARS) race->car_places[slot] = (uint8_t)(race->car_count - i);
                if (slot == race->viewed) place = i;
                else if (slot == rival) rival_at = i;
                keep_place(race, slot, race->car_count - i);
            }
            if (race->race_type == 1) place = place <= rival_at ? 2 : 1;
            else place = race->car_count - place;
        }
        keep_place(race, race->viewed, place);
    }
    hud->place = place;
    if (place != hud->place_shown) {
        hud_draw_number(t, dash->yellow, place, x, y, step, 1);
        hud->place_shown = place;
    }
}

/* 0051cc50 + 4 i: RaceSplits.unknown_cc50, then the players' records word
 * by word (the earlier stages' times). */
static int32_t after_splits(const RaceState *race, int32_t i)
{
    if (i == 0) return race->splits.unknown_cc50;
    const int64_t at = 4 * ((int64_t)i - 1);
    if (i < 0 || at + 4 > (int64_t)sizeof race->players) return 0;  /* port guard */
    int32_t v;
    memcpy(&v, (const uint8_t *)race->players + at, sizeof v);
    return v;
}

static const PlayerRecord *record_of(const RaceState *race, const Car *car)
{
    return (uint32_t)car->player <= RACE_PLAYERS ? &race->players[car->player] : NULL;  /* port guard */
}

/* The time the top row shows (0041e48c's ticks). */
static int32_t top_row_ticks(const HudState *hud, const RaceState *race, const Car *car)
{
    int32_t ticks = race->tick;
    const PlayerRecord *record = record_of(race, car);
    if (race->circuit == 0) {
        if (race->loop_nodes < car->node && record != NULL) ticks = record->finish + HUD_START_TICKS;
    } else if (1 < car->lap && record != NULL && car->lap < RACE_LAP_MARKS) {
        ticks = record->lap_marks[car->lap] + HUD_START_TICKS;
        if (race->tick - ticks < LAST_LAP_SHOWN) {
            if (hud->mode == 0 && 2 < car->lap) ticks -= record->lap_marks[car->lap - 1];
        } else {
            ticks = race->tick;
            if (hud->mode == 0) ticks = race->tick - record->lap_marks[car->lap];
        }
    }
    if (hud->mode == 1)
        for (int32_t i = 0; i < race->track_variant; i++) ticks += after_splits(race, i);
    return ticks;
}

typedef struct {
    int32_t speed[3], gear[2], place[3];        /* x, y, step; gear x, y */
    int32_t time[4], gap[4];                    /* x, y, step, space */
    int32_t distance[4], lap[5];                /* x, y, step, space; x, y, step, laps x, laps y */
} TopRowPlaces;

static const TopRowPlaces TOP_ROW_LOW = {
    { 0x7a, 1, 0xe }, { 0xba, 1 }, { 0x121, 1, 7 }, { 0x48, 1, 8, 3 }, { 0xe0, 1, 8, 3 },
    { 0xd9, 1, 8, 2 }, { 0xde, 1, 8, 0xf5, 1 }
};
static const TopRowPlaces TOP_ROW_HIGH = {
    { 0x106, 2, 0x1b }, { 0x17a, 5 }, { 0x244, 5, 0x13 }, { 0x90, 5, 0x13, 8 }, { 0x1c0, 5, 0x13, 8 },
    { 0x1b8, 5, 0x13, 7 }, { 0x1c0, 5, 0x13, 0x1f2, 5 }
};

void hud_draw_top_row(const QuadTarget *t, HudState *hud, const DashArt *dash, RaceState *race, const Car *car,
                      int16_t german, int32_t high_res, uint32_t strip_colour)
{
    if (hud->waiting != 0) {
        blit_shape_placed(t, dash->wait);
        return;
    }
    const int32_t ticks = top_row_ticks(hud, race, car);
    const int32_t gap = race->splits.gap, gap_ticks = race->splits.gap_shown;
    if (hud->redraw == 0 || race->tick < TOP_ROW_FROM) {
        if (race->tick < LABELS_FROM) {
            hud_draw_start_lights(t, hud, dash, race->start_beeps, race->tick, high_res, strip_colour);
            return;
        }
    } else {
        const TopRowPlaces *p = high_res != 0 ? &TOP_ROW_HIGH : &TOP_ROW_LOW;
        if (hud->mode < 2) {
            hud_draw_speed(t, hud, dash, car, race, german, p->speed[0], p->speed[1], p->speed[2]);
            hud_draw_gear(t, hud, dash, car, race, p->gear[0], p->gear[1]);
            hud_draw_place(t, hud, dash, race, gap, p->place[0], p->place[1], p->place[2]);
        }
        if (hud->mode == 0 || hud->mode == 1) {
            hud_draw_race_time(t, hud, dash, race, gap, gap_ticks, high_res, strip_colour, ticks, p->time[0],
                               p->time[1], p->time[2], p->time[3]);
            if (hud->mode == 0 && gap_ticks < 1) {
                if (race->circuit == 0)
                    hud_draw_distance(t, hud, dash, car, race, german, p->distance[0], p->distance[1], p->distance[2],
                                      p->distance[3]);
                else
                    hud_draw_lap(t, hud, dash, car, race, p->lap[0], p->lap[1], p->lap[2], p->lap[3], p->lap[4]);
            } else {
                hud_draw_gap(t, hud, dash, gap, p->gap[0], p->gap[1], p->gap[2], p->gap[3]);
            }
        } else if (hud->mode == 2) {
            hud_draw_splits(t, hud, dash->yellow, high_res);
        }
        hud->strip_dirty = 1;
    }
    if (hud->lost != 0) blit_shape_placed(t, dash->lost);
}
