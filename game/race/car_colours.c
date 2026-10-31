/* port (car colours): see car_colours.h. */
#include "game/race/car_colours.h"
#include "game/race/car_shine.h"
#include "game/sys/palette.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum {
    CAR_COLOURS = 0xfa,       /* the entries a car's palette maps (race_palette.c) */
    BANK_COUNT = 8, BANK_DIRECTORY = 0x10, BANK_ENTRY = 8, SHAPE_WIDTH = 4, SHAPE_HEIGHT = 6, SHAPE_PIXELS = 0x10,
    MIN_GAIN_X10 = 3,         /* a block changes only for a gain of 0.3 levels of mean distance: the exe's colours were chosen by hand */
    MAX_ITEMS = 3072, MAX_POOL = MAX_ITEMS + 80,
    DARK_SPREAD = 10,         /* a colour is a paint tone when its 6-bit channels spread this far (a grey has no hue to keep in shadow) */
    DARK_WEIGHT_DIV = 6,     /* a darkened tone is wanted a sixth as much as the tone itself: the texels show the tone, the light only sometimes */
    LIGHT_SPREAD = 24,       /* a paint is saturated when its 6-bit channels spread this far: its lift keeps a hue the track's palette has few lighter tones of */
    LIGHT_WEIGHT_DIV = 6,    /* a lightened tone is wanted a sixth as much as the tone itself, as a darkened one */
    LIGHT_DARK_SPREAD = 6,   /* a dark paint (a teal body whose brightest channel is 20 of 63) is a paint tone from this spread when the chroma is most of its ... */
    LIGHT_DARK_CHROMA_PCT = 50,  /* ... brightest channel: a grey with a faint tint is not */
    LIGHT_DARK_WEIGHT_DIV = 2    /* a dark body's lifted tone is wanted half as much: its own tones are the track's few, and at a sixth it lost its slot to the rest */
};

static const float DARK_STEP[] = { 0.78f, 0.60f };   /* the light ratios the PS1 light's dusk and shadow bring (psx_lit_table) */
static const float LIGHT_STEP[] = { 0.40f, 0.60f };  /* the envmap's lifts (level_t) the middle and upper levels of a flat panel's reflection ask for: levels 8 to 11 */

typedef struct {
    uint8_t rgb6[3], rgb8[3];
    uint32_t weight;
} Item;

typedef struct {
    Item item[MAX_ITEMS];
    int count;
} Items;

static uint32_t le32(const uint8_t *b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }
static int32_t le16s(const uint8_t *b) { return (int16_t)(uint16_t)((uint32_t)b[0] | (uint32_t)b[1] << 8); }

/* how many texels of the bank's shapes (not the '!' ones) show each of its 256 colours */
static void histogram(const uint8_t *bank, uint32_t hist[256])
{
    for (uint32_t i = le32(bank + BANK_COUNT); i-- > 0;) {
        const uint8_t *entry = bank + BANK_DIRECTORY + BANK_ENTRY * i;
        if (entry[0] == '!') continue;
        const uint8_t *shape = bank + le32(entry + 4);
        const uint32_t n = (uint32_t)le16s(shape + SHAPE_WIDTH) * (uint32_t)le16s(shape + SHAPE_HEIGHT);
        for (uint32_t k = 0; k < n; k++) hist[shape[SHAPE_PIXELS + k]]++;
    }
}

static void add_item(Items *it, const uint8_t rgb6[3], uint32_t weight)
{
    for (int i = 0; i < it->count; i++)   /* colours a step apart (of 5 bits) are one: the cars share their greys and dark tones */
        if ((it->item[i].rgb6[0] >> 1) == (rgb6[0] >> 1) && (it->item[i].rgb6[1] >> 1) == (rgb6[1] >> 1) &&
            (it->item[i].rgb6[2] >> 1) == (rgb6[2] >> 1)) {
            it->item[i].weight += weight;
            return;
        }
    if (it->count >= MAX_ITEMS) return;
    Item *n = &it->item[it->count++];
    memcpy(n->rgb6, rgb6, 3);
    palette_6_to_8(1, rgb6, n->rgb8);
    n->weight = weight;
}

/* the tone under a darker light: the PS1 light and the shadow take a tone to the nearest colour of its hue, which a palette of the track's
 * colours has few of, so the darker tones are wanted too */
static void add_darkened(Items *it, const uint8_t rgb6[3], uint32_t weight)
{
    const int hi = rgb6[0] > rgb6[1] ? (rgb6[0] > rgb6[2] ? rgb6[0] : rgb6[2]) : (rgb6[1] > rgb6[2] ? rgb6[1] : rgb6[2]);
    const int lo = rgb6[0] < rgb6[1] ? (rgb6[0] < rgb6[2] ? rgb6[0] : rgb6[2]) : (rgb6[1] < rgb6[2] ? rgb6[1] : rgb6[2]);
    if (hi - lo < DARK_SPREAD || weight < DARK_WEIGHT_DIV) return;
    for (size_t s = 0; s < sizeof DARK_STEP / sizeof *DARK_STEP; s++) {
        uint8_t dark[3];
        for (int k = 0; k < 3; k++) dark[k] = (uint8_t)((float)rgb6[k] * DARK_STEP[s] + 0.5f);
        add_item(it, dark, weight / DARK_WEIGHT_DIV);
    }
}

/* the tone under the envmap's lift (body_shine 4): a saturated paint's lift goes toward white with less chroma, and a palette of the track's colours
 * has one pale tone of the hue (a steel blue) that every level from the middle up falls to, so a flat panel's reflection is one flat colour. The
 * lighter tones are wanted too, so the ramp has steps there */
static void add_lightened(Items *it, const uint8_t rgb6[3], uint32_t weight)
{
    const int hi = rgb6[0] > rgb6[1] ? (rgb6[0] > rgb6[2] ? rgb6[0] : rgb6[2]) : (rgb6[1] > rgb6[2] ? rgb6[1] : rgb6[2]);
    const int lo = rgb6[0] < rgb6[1] ? (rgb6[0] < rgb6[2] ? rgb6[0] : rgb6[2]) : (rgb6[1] < rgb6[2] ? rgb6[1] : rgb6[2]);
    const int saturated = hi - lo >= LIGHT_SPREAD, dark_paint = hi - lo >= LIGHT_DARK_SPREAD && (hi - lo) * 100 >= hi * LIGHT_DARK_CHROMA_PCT;
    if (!(saturated || dark_paint) || weight < LIGHT_WEIGHT_DIV) return;
    uint8_t rgb8[3];
    palette_6_to_8(1, rgb6, rgb8);
    for (size_t s = 0; s < sizeof LIGHT_STEP / sizeof *LIGHT_STEP; s++) {
        float want[3];
        uint8_t light[3];
        car_envmap_want(rgb8, LIGHT_STEP[s], want);
        for (int k = 0; k < 3; k++) light[k] = (uint8_t)fminf(want[k] / 4.0f + 0.5f, 63.0f);
        add_item(it, light, weight / (saturated ? LIGHT_WEIGHT_DIV : LIGHT_DARK_WEIGHT_DIV));
    }
}

static void add_bank(Items *it, const uint8_t *bank, const uint8_t *palette6)
{
    if (bank == NULL || palette6 == NULL) return;
    uint32_t hist[256] = { 0 };
    histogram(bank, hist);
    for (int e = 0; e < CAR_COLOURS; e++)
        if (hist[e] != 0) add_item(it, palette6 + 3 * e, hist[e]);
    for (int e = 0; e < CAR_COLOURS; e++)   /* after every real tone: a darkened one never takes the place of a real one in the 5-bit merge */
        if (hist[e] != 0) add_darkened(it, palette6 + 3 * e, hist[e]), add_lightened(it, palette6 + 3 * e, hist[e]);
}

static void add_car(Items *it, const RaceCarArt *a)
{
    add_bank(it, a->bank, a->palette);
    add_bank(it, a->bank2, a->palette2);
}

/* a distance for colours a person sees: green counts most */
static uint32_t distance(const uint8_t a[3], const uint8_t b[3])
{
    const int dr = a[0] - b[0], dg = a[1] - b[1], db = a[2] - b[2];
    return (uint32_t)(2 * dr * dr + 4 * dg * dg + 3 * db * db);
}

static double mean_distance(const Items *it, const uint32_t err[])
{
    double sum = 0, total = 0;
    for (int j = 0; j < it->count; j++) sum += (double)it->item[j].weight * sqrt((double)err[j] / 9.0), total += (double)it->item[j].weight;
    return total > 0 ? sum / total : 0;
}

/* err[j]: the item's squared distance to its nearest of the colours `base` (n of them, 8-bit) */
static void nearest(const Items *it, const uint8_t (*base)[3], int n, uint32_t err[])
{
    for (int j = 0; j < it->count; j++) {
        uint32_t best = UINT32_MAX;
        for (int k = 0; k < n; k++) {
            const uint32_t d = distance(it->item[j].rgb8, base[k]);
            if (d < best) best = d;
        }
        err[j] = best;
    }
}

static void screen_colour(const RacePalette *p, int e, uint8_t out[3]) { memcpy(out, p->screen_rgbx + 4 * e, 3); }

static void screen_colour6(const RacePalette *p, int e, uint8_t out[3])
{
    uint8_t c[3];
    screen_colour(p, e, c);
    for (int k = 0; k < 3; k++) out[k] = (uint8_t)(c[k] >> 2);
}

typedef struct {
    uint8_t rgb6[3], rgb8[3];
    int old;   /* the block's entry now at this position, or -1 */
} Pool;

/* the block's new colours: `count` picks of the pool, greedy by the weighted distance they take off the items */
static int pick(const Items *it, const Pool *pool, int pool_n, const uint8_t (*base)[3], int base_n, int count, uint8_t chosen[])
{
    uint32_t *err = malloc(sizeof(uint32_t) * (size_t)(it->count + 1));
    if (err == NULL) return 0;
    nearest(it, base, base_n, err);
    memset(chosen, 0, (size_t)pool_n);
    int got = 0;
    while (got < count) {
        int best = -1;
        uint64_t best_gain = 0;
        for (int c = 0; c < pool_n; c++) {
            if (chosen[c]) continue;
            uint64_t gain = 0;
            for (int j = 0; j < it->count; j++) {
                if (err[j] == 0) continue;
                const uint32_t d = distance(it->item[j].rgb8, pool[c].rgb8);
                if (d < err[j]) gain += (uint64_t)(err[j] - d) * it->item[j].weight;
            }
            if (gain > best_gain) best_gain = gain, best = c;
        }
        if (best < 0) break;
        chosen[best] = 1, got++;
        for (int j = 0; j < it->count; j++) {
            const uint32_t d = distance(it->item[j].rgb8, pool[best].rgb8);
            if (d < err[j]) err[j] = d;
        }
    }
    free(err);
    return got;
}

static int fit_block(RacePalette *p, const RaceState *race, const RacePaletteBlock *b, const Items *it, const uint8_t keep[256], CarFitBlock *out)
{
    out->first = b->first, out->count = b->count, out->car = b->car, out->colours = it->count, out->changed = 0;
    uint8_t usable[RACE_PALETTE_COLOURS];
    race_palette_enabled_mask(p, race, usable);
    uint8_t (*base)[3] = malloc(sizeof(uint8_t[3]) * (RACE_PALETTE_COLOURS + MAX_POOL));
    Pool *pool = malloc(sizeof *pool * MAX_POOL);
    uint8_t *chosen = malloc(MAX_POOL);
    uint32_t *err = malloc(sizeof(uint32_t) * (size_t)(it->count + 1));
    int changed = 0;
    if (base == NULL || pool == NULL || chosen == NULL || err == NULL) goto done;
    int base_n = 0, places = 0;
    uint8_t held[RACE_PALETTE_COLOURS] = { 0 };   /* the block's entries that stay: the ones the scenery is mapped to */
    for (int k = 0; k < b->count; k++) held[k] = keep != NULL && keep[b->first + k] != 0, places += !held[k];
    for (int e = 0; e < CAR_COLOURS; e++)
        if (usable[e] && (e < b->first || e >= b->first + b->count || held[e - b->first])) screen_colour(p, e, base[base_n++]);
    int pool_n = 0;
    uint8_t old6[RACE_PALETTE_COLOURS][3];
    for (int k = 0; k < b->count; k++) {   /* the block's colours as they are: kept where they do as well */
        screen_colour6(p, b->first + k, old6[k]);
        if (held[k]) continue;
        Pool *c = &pool[pool_n++];
        memcpy(c->rgb6, old6[k], 3);
        palette_6_to_8(1, c->rgb6, c->rgb8);
        c->old = k;
    }
    const int old_n = pool_n;
    for (int j = 0; j < it->count && pool_n < MAX_POOL; j++) {
        int dup = 0;
        for (int k = 0; k < old_n; k++) dup |= memcmp(pool[k].rgb6, it->item[j].rgb6, 3) == 0;
        if (dup) continue;
        Pool *c = &pool[pool_n++];
        memcpy(c->rgb6, it->item[j].rgb6, 3), memcpy(c->rgb8, it->item[j].rgb8, 3);
        c->old = -1;
    }
    /* what the block's own colours reach now */
    uint8_t (*with_old)[3] = base;
    int with_n = base_n;
    for (int k = 0; k < old_n; k++) memcpy(with_old[with_n++], pool[k].rgb8, 3);
    nearest(it, with_old, with_n, err);
    out->before = mean_distance(it, err);
    const int got = places > 0 ? pick(it, pool, pool_n, base, base_n, places, chosen) : 0;
    if (got == 0) { out->after = out->before; goto done; }
    /* the new set: the old entries that were picked stay where they are, the new ones take the places of those that were not */
    uint8_t layout[RACE_PALETTE_COLOURS][3];
    int taken[RACE_PALETTE_COLOURS] = { 0 }, placed = 0;
    for (int k = 0; k < b->count; k++) taken[k] = held[k];
    for (int c = 0; c < old_n; c++)
        if (chosen[c]) memcpy(layout[pool[c].old], pool[c].rgb6, 3), taken[pool[c].old] = 1, placed++;
    int next = 0;
    for (int c = old_n; c < pool_n; c++) {
        if (!chosen[c]) continue;
        while (next < b->count && taken[next]) next++;
        if (next >= b->count) break;
        memcpy(layout[next], pool[c].rgb6, 3), taken[next] = 1, placed++;
    }
    for (int k = 0; k < b->count; k++)   /* a place left over (the picks ran out) or held keeps its colour */
        if (!taken[k] || held[k]) memcpy(layout[k], old6[k], 3);
    int n8 = base_n;
    for (int k = 0; k < b->count; k++)
        if (!held[k]) palette_6_to_8(1, layout[k], base[n8++]);
    nearest(it, base, n8, err);
    const double after = mean_distance(it, err);
    if (after > out->before - (double)MIN_GAIN_X10 / 10.0) { out->after = out->before; goto done; }   /* not better: the exe's */
    out->after = after;
    uint8_t flat[RACE_PALETTE_COLOURS * 3], is_new[RACE_PALETTE_COLOURS];
    for (int k = 0; k < b->count; k++) {
        memcpy(flat + 3 * k, layout[k], 3);
        is_new[k] = memcmp(layout[k], old6[k], 3) != 0;
        changed += is_new[k];
    }
    if (changed != 0) {
        if (b->tinted) {   /* the shared palette is tinted by the time of day: only the colours that are new, as the exe tints it */
            uint8_t t[RACE_PALETTE_COLOURS * 3];
            memcpy(t, flat, (size_t)b->count * 3);
            race_palette_tint(p, 0, b->count, t);
            for (int k = 0; k < b->count; k++)
                if (is_new[k]) memcpy(flat + 3 * k, t + 3 * k, 3);
        }
        race_palette_write(p, b->first, b->count, flat);
    }
    out->changed = changed;
done:
    free(base), free(pool), free(chosen), free(err);
    return changed;
}

int car_colours_fit(RacePalette *p, const RaceState *race, const RaceCarArt art[RACE_CARS], int have_carpal, const uint8_t keep[256],
                    CarFitReport *report)
{
    RacePaletteBlock blocks[RACE_PALETTE_BLOCKS];
    const int n = race_palette_blocks(race, have_carpal, blocks);
    Items *it = malloc(sizeof *it);
    if (it == NULL) return 0;
    int total = 0;
    if (report != NULL) report->count = 0;
    for (int i = 0; i < n; i++) {
        memset(it, 0, sizeof *it);
        if (blocks[i].car >= 0) {
            if ((uint32_t)blocks[i].car < RACE_CARS) add_car(it, &art[blocks[i].car]);
        } else {
            for (int c = 0; c < race->car_count && c < RACE_CARS; c++) add_car(it, &art[c]);
        }
        if (it->count == 0) continue;
        CarFitBlock fit;
        memset(&fit, 0, sizeof fit);
        total += fit_block(p, race, &blocks[i], it, keep, &fit);
        if (report != NULL && report->count < RACE_PALETTE_BLOCKS) report->block[report->count++] = fit;
    }
    free(it);
    return total;
}
