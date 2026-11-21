/* A car kit's check (formats/car_kit_check.h): the wheel rules, by shape alone. */
#include "formats/car_kit_check.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WHEELS = 4 };

static const char *const WHEEL_NAMES[WHEELS] = { "wheel_fl", "wheel_fr", "wheel_rl", "wheel_rr" };

#define NOT_ROUND 0.15     /* height and depth of a wheel differ by more than this share of the larger */
#define PAIR_SIZE 0.10     /* the two wheels of an axle differ by more than this share of the larger height */
#define GROUND 0.10        /* the wheels' bottoms differ by more than this share of a wheel's height */

void kit_problems_init(KitProblems *p) { memset(p, 0, sizeof *p); }

void kit_problems_free(KitProblems *p)
{
    free(p->item);
    memset(p, 0, sizeof *p);
}

size_t kit_problems_errors(const KitProblems *p)
{
    size_t n = 0;
    for (size_t i = 0; i < p->count; i++) n += p->item[i].severity == KIT_ERROR;
    return n;
}

static int add(KitProblems *out, KitSeverity sev, const char *id, const char *fmt, ...) __attribute__((format(printf, 4, 5)));
static int add(KitProblems *out, KitSeverity sev, const char *id, const char *fmt, ...)
{
    if (out->count == out->cap) {
        const size_t cap = out->cap ? out->cap * 2 : 8;
        KitProblem *grown = realloc(out->item, cap * sizeof *grown);
        if (grown == NULL) return 0;
        out->item = grown, out->cap = cap;
    }
    KitProblem *p = &out->item[out->count++];
    p->severity = sev;
    snprintf(p->id, sizeof p->id, "%s", id);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(p->text, sizeof p->text, fmt, ap);
    va_end(ap);
    return 1;
}

/* the importer's reading of a name (car_import_mesh.c): `want`, then perhaps more, case aside */
static int is_wheel(const char *name, const char *want)
{
    const size_t n = strlen(want);
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)name[i]) != want[i]) return 0;
    return 1;
}

typedef struct {
    int object;                     /* -1: none */
    int usable;                     /* has faces, so its box means something */
    double lo[3], hi[3];
} Wheel;

static void measure(const ObjModel *m, Wheel *w)
{
    const ObjObject *o = &m->objects[w->object];
    for (int k = 0; k < 3; k++) w->lo[k] = INFINITY, w->hi[k] = -INFINITY;
    for (uint32_t i = 0; i < o->vertex_count; i++) {
        const ObjVec3 v = m->vertices[o->first_vertex + i];
        const double c[3] = { v.x, v.y, v.z };
        for (int k = 0; k < 3; k++) {
            if (c[k] < w->lo[k]) w->lo[k] = c[k];
            if (c[k] > w->hi[k]) w->hi[k] = c[k];
        }
    }
}

static double extent(const Wheel *w, int axis) { return w->hi[axis] - w->lo[axis]; }
static double middle(const Wheel *w, int axis) { return (w->hi[axis] + w->lo[axis]) / 2; }
static const char *label(const ObjModel *m, const Wheel *w) { return m->objects[w->object].name; }

static int claim(const ObjModel *m, Wheel *wheels, KitProblems *out, int *ok)
{
    for (int w = 0; w < WHEELS; w++) wheels[w].object = -1;
    for (uint32_t o = 0; o < m->object_count; o++)
        for (int w = 0; w < WHEELS; w++) {
            if (!is_wheel(m->objects[o].name, WHEEL_NAMES[w])) continue;
            if (wheels[w].object >= 0)
                *ok = *ok && add(out, KIT_ERROR, "wheel-twice", "Two objects are %s: '%s' and '%s'. Each wheel must be one object.",
                                 WHEEL_NAMES[w], m->objects[wheels[w].object].name, m->objects[o].name);
            else
                wheels[w].object = (int)o;
            break;
        }
    return *ok;
}

static int presence(const ObjModel *m, Wheel *wheels, KitProblems *out)
{
    int ok = 1;
    for (int w = 0; w < WHEELS; w++) {
        if (wheels[w].object < 0) {
            if (w < 2)
                ok = ok && add(out, KIT_ERROR, "wheel-missing", "No object %s: the steered %s front wheel. The race hangs without both front wheels.",
                               WHEEL_NAMES[w], w ? "right" : "left");
            else
                ok = ok && add(out, KIT_WARNING, "wheel-missing-rear", "No object %s: the car has no %s rear wheel.", WHEEL_NAMES[w], w == 2 ? "left" : "right");
            continue;
        }
        if (m->objects[wheels[w].object].face_count == 0) {
            ok = ok && add(out, KIT_ERROR, "wheel-no-faces", "%s has no faces: a wheel is drawn with polygons, one flat face on each side.", label(m, &wheels[w]));
            continue;
        }
        wheels[w].usable = 1;
        measure(m, &wheels[w]);
    }
    return ok;
}

static int shape(const ObjModel *m, const Wheel *w, KitProblems *out)
{
    const double h = extent(w, 1), d = extent(w, 2), x = extent(w, 0), big = fmax(h, d);
    int ok = 1;
    if (big > 0 && fabs(h - d) > NOT_ROUND * big)
        ok = add(out, KIT_WARNING, "wheel-not-round", "%s is %.0f high and %.0f deep: a wheel is round, its height and depth about equal.", label(m, w), h, d);
    if (x > big && big > 0)
        ok = ok && add(out, KIT_WARNING, "wheel-too-wide", "%s is %.0f wide but only %.0f high: a wheel is narrower than it is tall.", label(m, w), x, big);
    return ok;
}

static int side(const ObjModel *m, int index, const Wheel *w, KitProblems *out)
{
    const int left = index % 2 == 0;
    const double x = middle(w, 0);
    if ((left && x < 0) || (!left && x > 0) || x == 0) return 1;
    return add(out, KIT_WARNING, "wheel-side", "%s is on the car's %s side (x %.0f) but named for the %s one: Blender's +X is the car's right.", label(m, w),
               left ? "right" : "left", x, left ? "left" : "right");
}

static int pair(const ObjModel *m, const Wheel *a, const Wheel *b, KitProblems *out)
{
    const double ha = extent(a, 1), hb = extent(b, 1), big = fmax(ha, hb);
    if (big <= 0 || fabs(ha - hb) <= PAIR_SIZE * big) return 1;
    return add(out, KIT_WARNING, "wheel-pair-size", "%s is %.0f high and %s %.0f: the two wheels of an axle should be the same size.", label(m, a), ha, label(m, b), hb);
}

static int order(const ObjModel *m, const Wheel *front, const Wheel *rear, KitProblems *out)
{
    if (middle(front, 2) < middle(rear, 2)) return 1;   /* Blender's front is -Z */
    return add(out, KIT_WARNING, "wheel-order", "%s is behind %s: the car's front is towards -Z in Blender.", label(m, front), label(m, rear));
}

static int ground(const ObjModel *m, const Wheel *wheels, KitProblems *out)
{
    double low = INFINITY, high = -INFINITY, tall = 0;
    for (int w = 0; w < WHEELS; w++) {
        if (!wheels[w].usable) continue;
        if (wheels[w].lo[1] < low) low = wheels[w].lo[1];
        if (wheels[w].lo[1] > high) high = wheels[w].lo[1];
        if (extent(&wheels[w], 1) > tall) tall = extent(&wheels[w], 1);
    }
    if (tall <= 0 || high - low <= GROUND * tall) return 1;
    (void)m;
    return add(out, KIT_WARNING, "wheel-ground", "The wheels' bottoms are %.0f apart (a wheel is %.0f high): all four should rest on the ground.", high - low, tall);
}

int car_kit_check_wheels(const ObjModel *m, KitProblems *out)
{
    Wheel wheels[WHEELS];
    memset(wheels, 0, sizeof wheels);
    int ok = 1;
    claim(m, wheels, out, &ok);
    ok = presence(m, wheels, out) && ok;
    for (int w = 0; w < WHEELS && ok; w++)
        if (wheels[w].usable) ok = shape(m, &wheels[w], out) && side(m, w, &wheels[w], out);
    for (int axle = 0; axle < 2 && ok; axle++)
        if (wheels[axle * 2].usable && wheels[axle * 2 + 1].usable) ok = pair(m, &wheels[axle * 2], &wheels[axle * 2 + 1], out);
    for (int f = 0; f < 2 && ok; f++)   /* each front wheel against the rear one on its side */
        if (wheels[f].usable && wheels[f + 2].usable) ok = order(m, &wheels[f], &wheels[f + 2], out);
    return ok && ground(m, wheels, out);
}
