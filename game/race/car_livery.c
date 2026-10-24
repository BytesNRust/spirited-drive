#include "game/race/car_livery.h"

#include <stdlib.h>

enum {
    PALETTE_ENTRIES = 256, PALETTE_8BIT = 0x24, PALETTE_6BIT = 0x22,
    CHANNEL_BITS = 5, CHANNEL_MASK = 31, CHANNEL_SHIFT_8 = 3,
    LUMA_R = 30, LUMA_G = 59, LUMA_B = 11   /* .3 r + .59 g + .11 b, in hundredths */
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

int car_livery_map(const uint32_t *normal, const uint32_t *alternate, CarLiveryPair pairs[CAR_LIVERY_PAIRS])
{
    Weighted *from = malloc(sizeof *from * PSX_COLOURS), *to = malloc(sizeof *to * PSX_COLOURS);
    int count = 0;
    if (from != NULL && to != NULL) {
        const size_t nf = body_colours(normal, alternate, from), nt = body_colours(alternate, normal, to);
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
static uint16_t entry_colour(const uint8_t *entry, int type)
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
        const uint16_t colour = entry_colour(entry, type);
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
