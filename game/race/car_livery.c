#include "game/race/car_livery.h"

#include <stdlib.h>

enum {
    PALETTE_ENTRIES = 256, PALETTE_8BIT = 0x24, PALETTE_6BIT = 0x22,
    CHANNEL_BITS = 5, CHANNEL_MASK = 31, CHANNEL_SHIFT_8 = 3,
    LUMA_R = 30, LUMA_G = 59, LUMA_B = 11,  /* .3 r + .59 g + .11 b, in hundredths */
    SATURATED_SPREAD = 32, SATURATED_SHARE = 16
};
/* the hash's multipliers: past int, so not enumerators */
static const uint32_t SALT_SLOT = 0x85ebca6bu, SALT_SEED = 0x9e3779b1u, SALT_FINAL = 0xc2b2ae35u;

/* A colour's luminance for the ranking, from its 8-bit channels. */
static uint32_t luma(uint16_t c)
{
    const uint32_t r = (uint32_t)(c & CHANNEL_MASK) << CHANNEL_SHIFT_8;
    const uint32_t g = (uint32_t)((c >> CHANNEL_BITS) & CHANNEL_MASK) << CHANNEL_SHIFT_8;
    const uint32_t b = (uint32_t)((c >> (2 * CHANNEL_BITS)) & CHANNEL_MASK) << CHANNEL_SHIFT_8;
    return LUMA_R * r + LUMA_G * g + LUMA_B * b;
}

/* One colour with its pixel count, as the ranking sorts it. */
typedef struct {
    uint16_t colour;
    uint64_t weight;
} Weighted;

/* Luminance order, the colour breaking a tie, so the order is the same on every run. */
static int by_luma(const void *pa, const void *pb)
{
    const Weighted *a = pa, *b = pb;
    const uint32_t la = luma(a->colour), lb = luma(b->colour);
    if (la != lb) return la < lb ? -1 : 1;
    return (int)a->colour - (int)b->colour;
}

/* The colours one livery uses and the other does not, weighted by their pixels, in luminance order. Returns how
 * many; `out` holds PSX_COLOURS entries. */
static size_t body_colours(const uint32_t *own, const uint32_t *other, Weighted *out)
{
    size_t n = 0;
    for (uint32_t c = 0; c < PSX_COLOURS; c++) {
        if (own[c] == 0 || other[c] != 0) continue;
        out[n].colour = (uint16_t)c, out[n].weight = own[c];
        n++;
    }
    qsort(out, n, sizeof *out, by_luma);
    return n;
}

/* Adds the forced colours the count left out, each at the mean weight of the body colours, and sorts the list again. */
static size_t add_forced(Weighted *from, size_t nf, const uint16_t *forced, int forced_count)
{
    uint64_t total = 0;
    for (size_t i = 0; i < nf; i++) total += from[i].weight;
    const uint64_t mean = nf > 0 ? total / nf : 1;
    for (int k = 0; k < forced_count; k++) {
        size_t i = 0;
        while (i < nf && from[i].colour != forced[k]) i++;
        if (i < nf) continue;
        from[nf].colour = forced[k], from[nf].weight = mean > 0 ? mean : 1;
        nf++;
    }
    qsort(from, nf, sizeof *from, by_luma);
    return nf;
}

int car_livery_map(const uint32_t *normal, const uint32_t *alternate, const uint16_t *forced, int forced_count,
                   CarLiveryPair pairs[CAR_LIVERY_PAIRS])
{
    Weighted *from = malloc(sizeof *from * (PSX_COLOURS + CAR_LIVERY_PAIRS)), *to = malloc(sizeof *to * PSX_COLOURS);
    int count = 0;
    if (from != NULL && to != NULL) {
        size_t nf = body_colours(normal, alternate, from);
        const size_t nt = body_colours(alternate, normal, to);
        if (forced_count > 0 && nf <= CAR_LIVERY_PAIRS) nf = add_forced(from, nf, forced, forced_count);
        if (nf > 0 && nt > 0 && nf <= CAR_LIVERY_PAIRS) {
            uint64_t total_from = 0, total_to = 0;
            for (size_t i = 0; i < nf; i++) total_from += from[i].weight;
            for (size_t i = 0; i < nt; i++) total_to += to[i].weight;
            /* each normal colour's midpoint on the alternate's cumulative weight: the first alternate colour whose
             * running total reaches it. Doubled, so the midpoint is whole: 2 * cum_to / total_to >= mid2 / total_from
             * is written as 2 * cum_to * total_from >= mid2 * total_to. The midpoints only grow, so k only grows. */
            uint64_t before = 0, cumulative = 0;
            size_t k = 0;
            for (size_t i = 0; i < nf; i++) {
                const uint64_t mid2 = 2 * before + from[i].weight;
                while (k + 1 < nt && 2 * (cumulative + to[k].weight) * total_from < mid2 * total_to) {
                    cumulative += to[k].weight;
                    k++;
                }
                pairs[count].from = from[i].colour, pairs[count].to = to[k].colour;
                count++;
                before += from[i].weight;
            }
        }
    }
    free(from);
    free(to);
    return count;
}

/* The 5-bit colour of a palette entry of this type. A 6-bit entry is 8-bit's >> 2, so its 5 bits are its bits 1..5. */
uint16_t car_livery_colour(const uint8_t *entry, int type)
{
    const int shift = type == PALETTE_6BIT ? 1 : CHANNEL_SHIFT_8;
    const uint16_t r = (uint16_t)(entry[0] >> shift);
    const uint16_t g = (uint16_t)(entry[1] >> shift);
    const uint16_t b = (uint16_t)(entry[2] >> shift);
    return (uint16_t)(r | g << CHANNEL_BITS | b << (2 * CHANNEL_BITS));
}

void car_livery_remap(uint8_t *palette, int type, const CarLiveryPair *pairs, int count)
{
    if (type != PALETTE_8BIT && type != PALETTE_6BIT) return;
    for (int i = 0; i < PALETTE_ENTRIES; i++) {
        uint8_t *entry = palette + 3 * i;
        const uint16_t colour = car_livery_colour(entry, type);
        for (int k = 0; k < count; k++) {
            if (pairs[k].from != colour) continue;
            const uint16_t to = pairs[k].to;
            const uint8_t channels[3] = { (uint8_t)((to & CHANNEL_MASK) << CHANNEL_SHIFT_8),
                                          (uint8_t)(((to >> CHANNEL_BITS) & CHANNEL_MASK) << CHANNEL_SHIFT_8),
                                          (uint8_t)(((to >> (2 * CHANNEL_BITS)) & CHANNEL_MASK) << CHANNEL_SHIFT_8) };
            for (int c = 0; c < 3; c++) entry[c] = type == PALETTE_6BIT ? channels[c] >> 2 : channels[c];
            break;
        }
    }
}

/* A small integer hash (xorshift and multiply) of the seed and the slot. Its top bit is taken: the low bits of a
 * multiply only depend on the low bits of its input, so they would not vary with the seed. */
static uint32_t mix(uint32_t seed, int slot)
{
    uint32_t h = seed * SALT_SEED ^ (uint32_t)(slot + 1) * SALT_SLOT;
    h ^= h >> 15;
    h *= SALT_SLOT;
    h ^= h >> 13;
    h *= SALT_FINAL;
    h ^= h >> 16;
    return h;
}

int car_livery_pick(uint32_t seed, int slot, int car, int player_car, int player_livery)
{
    if (car == player_car) return player_livery ? 0 : 1;
    return (int)(mix(seed, slot) >> 31);
}

/* A colour that reads as paint and not as shading: its 8-bit channels spread by SATURATED_SPREAD or more. */
static int saturated(uint16_t c)
{
    const int r = c & CHANNEL_MASK, g = (c >> CHANNEL_BITS) & CHANNEL_MASK, b = (c >> (2 * CHANNEL_BITS)) & CHANNEL_MASK;
    const int hi = r > g ? (r > b ? r : b) : (g > b ? g : b), lo = r < g ? (r < b ? r : b) : (g < b ? g : b);
    return ((hi - lo) << CHANNEL_SHIFT_8) >= SATURATED_SPREAD;
}

/* The colour the swatch shows: the heaviest of the pairs' colours (`from` or `to`) in `counts`, but a saturated one
 * wins over a grey one when it weighs at least 1/SATURATED_SHARE of that heaviest: the textures' dark shading and
 * interior outweigh the paint, which is the colour people see. 0 when none is used. */
static int main_colour(const CarLiveryPair *pairs, int count, const uint32_t *counts, int to, uint16_t *colour)
{
    uint32_t most = 0, most_saturated = 0;
    uint16_t heaviest = 0, paint = 0;
    for (int i = 0; i < count; i++) {
        const uint16_t c = to ? pairs[i].to : pairs[i].from;
        const uint32_t n = counts[c];
        if (n > most) most = n, heaviest = c;
        if (n > most_saturated && saturated(c)) most_saturated = n, paint = c;
    }
    if (most == 0) return 0;
    *colour = most_saturated != 0 && most_saturated * SATURATED_SHARE >= most ? paint : heaviest;
    return 1;
}

int car_livery_main(const CarLiveryPair *pairs, int count, const uint32_t *alternate, uint16_t *colour)
{
    return main_colour(pairs, count, alternate, 1, colour);
}

int car_livery_main_from(const CarLiveryPair *pairs, int count, const uint32_t *normal, uint16_t *colour)
{
    return main_colour(pairs, count, normal, 0, colour);
}
