/* port (body shine, envmap): see car_envmap.h. The image is additive, as the PS1 reflection pass: 128 is transparent (no light added, the
 * paint stays as it is), and above it a byte is a white streak or spot. */
#include "game/race/car_envmap.h"

#include "game/race/car_shine.h"

#include <math.h>

enum { SIZE = CAR_ENVMAP_SIZE, CIRCLES = 4 };
static const float STREAK_WIDTH = 0.03f;                    /* the half-width of a streak, as the reflection's distance from its circle */
static const float SUN_SPREAD = 0.004f;                     /* 1 - cos of the sun spot's angle: it falls off over about 0.09 radian */
static const float SUN_LIFT = 1.0f;
static const float SUN[3] = { -0.4505f, 0.4505f, 0.7708f }; /* unit, toward the viewer, upper left */

/* the great circles the streaks follow, as their unit normals (toward the viewer, y up): the first is the horizon, unbroken; the others
 * are cut into pieces by the azimuth, so the streaks are long and thin with gaps, not a glow */
static const float CIRCLE[CIRCLES][3] = {
    { 0.0f, 1.0f, 0.0f },
    { 0.6136f, 0.7892f, 0.0f },      /* tilted 38 degrees: a long streak across the upper half */
    { -0.4508f, 0.5585f, 0.6976f },
    { 0.3015f, -0.2513f, 0.9196f },
};

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

void car_envmap_uv(const float normal[3], const float view[3], float uv[2])
{
    const float d = view[0] * normal[0] + view[1] * normal[1] + view[2] * normal[2];
    float r[3];
    for (int k = 0; k < 3; k++) r[k] = view[k] - 2.0f * d * normal[k];
    /* the sphere map looks down -z with z toward the viewer; the camera's z points away, so its reflection is flipped */
    const float rx = r[0], ry = r[1], rz = -r[2];
    float m = 2.0f * sqrtf(rx * rx + ry * ry + (rz + 1.0f) * (rz + 1.0f));
    if (m < 1e-6f) m = 1e-6f;
    uv[0] = clampf(rx / m * 0.5f + 0.5f, 0.0f, 1.0f);
    uv[1] = clampf(ry / m * 0.5f + 0.5f, 0.0f, 1.0f);
}

int32_t car_envmap_pack(const float uv[2])
{
    const uint32_t u = (uint32_t)lroundf(clampf(uv[0], 0.0f, 1.0f) * 65535.0f);
    const uint32_t v = (uint32_t)lroundf(clampf(uv[1], 0.0f, 1.0f) * 65535.0f);
    return (int32_t)((u << 16) | v);
}

void car_envmap_unpack(int32_t packed, float uv[2])
{
    const uint32_t p = (uint32_t)packed;
    uv[0] = (float)(p >> 16) / 65535.0f;
    uv[1] = (float)(p & 0xffffu) / 65535.0f;
}

/* the streak strength (0 .. 1) in the direction `r` (unit, z toward the viewer, y up): the strongest of the great circles, each thin */
static float streak(const float r[3])
{
    const float phi = atan2f(r[1], r[0]);
    float best = 0.0f;
    for (int i = 0; i < CIRCLES; i++) {
        const float *n = CIRCLE[i];
        const float d = fabsf(n[0] * r[0] + n[1] * r[1] + n[2] * r[2]);
        float w = expf(-(d / STREAK_WIDTH) * (d / STREAK_WIDTH));
        if (i > 0) w *= clampf(0.5f + 2.0f * sinf(5.0f * phi + 1.3f * (float)i), 0.0f, 1.0f);   /* the gaps */
        if (w > best) best = w;
    }
    return best;
}

/* the light added in the direction `r`, 0 (none) .. 1 (the hot spot) */
static float light(const float r[3])
{
    const float dot = r[0] * SUN[0] + r[1] * SUN[1] + r[2] * SUN[2];
    const float spot = SUN_LIFT * expf(-(1.0f - dot) / SUN_SPREAD);
    const float s = 0.85f * streak(r);
    return clampf(spot > s ? spot : s, 0.0f, 1.0f);
}

static void make(uint8_t *image)
{
    for (int y = 0; y < SIZE; y++) {
        const float t = 1.0f - 2.0f * ((float)y + 0.5f) / SIZE;
        for (int x = 0; x < SIZE; x++) {
            const float s = 2.0f * ((float)x + 0.5f) / SIZE - 1.0f;
            const float rho2 = clampf(s * s + t * t, 0.0f, 1.0f);   /* the disc: its rim is the directions behind */
            const float rz = 1.0f - 2.0f * rho2, k = sqrtf(2.0f * (1.0f + rz));
            const float r[3] = { s * k, t * k, rz };
            image[y * SIZE + x] = (uint8_t)lroundf(clampf(128.0f + 127.0f * light(r), 128.0f, 255.0f));
        }
    }
}

const uint8_t *car_envmap_image(void)
{
    static uint8_t image[SIZE * SIZE];
    static int made;
    if (!made) make(image), made = 1;
    return image;
}

uint8_t car_envmap_sample(const uint8_t *image, float u, float v)
{
    int x = (int)(clampf(u, 0.0f, 1.0f) * SIZE), y = (int)(clampf(1.0f - v, 0.0f, 1.0f) * SIZE);
    x = x >= SIZE ? SIZE - 1 : x, y = y >= SIZE ? SIZE - 1 : y;
    return image[y * SIZE + x];
}

float car_envmap_level(uint8_t sample, float gain)
{
    const float e = clampf(((float)sample - 128.0f) / 127.0f, 0.0f, 1.0f);   /* additive only: nothing below the neutral */
    return clampf(CAR_SHINE_NEUTRAL + (CAR_SHINE_LEVELS - 1 - CAR_SHINE_NEUTRAL) * e * gain, 0.0f, (float)(CAR_SHINE_LEVELS - 1));
}
