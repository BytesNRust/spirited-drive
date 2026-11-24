/* See light_track_copy.h. */
#include "light_track_copy.h"
#include "light_strip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef union {
    LightTrackBrightness b;
    LightTrackTone t;
    LightTrackLamp l;
} Any;

static const char *const TIME_NAME[LT_TIMES] = { "Dawn", "Day", "Dusk", "Night" };

static int fail(char *why, size_t cap, const char *text)
{
    if (why != NULL && cap > 0) snprintf(why, cap, "%s", text);
    return 0;
}

static int kind_ok(int kind) { return kind == LT_BRIGHTNESS || kind == LT_TONE || kind == LT_LAMP; }

static int get_any(const LightTrack *f, int kind, int time, int i, Any *a)
{
    if (kind == LT_BRIGHTNESS) return light_track_get_brightness(f, time, i, &a->b);
    if (kind == LT_TONE) return light_track_get_tone(f, time, i, &a->t);
    return light_track_get_lamp(f, time, i, &a->l);
}

static int set_any(LightTrack *f, int kind, int time, int i, const Any *a, char *why, size_t cap)
{
    if (kind == LT_BRIGHTNESS) return light_track_set_brightness(f, time, i, &a->b, why, cap);
    if (kind == LT_TONE) return light_track_set_tone(f, time, i, &a->t, why, cap);
    return light_track_set_lamp(f, time, i, &a->l, why, cap);
}

static unsigned lamp_mask(const LightTrackLamp *l) { return l->has_strip ? l->mask & LT_ALL_POINTS : 0; }

static int same_key(int kind, const Any *a, const Any *b)
{
    if (kind == LT_BRIGHTNESS) return a->b.from == b->b.from && a->b.to == b->b.to && a->b.mask == b->b.mask;
    if (kind == LT_TONE) return a->t.from == b->t.from && a->t.to == b->t.to && a->t.mask == b->t.mask;
    return a->l.node == b->l.node && a->l.has_strip == b->l.has_strip && lamp_mask(&a->l) == lamp_mask(&b->l);
}

static int same_value(int kind, const Any *a, const Any *b)
{
    if (kind == LT_BRIGHTNESS) return a->b.percent == b->b.percent;
    if (kind == LT_TONE) return a->t.percent == b->t.percent && strcmp(a->t.colour, b->t.colour) == 0;
    return a->l.percent == b->l.percent && a->l.has_colour == b->l.has_colour && (!a->l.has_colour || strcmp(a->l.colour, b->l.colour) == 0) &&
           a->l.has_radius == b->l.has_radius && (!a->l.has_radius || a->l.radius == b->l.radius);
}

// another key that the order of the file could matter for: brightness shares nodes and points, a tone shares nodes (a node has one tone); lamps add up whatever their order
static int overlaps(int kind, const Any *a, const Any *b)
{
    if (kind == LT_BRIGHTNESS) return a->b.from <= b->b.to && b->b.from <= a->b.to && (a->b.mask & b->b.mask) != 0;
    if (kind == LT_TONE) return a->t.from <= b->t.to && b->t.from <= a->t.to;
    return 0;
}

int light_track_copy_plan(const LightTrack *f, int kind, int from_time, int index, unsigned times, LightTrackCopyPlan *plan, char *why, size_t cap)
{
    if (!kind_ok(kind)) return fail(why, cap, "only Brightness, Tone and Lamps lines are copied");
    if (from_time < 0 || from_time >= LT_TIMES) return fail(why, cap, "no such time");
    Any src;
    if (!get_any(f, kind, from_time, index, &src)) return fail(why, cap, "there is no such line");
    memset(plan, 0, sizeof *plan);
    plan->kind = kind, plan->from_time = from_time, plan->index = index;
    for (int t = 0; t < LT_TIMES; t++) {
        if (t == from_time || !((times >> t) & 1u)) continue;
        LightTrackCopyTime *to = &plan->to[t];
        to->action = LT_COPY_ADD, to->clash = -1;
        const int n = light_track_count(f, t, kind);
        for (int i = 0; i < n; i++) {
            Any other;
            if (!get_any(f, kind, t, i, &other)) continue;
            if (same_key(kind, &src, &other)) {
                to->action = same_value(kind, &src, &other) ? LT_COPY_SAME : LT_COPY_CLASH;
                to->clash = i;
                to->overlaps = 0;
                break;
            }
            to->overlaps += overlaps(kind, &src, &other);
        }
    }
    return 1;
}

int light_track_copy_apply(LightTrack *f, const LightTrackCopyPlan *plan, int replace, LightTrackCopyResult *result, char *why, size_t cap)
{
    LightTrackCopyResult done = { 0, 0, 0, 0 };
    Any src;
    if (!kind_ok(plan->kind) || !get_any(f, plan->kind, plan->from_time, plan->index, &src)) return fail(why, cap, "there is no such line");
    size_t size = 0;
    char *text = light_track_format(f, &size);
    if (text == NULL) return fail(why, cap, "out of memory");
    LightTrack work;
    char reason[LT_WHY];
    const int cloned = light_track_parse(text, size, f->nodes, &work, reason, sizeof reason);
    free(text);
    if (!cloned) return fail(why, cap, "out of memory");
    for (int t = 0; t < LT_TIMES; t++) {
        const LightTrackCopyTime *to = &plan->to[t];
        int ok = 1;
        if (to->action == LT_COPY_ADD) ok = set_any(&work, plan->kind, t, -1, &src, reason, sizeof reason), done.added += ok;
        else if (to->action == LT_COPY_SAME) done.same++;
        else if (to->action == LT_COPY_CLASH && replace) ok = set_any(&work, plan->kind, t, to->clash, &src, reason, sizeof reason), done.replaced += ok;
        else if (to->action == LT_COPY_CLASH) done.skipped++;
        if (!ok) {
            light_track_free(&work);
            if (why != NULL && cap > 0) snprintf(why, cap, "Not copied to %s: %s", TIME_NAME[t], reason);
            return 0;
        }
    }
    light_track_free(f);
    *f = work;
    if (result != NULL) *result = done;
    return 1;
}

static void strip_words(unsigned mask, int has_strip, char *out, size_t cap)
{
    char t[48];
    if (!has_strip || (mask & LT_ALL_POINTS) == LT_ALL_POINTS || !light_strip_text(mask, t, sizeof t)) out[0] = '\0';
    else snprintf(out, cap, " %s", t);
}

int light_track_copy_key_text(const LightTrack *f, int kind, int time, int index, char *out, size_t cap)
{
    Any a;
    char strip[56];
    if (cap > 0) out[0] = '\0';
    if (!kind_ok(kind) || !get_any(f, kind, time, index, &a)) return 0;
    if (kind == LT_LAMP) {
        strip_words(a.l.mask, a.l.has_strip, strip, sizeof strip);
        snprintf(out, cap, "node %u%s", a.l.node, strip);
    } else {
        const unsigned from = kind == LT_BRIGHTNESS ? a.b.from : a.t.from, to = kind == LT_BRIGHTNESS ? a.b.to : a.t.to, mask = kind == LT_BRIGHTNESS ? a.b.mask : a.t.mask;
        strip_words(mask, 1, strip, sizeof strip);
        snprintf(out, cap, "%u-%u%s", from, to, strip);
    }
    return 1;
}

int light_track_copy_value_text(const LightTrack *f, int kind, int time, int index, char *out, size_t cap)
{
    Any a;
    if (cap > 0) out[0] = '\0';
    if (!kind_ok(kind) || !get_any(f, kind, time, index, &a)) return 0;
    if (kind == LT_BRIGHTNESS) snprintf(out, cap, "%u %%", a.b.percent);
    else if (kind == LT_TONE) snprintf(out, cap, "%s %u %%", a.t.colour, a.t.percent);
    else {
        snprintf(out, cap, "%u %%", a.l.percent);
        if (a.l.has_colour) snprintf(out + strlen(out), cap - strlen(out), " %s", a.l.colour);
        if (a.l.has_radius) snprintf(out + strlen(out), cap - strlen(out), " radius %u", a.l.radius);
    }
    return 1;
}

int light_track_copy_key(const LightTrack *f, int kind, int time, int index, unsigned key[3])
{
    Any a;
    if (!kind_ok(kind) || !get_any(f, kind, time, index, &a)) return 0;
    if (kind == LT_BRIGHTNESS) key[0] = a.b.from, key[1] = a.b.to, key[2] = a.b.mask;
    else if (kind == LT_TONE) key[0] = a.t.from, key[1] = a.t.to, key[2] = a.t.mask;
    else key[0] = a.l.node, key[1] = (unsigned)a.l.has_strip, key[2] = lamp_mask(&a.l);
    return 1;
}
