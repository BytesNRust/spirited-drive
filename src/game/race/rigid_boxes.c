/* Where two bodies' boxes touch (00417404): a face centre or a corner of one
 * inside the other. The original also has a second order of the face tests
 * and a normal always picked by the depth (004b90f8, 004b9108, 004b910c);
 * nothing writes those switches, so only the order and choice the game ships
 * with are kept. Its flag 004f9610 (the normal came from the velocity) is
 * never read and is left out. */
#include "game/race/rigid_body.h"

#include "game/sys/fixed_math.h"
#include "game/sys/rigid_math.h"

enum { FAST_APART = 0xf0000 };

/* The corners in the order they are tried: signs along axes 0, 1, 2. */
static const int8_t CORNERS[8][3] = { { 1, 1, 1 },   { 1, 1, -1 },  { 1, -1, 1 },  { 1, -1, -1 },
                                      { -1, 1, 1 },  { -1, 1, -1 }, { -1, -1, 1 }, { -1, -1, -1 } };

static int32_t magnitude(int32_t v)
{
    return v > 0 ? v : -v;
}

/* p, in the box's frame, within its extents on every axis */
static int inside(const int32_t p[3], const RigidBody *box)
{
    for (int i = 0; i < 3; i++)
        if (p[i] > box->extents[i] || p[i] < -box->extents[i]) return 0;
    return 1;
}

/* The world point at signs[k] times each half extent along the box's axes
 * (0 leaves an axis out). */
static void box_point(const RigidBody *box, const int32_t signs[3], int32_t point[3])
{
    for (int i = 0; i < 3; i++) {
        int32_t at = box->position[i];
        for (int k = 0; k < 3; k++) at += signs[k] * ((box->matrix[3 * k + i] >> 8) * (box->extents[k] >> 8));
        point[i] = at;
    }
}

static void axis_normal(const RigidBody *box, int axis, int32_t sign, int32_t normal[3])
{
    for (int i = 0; i < 3; i++) normal[i] = sign * box->matrix[3 * axis + i];
}

/* A face centre of one box inside the other: b's faces first (b_in_a's
 * columns are b's half axes in a's frame), then a's (a_in_b's rows). */
static int face_inside(const RigidBody *a, const RigidBody *b, const int32_t b_in_a[9], const int32_t a_in_b[9],
                       const int32_t centres[6], int32_t point[3], int32_t normal[3])
{
    for (int32_t s = 1; s >= -1; s -= 2)
        for (int k = 0; k < 3; k++) {
            int32_t p[3];
            for (int i = 0; i < 3; i++) p[i] = centres[i] + s * b_in_a[3 * i + k];
            if (!inside(p, a)) continue;
            const int32_t signs[3] = { k == 0 ? s : 0, k == 1 ? s : 0, k == 2 ? s : 0 };
            box_point(b, signs, point);
            axis_normal(b, k, s, normal);
            return 1;
        }
    for (int32_t s = 1; s >= -1; s -= 2)
        for (int k = 0; k < 3; k++) {
            int32_t p[3];
            for (int i = 0; i < 3; i++) p[i] = s * a_in_b[3 * k + i] - centres[3 + i];
            if (!inside(p, b)) continue;
            const int32_t signs[3] = { k == 0 ? s : 0, k == 1 ? s : 0, k == 2 ? s : 0 };
            box_point(a, signs, point);
            axis_normal(a, k, -s, normal);
            return 1;
        }
    return 0;
}

/* The face a corner went through, of the box it is inside: the axes signed
 * towards the corner (flip[k] -1 turns axis k round). Moving apart fast,
 * the axis least along the other body's velocity; else the shallowest. */
static void corner_normal(const RigidBody *box, const int32_t p[3], const int32_t flip[3], const RigidBody *other,
                          int fast, int32_t normal[3])
{
    int32_t n[3][3], d[3];
    for (int k = 0; k < 3; k++)
        for (int i = 0; i < 3; i++) n[k][i] = flip[k] * box->matrix[3 * k + i];
    int pick;
    if (fast) {
        for (int k = 0; k < 3; k++)
            d[k] = magnitude((n[k][0] >> 8) * (other->velocity[0] >> 8) + (n[k][1] >> 8) * (other->velocity[1] >> 8) +
                             (n[k][2] >> 8) * (other->velocity[2] >> 8));
        pick = d[1] < d[0] && d[2] < d[0] ? 0 : d[1] <= d[2] ? 2 : 1;
    } else {
        for (int k = 0; k < 3; k++) d[k] = p[k] < 0 ? box->extents[k] + p[k] : box->extents[k] - p[k];
        pick = d[0] < d[1] && d[0] < d[2] ? 0 : d[1] < d[2] ? 1 : 2;
    }
    for (int i = 0; i < 3; i++) normal[i] = n[pick][i];
}

static int moving_apart_fast(const RigidBody *a, const RigidBody *b)
{
    int32_t most = magnitude(b->velocity[0] - a->velocity[0]);
    const int32_t y = magnitude(b->velocity[1] - a->velocity[1]), z = magnitude(b->velocity[2] - a->velocity[2]);
    if (most < y) most = y;
    if (most < z) most = z;
    return most > FAST_APART;
}

int rb_boxes_touch(const RigidBody *a, const RigidBody *b, const int32_t b_in_a[9], const int32_t a_in_b[9],
                   const int32_t centres[6], int32_t point[3], int32_t normal[3])
{
    if (face_inside(a, b, b_in_a, a_in_b, centres, point, normal)) return 1;
    for (int c = 0; c < 8; c++) {
        int32_t p[3];
        for (int i = 0; i < 3; i++)
            p[i] = CORNERS[c][0] * b_in_a[3 * i] + CORNERS[c][1] * b_in_a[3 * i + 1] +
                   CORNERS[c][2] * b_in_a[3 * i + 2] + centres[i];
        if (!inside(p, a)) continue;
        const int32_t signs[3] = { CORNERS[c][0], CORNERS[c][1], CORNERS[c][2] };
        box_point(b, signs, point);
        const int32_t flip[3] = { p[0] > 0 ? -1 : 1, p[1] > 0 ? -1 : 1, p[2] > 0 ? -1 : 1 };
        corner_normal(a, p, flip, b, moving_apart_fast(a, b), normal);
        return 1;
    }
    for (int c = 0; c < 8; c++) {
        int32_t q[3];
        for (int i = 0; i < 3; i++)
            q[i] = CORNERS[c][0] * a_in_b[i] + CORNERS[c][1] * a_in_b[3 + i] + CORNERS[c][2] * a_in_b[6 + i] -
                   centres[3 + i];
        if (!inside(q, b)) continue;
        const int32_t signs[3] = { CORNERS[c][0], CORNERS[c][1], CORNERS[c][2] };
        box_point(a, signs, point);
        const int32_t flip[3] = { q[0] < 0 ? -1 : 1, q[1] < 0 ? -1 : 1, q[2] < 0 ? -1 : 1 };
        corner_normal(b, q, flip, a, moving_apart_fast(a, b), normal);
        return 1;
    }
    return 0;
}

enum {
    SEPARATE_STEPS = 0x20,
    CLOSING_FAST = 0x140000,      /* 004b9110 */
    CENTRES_MET = 0x3333,         /* 004b9118 */
    HEAD_ON = 0xd999              /* 004b9120 */
};

static int32_t dot8(const int32_t u[3], const int32_t v[3])
{
    return (u[0] >> 8) * (v[0] >> 8) + (u[1] >> 8) * (v[1] >> 8) + (u[2] >> 8) * (v[2] >> 8);
}

static int32_t largest(const int32_t v[3])
{
    int32_t most = magnitude(v[0]);
    if (most < magnitude(v[1])) most = magnitude(v[1]);
    if (most < magnitude(v[2])) most = magnitude(v[2]);
    return most;
}

/* How far each body moves per step to come apart: closing fast and head on
 * (or with the centres together), back along their own velocities; else
 * along the line between the centres, the lighter (+0x60) moving more. */
static void separating_push(const RigidBody *a, const RigidBody *b, const int32_t gap[3], int32_t push_a[3],
                            int32_t push_b[3])
{
    int32_t closing[3];
    for (int i = 0; i < 3; i++) closing[i] = a->velocity[i] - b->velocity[i];
    int head_on = 0;
    if (largest(closing) > CLOSING_FAST) {
        head_on = 1;
        if (largest(gap) >= CENTRES_MET) {
            int32_t along[3] = { closing[0], closing[1], closing[2] }, across[3] = { gap[0], gap[1], gap[2] };
            rm_unit(along);
            rm_normalise(across);
            head_on = dot8(along, across) > HEAD_ON;
        }
    }
    if (head_on) {
        const int32_t back = -rm_inverse(rm_length(closing)) >> 8;
        for (int i = 0; i < 3; i++) {
            push_a[i] = (back * (a->velocity[i] >> 8)) >> 5;
            push_b[i] = (back * (b->velocity[i] >> 8)) >> 5;
        }
        return;
    }
    int32_t n[3] = { gap[0], gap[1], gap[2] };
    rm_unit_rounded(n);
    const int32_t total = a->pdn_10 + b->pdn_10;
    const int32_t share_a = fx_div(a->pdn_10, total) >> 2, share_b = fx_div(b->pdn_10, total) >> 2;
    for (int i = 0; i < 3; i++) {
        push_a[i] = -((n[i] * share_b) >> 14) >> 5;
        push_b[i] = (n[i] * share_a) >> 19;
    }
}

int rb_bodies_separate(RigidBody *a, RigidBody *b, int32_t point[3], int32_t normal[3])
{
    int32_t turn[3][3], b_in_a[9], a_in_b[9];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) turn[i][j] = dot8(&a->matrix[3 * i], &b->matrix[3 * j]);
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            b_in_a[3 * i + j] = (turn[i][j] >> 8) * (b->extents[j] >> 8);
            a_in_b[3 * i + j] = (turn[i][j] >> 8) * (a->extents[i] >> 8);
        }
    int touched = 0;
    int32_t push_a[3] = { 0, 0, 0 }, push_b[3] = { 0, 0, 0 };
    for (int step = 0; step < SEPARATE_STEPS; step++) {
        int32_t gap[3], centres[6];
        for (int i = 0; i < 3; i++) gap[i] = b->position[i] - a->position[i];
        for (int i = 0; i < 3; i++) {
            centres[i] = dot8(&a->matrix[3 * i], gap);
            centres[3 + i] = dot8(&b->matrix[3 * i], gap);
        }
        if (!rb_boxes_touch(a, b, b_in_a, a_in_b, centres, point, normal)) return touched;
        if (!touched) separating_push(a, b, gap, push_a, push_b);
        touched = 1;
        for (int i = 0; i < 3; i++) {
            a->position[i] += push_a[i];
            b->position[i] += push_b[i];
        }
    }
    return touched;
}
