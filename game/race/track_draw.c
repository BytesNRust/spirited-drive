#include "game/race/track_draw.h"

#include "game/gfx/x86_int.h"
#include "game/race/track_tables.h"

#include <string.h>

unsigned outcode_shared(const ClipRect *clip, const int32_t *screen, int count)
{
    unsigned shared = 0xF;
    for (int i = 0; i < count && shared != 0; i++) {
        /* compared unsigned, so INT32_MIN (behind the eye) is right of the screen */
        const uint32_t x = (uint32_t)screen[2 * i], y = (uint32_t)screen[2 * i + 1];
        unsigned code = 0;
        if (x < (uint32_t)clip->x0) code |= 1;
        if ((uint32_t)clip->x1 < x + 1) code |= 2;
        if (y < (uint32_t)clip->y0) code |= 4;
        if ((uint32_t)clip->y1 < y + 1) code |= 8;
        shared &= code;
    }
    return shared;
}

/* 004255ec: cell (u_cell, v_cell) of a texture split 2^u_level by 2^v_level;
 * cells are counted from the far edge. */
void track_draw_cell(const DrawContext *ctx, const FshEntry *t, int u_level, int v_level, int u_cell, int v_cell,
                     const int32_t screen[8])
{
    /* the entry's +4 / +6 read as signed words; shifts by cl, 32-bit products */
    const int32_t w = (int16_t)t->width >> (u_level & 31), h = (int16_t)t->height >> (v_level & 31);
    const int32_t u0 = x_shl(x_mul(x_sub(x_sub(x_shl(1, u_level & 31), 1), u_cell), w), 16);
    const int32_t v0 = x_shl(x_mul(h, x_sub(x_sub(x_shl(1, v_level & 31), 1), v_cell)), 16);
    const int32_t u1 = x_add(u0, x_shl(w - 1, 16)), v1 = x_add(v0, x_shl(h - 1, 16));
    if (u0 < u1 && v0 < v1) {
        const int32_t uv[8] = { u0, v0, u1, v0, u1, v1, u0, v1 };
        ctx->sink.draw_uv(ctx->sink.user, t, uv, screen);
    }
}

/* (value * t) >> 16 rounded half up, low 32 bits. */
static int32_t mul_round(int32_t value, int32_t t)
{
    const int64_t p = (int64_t)value * t;
    return (int32_t)((uint32_t)((uint64_t)p >> 16) + (uint32_t)(((uint64_t)p >> 15) & 1));
}

/* A point in front of the near plane is pushed onto it along the road
 * direction before projecting (the same as 0044aa94 does for vertices). */
void track_project_near(const DrawContext *ctx, const int32_t p[3], int32_t near, int32_t screen[2])
{
    int32_t q[3] = { p[0], p[1], p[2] };
    if (p[2] < near) {
        const int32_t dz = ctx->near_dir[2], gap = x_sub(near, p[2]);
        const int32_t t = (int32_t)(((int64_t)(gap % dz) << 16) / dz) + (int32_t)((uint32_t)(gap / dz) << 16);
        q[0] = x_add(p[0], mul_round(t, ctx->near_dir[0]));
        q[1] = x_add(p[1], mul_round(t, ctx->near_dir[1]));
        q[2] = near;
    }
    view_project(ctx->view, q, screen);
}

static void midpoint(const int32_t *a, const int32_t *b, int32_t out[3])
{
    for (int i = 0; i < 3; i++) out[i] = x_add(a[i], b[i]) / 2;
}

static void pack(int32_t out[8], const int32_t *s0, const int32_t *s1, const int32_t *s2, const int32_t *s3)
{
    const int32_t *s[4] = { s0, s1, s2, s3 };
    for (int i = 0; i < 4; i++) out[2 * i] = s[i][0], out[2 * i + 1] = s[i][1];
}

/* 004256ac: a quarter's corners and screen points copied together, then 00425718 on them */
static void child(const DrawContext *ctx, const FshEntry *t, int u_level, int depth, int u_cell, int v_cell,
                  const int32_t *c0, const int32_t *c1, const int32_t *c2, const int32_t *c3, const int32_t *s0,
                  const int32_t *s1, const int32_t *s2, const int32_t *s3, int32_t near)
{
    const int32_t *const cam[4] = { c0, c1, c2, c3 };
    int32_t screen[8];
    pack(screen, s0, s1, s2, s3);
    track_draw_subdivided(ctx, t, u_level, depth, u_cell, v_cell, cam, screen, near);
}

void track_draw_subdivided(const DrawContext *ctx, const FshEntry *texture, int u_level, int depth, int u_cell,
                           int v_cell, const int32_t *const cam[4], const int32_t screen[8], int32_t near)
{
    if (near > cam[3][2] && near > cam[0][2]) return;
    const int32_t limit = TRACK_SUBDIVIDE_Z[depth] >> (ctx->world_shift & 31);  /* sar by cl */
    if ((cam[1][2] >= limit && cam[2][2] >= limit) || depth >= TRACK_SUBDIVIDE_MAX) {
        if ((outcode_shared(&ctx->clip, screen, 4) & 7) == 0)
            track_draw_cell(ctx, texture, u_level, depth, u_cell, v_cell, screen);
        return;
    }
    /* Four children; the camera-space corners passed down are the plain
     * midpoints, only their projections use the near-plane push. */
    int32_t m01[3], m23[3], m03[3], mc[3], m12[3];
    int32_t s01[2], s23[2], s03[2], sc[2], s12[2];
    midpoint(cam[0], cam[1], m01);
    track_project_near(ctx, m01, near, s01);
    midpoint(cam[2], cam[3], m23);
    track_project_near(ctx, m23, near, s23);
    midpoint(cam[0], cam[3], m03);
    track_project_near(ctx, m03, near, s03);
    midpoint(m23, m01, mc);
    track_project_near(ctx, mc, near, sc);
    const int u = u_level + 1, d = depth + 1;
    child(ctx, texture, u, d, 2 * u_cell + 1, 2 * v_cell + 1, cam[0], m01, mc, m03, &screen[0], s01, sc, s03, near);
    child(ctx, texture, u, d, 2 * u_cell + 1, 2 * v_cell, m03, mc, m23, cam[3], s03, sc, s23, &screen[6], near);
    if (near > m23[2] && near > m01[2]) return;
    midpoint(cam[1], cam[2], m12);
    track_project_near(ctx, m12, near, s12);
    child(ctx, texture, u, d, 2 * u_cell, 2 * v_cell + 1, m01, cam[1], m12, mc, s01, &screen[2], s12, sc, near);
    child(ctx, texture, u, d, 2 * u_cell, 2 * v_cell, mc, m12, cam[2], m23, sc, s12, &screen[4], s23, near);
}
