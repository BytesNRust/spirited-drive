#include "formats/track_frame.h"

#include "formats/fixed_math.h"

enum { ANGLE_MASK = 0x3fff, TO_ANGLE16 = 2, HALF = 0x8000 };

/* a * b in 16.16, rounded to the nearest */
static int32_t mul(int32_t a, int32_t b)
{
    return (int32_t)(((int64_t)a * b + HALF) >> 16);
}

static int32_t sine(int32_t angle14)
{
    return fx_sin16((int32_t)(((uint32_t)angle14 & ANGLE_MASK) << TO_ANGLE16));
}

static int32_t cosine(int32_t angle14)
{
    return fx_cos16((int32_t)(((uint32_t)angle14 & ANGLE_MASK) << TO_ANGLE16));
}

/* The axes are the road's own (right, up, along) after a bank about along, a pitch about right and a heading about up. */
void track_frame_make(int32_t heading, int32_t slope, int32_t bank, TrackFrame *frame)
{
    const int32_t sb = sine(bank), cb = cosine(bank);
    const int32_t sp = sine(slope), cp = cosine(slope);
    const int32_t sh = sine(heading), ch = cosine(heading);
    /* in the heading-0, level frame: right (cb, -sb, 0), up (sb, cb, 0), along (0, 0, 1) */
    const int32_t flat[3][3] = { { cb, -sb, 0 }, { sb, cb, 0 }, { 0, 0, 0x10000 } };
    for (int i = 0; i < 3; ++i) {
        const int32_t x = flat[i][0], y = flat[i][1], z = flat[i][2];
        const int32_t y1 = mul(y, cp) + mul(z, sp), z1 = mul(z, cp) - mul(y, sp);   /* pitch: along gains height */
        frame->axis[i][0] = mul(x, ch) + mul(z1, sh);                                /* heading: along turns towards +x */
        frame->axis[i][1] = y1;
        frame->axis[i][2] = mul(z1, ch) - mul(x, sh);
    }
}

void track_frame_of_node(const TriNode *node, TrackFrame *frame)
{
    track_frame_make(node->orientation, node->slope, node->slant_a, frame);
}

static int32_t dot(const int32_t axis[3], const int32_t v[3])
{
    const int64_t sum = (int64_t)axis[0] * v[0] + (int64_t)axis[1] * v[1] + (int64_t)axis[2] * v[2];
    return (int32_t)((sum + HALF) >> 16);
}

void track_frame_to_local(const TrackFrame *frame, const int32_t world[3], int32_t local[3])
{
    int32_t out[3];
    for (int i = 0; i < 3; ++i) out[i] = dot(frame->axis[i], world);
    local[0] = out[0], local[1] = out[1], local[2] = out[2];
}

void track_frame_to_world(const TrackFrame *frame, const int32_t local[3], int32_t world[3])
{
    int32_t out[3];
    for (int j = 0; j < 3; ++j) {
        const int64_t sum = (int64_t)frame->axis[0][j] * local[0] + (int64_t)frame->axis[1][j] * local[1]
                            + (int64_t)frame->axis[2][j] * local[2];
        out[j] = (int32_t)((sum + HALF) >> 16);
    }
    world[0] = out[0], world[1] = out[1], world[2] = out[2];
}
