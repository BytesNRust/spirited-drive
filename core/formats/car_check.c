#include "formats/car_check.h"

#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { SQUARE_PERCENT = 5, MIRROR_PERCENT = 10 };   /* how far the front axle may be off, of the car's length and width */

void car_check_free(CarCheckList *list)
{
    free(list->item);
    memset(list, 0, sizeof *list);
}

int car_check_add(CarCheckList *list, CarCheckSeverity severity, const char *format, ...)
{
    if (list->count == list->cap) {
        const size_t cap = list->cap ? list->cap * 2 : 8;
        CarCheckItem *grown = realloc(list->item, cap * sizeof *grown);
        if (grown == NULL) return 0;
        list->item = grown, list->cap = cap;
    }
    CarCheckItem *it = &list->item[list->count];
    it->severity = severity;
    va_list ap;
    va_start(ap, format);
    vsnprintf(it->text, sizeof it->text, format, ap);
    va_end(ap);
    list->count++;
    return 1;
}

size_t car_check_count(const CarCheckList *list, CarCheckSeverity severity)
{
    size_t n = 0;
    for (size_t i = 0; i < list->count; i++) n += list->item[i].severity == severity;
    return n;
}

typedef struct { const MeshDescPart *part; double x, y, z; int ok; } Wheel;

static const MeshDescPart *part_named(const MeshDesc *m, const char *prefix)
{
    for (uint32_t i = 0; i < m->part_count; i++)
        if (strncmp(m->parts[i].name, prefix, strlen(prefix)) == 0) return &m->parts[i];
    return NULL;
}

/* the wheel's middle; 0 with the finding when it moves no vertex or names one the model has not */
static int wheel_centre(const MeshDesc *m, const char *label, Wheel *w, CarCheckList *out, int *ok)
{
    const MeshDescPart *p = w->part;
    const uint32_t own = m->real_vertices && m->real_vertices < m->vertex_count ? m->real_vertices : m->vertex_count;
    if (p->count == 0) return car_check_add(out, CAR_CHECK_ERROR, "%s moves no vertex: that front wheel will not turn", label), *ok = 0, 1;
    w->x = w->y = w->z = 0;
    for (uint32_t k = 0; k < p->count; k++) {
        if (p->vertices[k] >= own) return car_check_add(out, CAR_CHECK_ERROR, "%s names vertex %u, outside the model's %u", label, p->vertices[k], own), *ok = 0, 1;
        w->x += m->vertices[p->vertices[k]].x, w->y += m->vertices[p->vertices[k]].y, w->z += m->vertices[p->vertices[k]].z;
    }
    w->x /= p->count, w->y /= p->count, w->z /= p->count;
    w->ok = 1;
    return 1;
}

static void body_box(const MeshDesc *m, double lo[3], double hi[3])
{
    const uint32_t n = m->real_vertices && m->real_vertices < m->vertex_count ? m->real_vertices : m->vertex_count;
    for (int k = 0; k < 3; k++) lo[k] = INFINITY, hi[k] = -INFINITY;
    for (uint32_t i = 0; i < n; i++) {
        const double v[3] = { m->vertices[i].x, m->vertices[i].y, m->vertices[i].z };
        for (int k = 0; k < 3; k++) lo[k] = v[k] < lo[k] ? v[k] : lo[k], hi[k] = v[k] > hi[k] ? v[k] : hi[k];
    }
}

static int steered_wheels(const MeshDesc *m, CarCheckList *out)
{
    const MeshDescPart *left = part_named(m, "left_tu"), *right = part_named(m, "right_tu");
    if (left == NULL && right == NULL) return 1;   /* a car with no steered wheel: nothing to say */
    if (left == NULL || right == NULL)
        return car_check_add(out, CAR_CHECK_ERROR, "the car has %s but no %s: both front wheels must turn", left ? "left_tur" : "right_tu", left ? "right_tu" : "left_tur");
    Wheel l = { left, 0, 0, 0, 0 }, r = { right, 0, 0, 0, 0 };
    int ok = 1;
    if (!wheel_centre(m, "left_tur", &l, out, &ok) || !wheel_centre(m, "right_tu", &r, out, &ok) || !ok) return 1;
    double lo[3], hi[3];
    body_box(m, lo, hi);
    if (l.x >= 0 || r.x <= 0)
        return car_check_add(out, CAR_CHECK_ERROR, "left_tur is at x %.0f and right_tu at x %.0f: the car's left is -x, so the wheels are swapped or the model is mirrored", l.x, r.x);
    const double middle = (lo[1] + hi[1]) / 2;
    if (l.y >= middle || r.y >= middle)
        if (!car_check_add(out, CAR_CHECK_WARNING, "a front wheel is above the middle of the car: its height is %.0f of %.0f..%.0f", l.y >= middle ? l.y : r.y, lo[1], hi[1])) return 0;
    if (fabs(l.y - r.y) > (hi[1] - lo[1]) * SQUARE_PERCENT / 100 || fabs(l.z - r.z) > (hi[2] - lo[2]) * SQUARE_PERCENT / 100) {
        /* the move that squares it, said for the right wheel (the left stays): along the car is z (its front is +z), height y */
        const double dz = r.z - l.z, dy = r.y - l.y;
        char along[64] = "", height[64] = "";
        if (fabs(dz) >= 1) snprintf(along, sizeof along, "%.0f %s", fabs(dz), dz > 0 ? "back" : "forward");
        if (fabs(dy) >= 1) snprintf(height, sizeof height, "%.0f %s", fabs(dy), dy > 0 ? "down" : "up");
        if (!car_check_add(out, CAR_CHECK_WARNING, "the front axle is not square: right_tu is %.0f %s left_tur along the car and %.0f %s it; to square it move right_tu %s%s%s (the model's units)",
                           fabs(dz), dz > 0 ? "ahead of" : "behind", fabs(dy), dy > 0 ? "above" : "below", along, along[0] && height[0] ? " and " : "", height)) return 0;
    }
    if (fabs(l.x + r.x) > (hi[0] - lo[0]) * MIRROR_PERCENT / 100)
        if (!car_check_add(out, CAR_CHECK_WARNING, "the front wheels do not mirror about the middle of the car: x %.0f and %.0f", l.x, r.x)) return 0;
    return 1;
}

static int labelled(const MeshDesc *m, uint8_t texture, const char *label)
{
    return texture < m->texture_count && strncmp(m->textures[texture].label, label, 8) == 0;
}

/* the race puts a tyre's frames on the quads its label texture: two for each of frntires and rertires, one at each side of the car and flat across x
 * (within FLAT units), so a wheel is one quad seen from the side */
static int tyre_label(const MeshDesc *m, const char *label, CarCheckList *out)
{
    enum { FLAT = 5 };
    int quads = 0, side[2] = { 0, 0 }, bad = 0;
    for (uint32_t i = 0; i < m->polygon_count; i++) {
        const MeshDescPolygon *p = &m->polygons[i];
        if (!labelled(m, p->texture, label)) continue;
        quads++;
        if ((p->flags & 7) != 4) {
            bad = 1;
            if (!car_check_add(out, CAR_CHECK_WARNING, "%s is on a polygon with %d corners: the tyre frame needs a quad", label, p->flags & 7)) return 0;
            continue;
        }
        int32_t lo = INT32_MAX, hi = INT32_MIN;
        double sum = 0;
        for (int k = 0; k < 4; k++) {
            if (p->vertex[k] >= m->vertex_count) { bad = 1; break; }
            const int32_t x = m->vertices[p->vertex[k]].x;
            lo = x < lo ? x : lo, hi = x > hi ? x : hi, sum += x;
        }
        if (bad) continue;
        side[sum < 0 ? 0 : 1]++;
        if (hi - lo > FLAT && !car_check_add(out, CAR_CHECK_WARNING, "a %s quad is not flat across x (%d units wide): a wheel seen from the side is a plane", label, hi - lo)) return 0;
    }
    if (quads == 0) return 1;
    if (quads != 2 && !bad) return car_check_add(out, CAR_CHECK_WARNING, "%s is on %d quads: the race puts the tyre frame on 2, one at each side", label, quads);
    if (quads == 2 && !bad && (side[0] != 1 || side[1] != 1)) return car_check_add(out, CAR_CHECK_WARNING, "the two %s quads must be one at each side of the car", label);
    return 1;
}

int car_check_wheels(const MeshDesc *mesh, CarCheckList *list)
{
    return steered_wheels(mesh, list) && tyre_label(mesh, "frntires", list) && tyre_label(mesh, "rertires", list);
}
