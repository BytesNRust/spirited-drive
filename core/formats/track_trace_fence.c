/* The fence step from the road keys: see track_trace_fence.h and track_trace.h. */
#include "formats/track_trace_fence.h"

#include "formats/track_template.h"

#include <stdlib.h>
#include <string.h>

static int is_fence_bit(unsigned bit)
{
    return bit == TRACE_KEY_FENCE_HEIGHT || bit == TRACE_KEY_FENCE_ART || bit == TRACE_KEY_BARRIER_CUSTOM(0) || bit == TRACE_KEY_BARRIER_CUSTOM(1);
}

static int value_ok(unsigned bit, int v)
{
    return bit == TRACE_KEY_FENCE_ART ? v >= 0 && v < TRACE_FENCE_PICTURES : v == 0 || v == 1;
}

static int value_of(const TraceKey *k, unsigned bit)
{
    if (bit == TRACE_KEY_FENCE_HEIGHT) return k->fence_tall;
    if (bit == TRACE_KEY_FENCE_ART) return k->fence_art;
    return k->barrier_custom[bit == TRACE_KEY_BARRIER_CUSTOM(TRACK_GEN_RIGHT)];
}

/* the refusal of a value out of range, naming the point */
static int fault(ExchangeError *err, int point, unsigned bit, int v)
{
    if (bit == TRACE_KEY_FENCE_HEIGHT) return exchange_fail(err, "the road key on point %d has a fence height of %d: low (0) or tall (1)", point, v), 0;
    if (bit == TRACE_KEY_FENCE_ART) return exchange_fail(err, "the road key on point %d has a fence picture of %d: planks (0), rail (1) or parapet (2)", point, v), 0;
    const char *side = bit == TRACE_KEY_BARRIER_CUSTOM(TRACK_GEN_LEFT) ? "left" : "right";
    return exchange_fail(err, "the road key on point %d has a %s barrier custom of %d: on (1) or off (0)", point, side, v), 0;
}

int trace_fence_check(const TraceKey *keys, int key_count, ExchangeError *err)
{
    static const unsigned BITS[] = { TRACE_KEY_FENCE_HEIGHT, TRACE_KEY_FENCE_ART, TRACE_KEY_BARRIER_CUSTOM(0), TRACE_KEY_BARRIER_CUSTOM(1) };
    for (int k = 0; k < key_count; ++k)
        for (size_t b = 0; b < sizeof BITS / sizeof BITS[0]; ++b)
            if ((keys[k].has & BITS[b]) && !value_ok(BITS[b], value_of(&keys[k], BITS[b]))) return fault(err, keys[k].point, BITS[b], value_of(&keys[k], BITS[b]));
    return 1;
}

/* the value of `bit` the keys hold at arc length `at` (the last key at or before it that sets it); 0 when none does yet */
static int held_at(const double *point_s, const TraceKey *keys, int key_count, unsigned bit, double at, int *v)
{
    double best = -1;
    int found = 0;
    for (int k = 0; k < key_count; ++k) {
        const double s = point_s[keys[k].point];
        if (!(keys[k].has & bit) || s > at + 1e-9 || s < best) continue;
        best = s, *v = value_of(&keys[k], bit), found = 1;
    }
    return found;
}

/* where a side's barrier stands when it follows what is drawn: the nearer to the road of what is there. Today that is the fence, which stands where the
 * shoulder ends, or with no fence the shoulder's end itself; the terrain wall (a later step) joins here. In the verge byte's 1/8 m past the road edge;
 * 0 when it is the template's (the shoulder the template's, 3 m: the template's barrier is there too) */
static int barrier_line(const TrackGenSide *side, double *eighths)
{
    if (!(side->has & TRACK_GEN_SIDE_SHOULDER)) return 0;
    *eighths = side->shoulder;
    return 1;
}

int trace_fence_apply(const double *point_s, const TraceKey *keys, int key_count, double step, TrackGenNode *nodes, int node_count)
{
    for (int k = 0; k < node_count; ++k) {
        const double at = k * step;
        int v;
        TrackGenFence *f = &nodes[k].fence;
        if (held_at(point_s, keys, key_count, TRACE_KEY_FENCE_HEIGHT, at, &v)) f->has |= TRACK_GEN_FENCE_HEIGHT, f->tall = v;
        if (held_at(point_s, keys, key_count, TRACE_KEY_FENCE_ART, at, &v)) f->has |= TRACK_GEN_FENCE_SLOT, f->slot = v * TRACK_GENERIC_FENCE_PICTURE_STEP;
        for (int s = 0; s < 2; ++s) {
            int custom = 0;
            held_at(point_s, keys, key_count, TRACE_KEY_BARRIER_CUSTOM(s), at, &custom);
            if (custom) continue;   /* the key's Barrier from edge, as trace_sides_apply set it */
            TrackGenSide *side = &nodes[k].side[s];
            double line;
            if (barrier_line(side, &line)) side->has |= TRACK_GEN_SIDE_BARRIER, side->barrier = line;
            else side->has &= ~(unsigned)TRACK_GEN_SIDE_BARRIER, side->barrier = 0;
        }
    }
    return 1;
}

void track_trace_point_fence(const TraceKey *keys, int key_count, int point_count, TraceFencePoint *out)
{
    TraceFencePoint now = { 0, TRACE_FENCE_PLANKS, { 0, 0 } };
    int k = 0;
    for (int i = 0; i < point_count; ++i) {
        while (k < key_count && keys[k].point < i) ++k;
        if (k < key_count && keys[k].point == i && trace_fence_check(&keys[k], 1, &(ExchangeError){ { 0 } })) {
            const TraceKey *key = &keys[k];
            if (key->has & TRACE_KEY_FENCE_HEIGHT) now.tall = key->fence_tall;
            if (key->has & TRACE_KEY_FENCE_ART) now.art = key->fence_art;
            for (int s = 0; s < 2; ++s)
                if (key->has & TRACE_KEY_BARRIER_CUSTOM(s)) now.custom[s] = key->barrier_custom[s];
        }
        out[i] = now;
    }
}

/* ---- the key list's edits ---- */

int track_trace_key_set_fence(TraceKey **keys, int *count, int *cap, int point, unsigned bit, int value)
{
    if (!is_fence_bit(bit) || !value_ok(bit, value)) return 0;
    int at = track_trace_key_find(*keys, *count, point);
    if (at < 0) {
        if (*count == *cap) {
            const int grown = *cap == 0 ? 8 : *cap * 2;
            TraceKey *more = realloc(*keys, sizeof *more * (size_t)grown);
            if (more == NULL) return 0;
            *keys = more;
            *cap = grown;
        }
        at = *count;
        while (at > 0 && (*keys)[at - 1].point > point) {
            (*keys)[at] = (*keys)[at - 1];
            --at;
        }
        memset(&(*keys)[at], 0, sizeof **keys);
        (*keys)[at].point = point;
        ++*count;
    }
    TraceKey *k = &(*keys)[at];
    k->has |= bit;
    if (bit == TRACE_KEY_FENCE_HEIGHT) k->fence_tall = value;
    else if (bit == TRACE_KEY_FENCE_ART) k->fence_art = value;
    else k->barrier_custom[bit == TRACE_KEY_BARRIER_CUSTOM(TRACK_GEN_RIGHT)] = value;
    return 1;
}

void track_trace_key_clear_fence(TraceKey *keys, int *count, int point, unsigned bit)
{
    const int at = track_trace_key_find(keys, *count, point);
    if (at < 0 || !is_fence_bit(bit)) return;
    TraceKey *k = &keys[at];
    k->has &= ~bit;
    if (bit == TRACE_KEY_FENCE_HEIGHT) k->fence_tall = 0;
    else if (bit == TRACE_KEY_FENCE_ART) k->fence_art = 0;
    else k->barrier_custom[bit == TRACE_KEY_BARRIER_CUSTOM(TRACK_GEN_RIGHT)] = 0;
    if (k->has != 0) return;
    memmove(&keys[at], &keys[at + 1], sizeof *keys * (size_t)(*count - at - 1));
    --*count;
}
