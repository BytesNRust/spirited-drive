/* The in-car dashboard's load (0041f9a0). */
#include "game/race/cockpit.h"

#include "game/sys/shape_bank.h"

#include <stdio.h>
#include <string.h>

enum {
    PAL_HEADER = 0x10,
    PAL_BYTES = 0x300,
    TYPE_24_BIT = 0x24,
    TYPE_POINTS = 0x7c,
    HIGH_FRAMES = 6,
    TRACKS = 8,
    RECORD_STRIDE = 0xa6b,
    FMM_WORDS_AT = 8,
    SPED_MIDDLE = 12
};

/* 004b9154 */
static const int32_t track_words[TRACKS] = { 480, 480, 480, 256, 480, 480, 480, 512 };

/* 004b92e8.. (high, six of each) and 004b9378.. (low, seven): 'w'l, 'w'r, 't'l, 's'l, 't'r, 's'r */
static const char *const high_names[6][HIGH_FRAMES] = {
    { "wstr", "wl06", "wl14", "wl22", "wl32", "wl45" }, { "wstr", "wr06", "wr14", "wr22", "wr32", "wr45" },
    { "tstr", "tl06", "tl14", "tl22", "tl32", "tl45" }, { "sstr", "sl06", "sl14", "sl22", "sl32", "sl45" },
    { "tstr", "tr06", "tr14", "tr22", "tr32", "tr45" }, { "sstr", "sr06", "sr14", "sr22", "sr32", "sr45" }
};
static const char *const low_names[6][COCKPIT_FRAMES] = {
    { "wstr", "wl06", "wl12", "wl18", "wl26", "wl34", "wl45" }, { "wstr", "wr06", "wr12", "wr18", "wr26", "wr34", "wr45" },
    { "tstr", "tl06", "tl12", "tl18", "tl26", "tl34", "tl45" }, { "sstr", "sl06", "sl12", "sl18", "sl26", "sl34", "sl45" },
    { "tstr", "tr06", "tr12", "tr18", "tr26", "tr34", "tr45" }, { "sstr", "sr06", "sr12", "sr18", "sr26", "sr34", "sr45" }
};
static const char *const digit_names[COCKPIT_DIGITS] = {
    "num0", "num1", "num2", "num3", "num4", "num5", "num6", "num7", "num8", "num9", "numb"
};
static const char *const light_names[COCKPIT_LIGHTS] = { "nob1", "lth3", "lth2", "lth1" };

static int32_t le32(const uint8_t *b)
{
    return (int32_t)((uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24);
}

static int32_t s16(const uint8_t *b)
{
    return (int16_t)(uint16_t)(b[0] | b[1] << 8);
}

static uint8_t *find(uint8_t *bank, const char *name)
{
    return shape_bank_find(bank, (const uint8_t *)name);
}

int cockpit_paths(CockpitFiles *files, const char *dir, const char *name, int32_t high)
{
    const int a = snprintf(files->bank, sizeof files->bank, "%s%s%s", dir, name, high ? "DH.fsh" : "DL.fsh");
    const int b = snprintf(files->fmm, sizeof files->fmm, "%s%s%s", dir, name, high ? "DH.fmm" : "DL.fmm");
    return a < 0 || a >= (int)sizeof files->bank || b < 0 || b >= (int)sizeof files->fmm ? -1 : 0;
}

static int find_palette(Cockpit *c, uint8_t *bank)
{
    uint8_t *pal = find(bank, "!pal");
    if (pal == NULL) {
        uint8_t *dash = shape_bank_find(bank, (const uint8_t *)"dash");
        if (dash == NULL) return -1;  /* 004732dd aborts */
        c->palette = dash + (le32(dash) >> 8) + PAL_HEADER;
        return 0;
    }
    c->palette = pal + PAL_HEADER;
    if ((le32(pal) & 0xff) == TYPE_24_BIT)
        for (int i = 0; i < PAL_BYTES; i++) c->palette[i] = (uint8_t)(c->palette[i] >> 2);
    return 0;
}

/* The steering frames. High: a missing 'w' frame takes its side's last found
 * (none found: a hang), the 't' / 's' ones that side's last too, maybe NULL. */
static int find_frames(Cockpit *c, uint8_t *bank, int32_t high)
{
    c->last_left = c->last_right = 0;
    if (!high) {
        for (int k = 0; k < COCKPIT_FRAMES; k++)
            for (int g = 0; g < 6; g++)
                if ((c->frames[g][k] = find(bank, low_names[g][k])) == NULL) return -1;
        return 0;
    }
    for (int k = 0; k < HIGH_FRAMES; k++) {
        int32_t *last[6] = { &c->last_left, &c->last_right, &c->last_left, &c->last_left, &c->last_right,
                             &c->last_right };
        for (int g = 0; g < 6; g++) {
            c->frames[g][k] = find(bank, high_names[g][k]);
            if (c->frames[g][k] != NULL) {
                if (g < 2) *last[g] = k;
                continue;
            }
            c->frames[g][k] = find(bank, high_names[g][*last[g]]);
            if (c->frames[g][k] == NULL && g < 2) return -1;
        }
    }
    return 0;
}

static int find_shapes(Cockpit *c, uint8_t *bank, int32_t high, int32_t model)
{
    uint8_t **needed[] = { &c->dash, &c->tach, &c->sped, &c->small_lit, &c->small_dark, &c->big_lit, &c->big_dark };
    const char *const names[] = { "dash", "tach", "sped", "smlt", "smdk", "bglt", "bgdk" };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++)
        if ((*needed[i] = find(bank, names[i])) == NULL) return -1;
    if (find_frames(c, bank, high) != 0) return -1;
    if ((c->gate = find(bank, "gate")) == NULL || (c->gate_back = find(bank, "gbak")) == NULL) return -1;
    for (int i = 0; i < COCKPIT_LIGHTS; i++) {
        c->light_count = i + 1;
        if ((c->lights[i] = find(bank, light_names[i])) == NULL) break;
    }
    c->digits_on = model == 3 || model == 0xb;
    if (c->digits_on)
        for (int i = 0; i < COCKPIT_DIGITS; i++)
            if ((c->digits[i] = find(bank, digit_names[i])) == NULL) return -1;  /* 004732dd aborts */
    return 0;
}

/* The shape's first 0x7c record, by each record's size (the word at +0 >> 8). */
static const uint8_t *points(const uint8_t *shape, const uint8_t *bank, size_t bank_size)
{
    const uint8_t *end = bank + bank_size;
    while (shape >= bank && shape + 4 <= end) {
        if ((le32(shape) & 0xff) == TYPE_POINTS) return shape;
        const int32_t step = le32(shape) >> 8;
        if (step <= 0) return NULL;  /* the original loops for ever */
        shape += step;
    }
    return NULL;
}

/* x = the record's word + the shape's hotspot x (+8) + place x (+0xc); y likewise (+0xa, +0xe). */
static CockpitPoint point_at(const uint8_t *rec, int k, const uint8_t *shape)
{
    return (CockpitPoint){ le32(rec + 8 + 8 * k) + s16(shape + 8) + s16(shape + 0xc),
                           le32(rec + 0xc + 8 * k) + s16(shape + 0xa) + s16(shape + 0xe) };
}

static int read_points(Cockpit *c, const uint8_t *bank, size_t bank_size)
{
    const uint8_t *rec = points(c->dash, bank, bank_size);
    if (rec == NULL || rec + 8 + 8 * COCKPIT_DASH_POINTS > bank + bank_size) return -1;
    for (int k = 0; k < COCKPIT_DASH_POINTS; k++) c->dash_points[k] = point_at(rec, k, c->dash);
    c->dash_points[8].x -= c->dash_points[2].x, c->dash_points[8].y -= c->dash_points[2].y;
    c->dash_points[9].x -= c->dash_points[3].x, c->dash_points[9].y -= c->dash_points[3].y;
    const struct { const uint8_t *shape; int32_t *count; CockpitPoint *out; } gauges[] = {
        { c->tach, &c->tach_count, c->tach_points }, { c->sped, &c->sped_count, c->sped_points }
    };
    for (int g = 0; g < 2; g++) {
        rec = points(gauges[g].shape, bank, bank_size);
        if (rec == NULL) return -1;
        *gauges[g].count = rec[4];
        if (*gauges[g].count > COCKPIT_GAUGE_POINTS || rec + 8 + 8 * *gauges[g].count > bank + bank_size) return -1;
        for (int k = 0; k < *gauges[g].count; k++) gauges[g].out[k] = point_at(rec, k, gauges[g].shape);
    }
    c->sped_points[SPED_MIDDLE].x = (c->sped_points[SPED_MIDDLE - 1].x + c->sped_points[SPED_MIDDLE + 1].x) / 2;
    c->sped_points[SPED_MIDDLE].y = (c->sped_points[SPED_MIDDLE - 1].y + c->sped_points[SPED_MIDDLE + 1].y) / 2;
    rec = points(c->gate, bank, bank_size);
    if (rec == NULL || rec + 8 + 8 * COCKPIT_GATE_POINTS > bank + bank_size) return -1;
    for (int k = 0; k < COCKPIT_GATE_POINTS; k++) c->gate_points[k] = point_at(rec, k, c->gate);
    return 0;
}

static void gauge_boxes(Cockpit *c)
{
    c->tach_x = s16(c->tach + 0xc), c->tach_y = s16(c->tach + 0xe);
    c->tach_w = s16(c->tach + 4), c->tach_h = s16(c->tach + 6);
    c->sped_x = s16(c->sped + 0xc), c->sped_y = s16(c->sped + 0xe);
    c->sped_w = s16(c->sped + 4), c->sped_h = s16(c->sped + 6);
}

static void start(CockpitWorld *w, HudState *hud, RaceState *race, int32_t *dash_camera, int32_t full)
{
    race->unknown_4b91c0 = 0;
    w->unknown_4b91c4 = 0;
    memset(w->unknown_4b92c4, 0, sizeof w->unknown_4b92c4);
    race->unknown_4b92d4 = 0;
    w->unknown_4f9b48 = 0;
    hud->mode = 0;
    hud->lost = 0;
    hud->redraw = 1;
    hud->strip_dirty = 1;
    memset(hud->split_shown, 0, sizeof hud->split_shown);
    race->camera_pitch = 0;
    race->landing_shake = 0;
    if (full) {
        *dash_camera = 0;
        race->chase_kinds[0].camera = 0;
        memset(hud->split, 0, sizeof hud->split);
        return;
    }
    race->chase_kinds[0].camera = *dash_camera != 0 ? 1 : 0;
}

int cockpit_load(Cockpit *c, CockpitWorld *w, HudState *hud, RaceState *race, int32_t *dash_camera, uint8_t *bank,
                 size_t bank_size, const uint8_t *fmm, const char *name, int32_t high, int32_t full)
{
    const int32_t track = race->track;
    if (track < 0 || track >= TRACKS) return -1;
    race->best_lap = race->records != NULL ? le32(race->records + RECORD_STRIDE * track) : 0;
    w->unknown_4f9b40 = track_words[track];
    race->best_lap_set = 0;
    snprintf(c->name, sizeof c->name, "%s", name);
    w->width = high ? 640 : 320;
    w->height = high ? 480 : 200;
    c->bank = bank;
    const Car *car = race->cars[race_slot(race->player)];
    if (car == NULL || find_palette(c, bank) != 0 || find_shapes(c, bank, high, car->model) != 0) return -1;
    if (full) {
        gauge_boxes(c);
        if (read_points(c, bank, bank_size) != 0 || fmm == NULL) return -1;
        c->fmm = fmm;
        for (int i = 0; i < COCKPIT_FMM_WORDS; i++) c->fmm_words[i] = le32(fmm + FMM_WORDS_AT + 4 * i);
    }
    start(w, hud, race, dash_camera, full);
    return 0;
}
