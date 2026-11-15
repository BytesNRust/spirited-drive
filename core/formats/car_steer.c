/* See car_steer.h. In x z, a vertex p turned by angle a about c lands at R(a) (p - c) + c, R(a) = [cos -sin; sin cos]. The fit
 * is the rotation that best takes the wheel's vertices onto vertex + offset (2D Procrustes on the centred points); the pivot then
 * solves (I - R) c = mean(q) - R mean(p). */
#include "formats/car_steer.h"

#include <math.h>
#include <stdlib.h>

static const double PI = 3.14159265358979323846;

static void turn(double a, double cx, double cz, double x, double z, double *ox, double *oz)
{
    const double c = cos(a), s = sin(a);
    *ox = cx + c * (x - cx) - s * (z - cz);
    *oz = cz + s * (x - cx) + c * (z - cz);
}

/* the wheel's vertices (p) and where their offsets put them (q), in x z */
static int points(const MeshDesc *d, const MeshDescPart *part, double (*p)[2], double (*q)[2], ExchangeError *err)
{
    for (uint32_t k = 0; k < part->count; k++) {
        const uint32_t v = part->vertices[k], o = part->first_offset_vertex + k;
        if (v >= d->vertex_count || o >= d->vertex_count) {
            exchange_fail(err, "turn part '%.8s': vertex %u or offset %u is outside the %u vertices", part->name, v, o, d->vertex_count);
            return 0;
        }
        p[k][0] = d->vertices[v].x, p[k][1] = d->vertices[v].z;
        q[k][0] = p[k][0] + d->vertices[o].x, q[k][1] = p[k][1] + d->vertices[o].z;
    }
    return 1;
}

static int fit(const double (*p)[2], const double (*q)[2], uint32_t n, CarSteer *out, ExchangeError *err)
{
    double mp[2] = { 0, 0 }, mq[2] = { 0, 0 };
    for (uint32_t k = 0; k < n; k++) mp[0] += p[k][0], mp[1] += p[k][1], mq[0] += q[k][0], mq[1] += q[k][1];
    mp[0] /= n, mp[1] /= n, mq[0] /= n, mq[1] /= n;
    double dot = 0, cross = 0;
    for (uint32_t k = 0; k < n; k++) {
        const double a[2] = { p[k][0] - mp[0], p[k][1] - mp[1] }, b[2] = { q[k][0] - mq[0], q[k][1] - mq[1] };
        dot += a[0] * b[0] + a[1] * b[1], cross += a[0] * b[1] - a[1] * b[0];
    }
    const double angle = atan2(cross, dot), c = cos(angle), s = sin(angle);
    const double t[2] = { mq[0] - (c * mp[0] - s * mp[1]), mq[1] - (s * mp[0] + c * mp[1]) };
    const double det = (1 - c) * (1 - c) + s * s;   /* of [1-c, s; -s, 1-c] */
    if (det < 1e-9) { exchange_fail(err, "the wheel does not turn: no pivot"); return 0; }
    out->degrees = angle * 180 / PI;
    out->pivot_x = (t[0] * (1 - c) - s * t[1]) / det;
    out->pivot_z = ((1 - c) * t[1] + s * t[0]) / det;
    out->residual = 0;
    for (uint32_t k = 0; k < n; k++) {
        double x, z;
        turn(angle, out->pivot_x, out->pivot_z, p[k][0], p[k][1], &x, &z);
        const double gap = hypot(x - q[k][0], z - q[k][1]);
        if (gap > out->residual) out->residual = gap;
    }
    return 1;
}

int car_steer_fit(const MeshDesc *desc, uint32_t part, CarSteer *out, ExchangeError *err)
{
    if (part >= desc->part_count) { exchange_fail(err, "no turn part %u (the mesh has %u)", part, desc->part_count); return 0; }
    const MeshDescPart *t = &desc->parts[part];
    if (t->count < 2) { exchange_fail(err, "turn part '%.8s' moves %u vertices: a fit needs 2", t->name, t->count); return 0; }
    double (*p)[2] = malloc(t->count * sizeof *p), (*q)[2] = malloc(t->count * sizeof *q);
    int ok = p != NULL && q != NULL;
    if (!ok) exchange_fail(err, "out of memory");
    ok = ok && points(desc, t, p, q, err) && fit((const double (*)[2])p, (const double (*)[2])q, t->count, out, err);
    free(p), free(q);
    return ok;
}

int car_steer_offsets(const MeshVertex *wheel, uint32_t count, double degrees, int outer, MeshVertex *offsets, ExchangeError *err)
{
    if (count == 0) { exchange_fail(err, "a wheel with no vertex"); return 0; }
    if (outer != -1 && outer != 1) { exchange_fail(err, "the wheel's outer side is -1 (left) or +1 (right), not %d", outer); return 0; }
    int32_t x = wheel[0].x, z_least = wheel[0].z, z_most = wheel[0].z;
    for (uint32_t k = 1; k < count; k++) {
        if (outer < 0 ? wheel[k].x < x : wheel[k].x > x) x = wheel[k].x;
        if (wheel[k].z < z_least) z_least = wheel[k].z;
        if (wheel[k].z > z_most) z_most = wheel[k].z;
    }
    const double a = degrees * PI / 180, cz = (z_least + (double)z_most) / 2;
    for (uint32_t k = 0; k < count; k++) {
        double tx, tz;
        turn(a, x, cz, wheel[k].x, wheel[k].z, &tx, &tz);
        offsets[k] = (MeshVertex){ (int32_t)lround(tx - wheel[k].x), 0, (int32_t)lround(tz - wheel[k].z) };
    }
    return 1;
}
