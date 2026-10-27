/* port (body shine, envmap): see car_envmap.h. The image is signed around the neutral byte (128): above it a lift (the sky, the clouds, the sun),
 * below it a shade (the band under the horizon, the ground). */
#include "game/race/car_envmap.h"

#include "formats/png.h"
#include "game/race/car_shine.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

enum { SIZE = CAR_ENVMAP_SIZE, CLOUD_BLOCK = 4, MAX_SIDE = 4096, PATH = 512 };
static const float SUN_SPREAD = 0.012f;                       /* 1 - cos of the sun spot's angle: it falls off over about 0.16 radian */
static const float SUN_BYTES = 127.0f;                        /* the sun's lift at its centre: 128 + this is the brightest byte (255) */
static const float SUN[3] = { -0.3478f, 0.7951f, 0.4969f };   /* unit, toward the viewer, high and upper left (elevation about 53 degrees) */

/* the sky (bytes above the neutral): from the horizon up to the zenith, a light gradient */
static const float SKY_HORIZON = 40.0f, SKY_ZENITH = 90.0f;

/* the clouds: the noise's threshold band (soft edges), their lift in bytes, the elevation they fade in over, and the blocks they are drawn in
 * (the image's texels are quantised to CLOUD_BLOCK texels, so the edges read blocky, as a PS1 texture's do) */
static const float CLOUD_LOW = 0.6f, CLOUD_HIGH = 0.85f, CLOUD_BYTES = 40.0f, CLOUD_FADE = 0.35f;
/* the sky's brightest byte before the sun: below the sun's, so the sun spot is the brightest texel */
static const float SKY_CAP = 110.0f;

/* the ground (bytes below the neutral): a flat dark grey, and the band just under the horizon that is darker (far scenery) */
static const float GROUND_DROP = -64.0f, BAND_DROP = -92.0f;
static const float BAND_CENTRE = -0.05f, BAND_WIDTH = 0.04f;   /* in elevation: the band lies just below the horizon */

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static float smoothstep01(float t) { return t <= 0.0f ? 0.0f : t >= 1.0f ? 1.0f : t * t * (3.0f - 2.0f * t); }

/* the sphere map's axes in camera space: y is the world's up, z points toward the viewer (the camera's -z) projected off the up, and x
 * completes them (x = z cross y). Only the up is the world's; the horizontal axes turn with the world, not with the camera's roll. */
static void frame(const float up[3], float x[3], float z[3])
{
    const float fwd[3] = { 0.0f, 0.0f, -1.0f };
    float d = fwd[0] * up[0] + fwd[1] * up[1] + fwd[2] * up[2];
    for (int k = 0; k < 3; k++) z[k] = fwd[k] - d * up[k];
    float len = sqrtf(z[0] * z[0] + z[1] * z[1] + z[2] * z[2]);
    if (len < 1e-4f) {   /* the up points along the view: take the camera's x instead */
        const float side[3] = { 1.0f, 0.0f, 0.0f };
        d = side[0] * up[0];
        for (int k = 0; k < 3; k++) z[k] = side[k] - d * up[k];
        len = sqrtf(z[0] * z[0] + z[1] * z[1] + z[2] * z[2]);
        if (len < 1e-6f) len = 1.0f;
    }
    for (int k = 0; k < 3; k++) z[k] /= len;
    x[0] = z[1] * up[2] - z[2] * up[1];
    x[1] = z[2] * up[0] - z[0] * up[2];
    x[2] = z[0] * up[1] - z[1] * up[0];
}

void car_envmap_uv(const float normal[3], const float view[3], const float up[3], float uv[2])
{
    const float d = view[0] * normal[0] + view[1] * normal[1] + view[2] * normal[2];
    float r[3];
    for (int k = 0; k < 3; k++) r[k] = view[k] - 2.0f * d * normal[k];
    float x[3], z[3];
    frame(up, x, z);
    const float rx = r[0] * x[0] + r[1] * x[1] + r[2] * x[2];
    const float ry = r[0] * up[0] + r[1] * up[1] + r[2] * up[2];
    const float rz = r[0] * z[0] + r[1] * z[1] + r[2] * z[2];
    float m = 2.0f * sqrtf(rx * rx + ry * ry + (rz + 1.0f) * (rz + 1.0f));
    if (m < 1e-6f) m = 1e-6f;
    uv[0] = clampf(rx / m + 0.5f, 0.0f, 1.0f);
    uv[1] = clampf(ry / m + 0.5f, 0.0f, 1.0f);
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

/* a hash of a lattice point, 0 .. 1 */
static float hash2(int x, int y)
{
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (float)(h & 0xffffu) / 65535.0f;
}

/* value noise over the lattice, smoothly interpolated; x wraps every `period` cells, so the sky goes round the car */
static float vnoise(float x, float y, int period)
{
    const float fx = floorf(x), fy = floorf(y);
    const int ix = (int)fx, iy = (int)fy;
    const float tx = x - fx, ty = y - fy;
    const float sx = tx * tx * (3.0f - 2.0f * tx), sy = ty * ty * (3.0f - 2.0f * ty);
    const int x0 = ((ix % period) + period) % period, x1 = (x0 + 1) % period;
    const float a = hash2(x0, iy), b = hash2(x1, iy), c = hash2(x0, iy + 1), d = hash2(x1, iy + 1);
    const float top = a + (b - a) * sx, bottom = c + (d - c) * sx;
    return top + (bottom - top) * sy;
}

/* the clouds in the direction `r` (unit, sphere frame, y up), 0 (clear) .. 1 (a patch): two octaves of noise over the azimuth (12 cells
 * round) and the elevation, thresholded to soft patches with clear gaps between them, and faded out toward the horizon */
static float cloud(const float r[3])
{
    const float phi = atan2f(r[2], r[0]) / (2.0f * 3.14159265f) + 0.5f;
    const float x = phi * 12.0f, y = r[1] * 8.0f;
    const float n = 0.6f * vnoise(x, y, 12) + 0.4f * vnoise(2.0f * x + 5.0f, 2.0f * y + 3.0f, 24);
    return smoothstep01((n - CLOUD_LOW) / (CLOUD_HIGH - CLOUD_LOW)) * smoothstep01(r[1] / CLOUD_FADE);
}

/* the band and the ground below the horizon (elevation e <= 0): the flat ground, darker in a band just under the line */
static float ground(float e)
{
    const float q = (e - BAND_CENTRE) / BAND_WIDTH;
    const float g = expf(-q * q);
    return GROUND_DROP + (BAND_DROP - GROUND_DROP) * g;
}

/* the sky above the horizon (elevation e > 0): a gradient, brighter toward the zenith */
static float sky(float e)
{
    return SKY_HORIZON + (SKY_ZENITH - SKY_HORIZON) * smoothstep01(e);
}

/* the direction of the texel at (s, t) on the sphere map's disc (z toward the viewer, y up) */
static void direction(float s, float t, float r[3])
{
    const float rho2 = clampf(s * s + t * t, 0.0f, 1.0f);   /* the disc: its rim is the directions behind */
    const float rz = 1.0f - 2.0f * rho2, k = sqrtf(2.0f * (1.0f + rz));
    r[0] = s * k, r[1] = t * k, r[2] = rz;
}

/* the offset from the neutral byte (-128 .. 127) of the texel in the direction `r`: the sky, the clouds (looked up at `rc`, the block's
 * centre) and the sun above the horizon, the band and ground below */
static float offset(const float r[3], const float rc[3])
{
    const float dot = r[0] * SUN[0] + r[1] * SUN[1] + r[2] * SUN[2];
    const float spot = expf(-(1.0f - dot) / SUN_SPREAD);   /* 0 away from the sun, 1 at its centre */
    float base = ground(r[1]);
    if (r[1] > 0) {
        base = sky(r[1]) + CLOUD_BYTES * cloud(rc);
        if (base > SKY_CAP) base = SKY_CAP;
    }
    return base + (SUN_BYTES - base) * spot;   /* the sun lifts toward its peak, so the ground (with no spot) is left as it is */
}

static void make(uint8_t *image)
{
    for (int y = 0; y < SIZE; y++) {
        const float t = 1.0f - 2.0f * ((float)y + 0.5f) / SIZE;
        for (int x = 0; x < SIZE; x++) {
            const float s = 2.0f * ((float)x + 0.5f) / SIZE - 1.0f;
            float r[3], rc[3];
            direction(s, t, r);
            /* the clouds are looked up at their block's centre, so their edges are blocks; the sky and the sun at the texel */
            const int bx = x / CLOUD_BLOCK * CLOUD_BLOCK + CLOUD_BLOCK / 2, by = y / CLOUD_BLOCK * CLOUD_BLOCK + CLOUD_BLOCK / 2;
            direction(2.0f * ((float)bx + 0.5f) / SIZE - 1.0f, 1.0f - 2.0f * ((float)by + 0.5f) / SIZE, rc);
            image[y * SIZE + x] = (uint8_t)lroundf(clampf(128.0f + offset(r, rc), 0.0f, 255.0f));
        }
    }
}

/* the red channel of a decoded PNG pixel (an indexed one through its palette): the image is grey */
static uint8_t grey_at(const PngImage *img, size_t at)
{
    return img->indexed ? img->palette[3 * img->pixels[at]] : img->pixels[4 * at];
}

/* the whole file at `path` (malloc'd, NULL when it cannot be read). The stdio read keeps this layer free of the platform's (the SDL one). */
static uint8_t *read_file(const char *path, size_t *size)
{
    *size = 0;
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    uint8_t *data = NULL;
    long n = 0;
    if (fseek(f, 0, SEEK_END) == 0) n = ftell(f);
    if (n > 0 && fseek(f, 0, SEEK_SET) == 0) data = malloc((size_t)n);
    if (data != NULL && fread(data, 1, (size_t)n, f) == (size_t)n) *size = (size_t)n;
    else free(data), data = NULL;
    fclose(f);
    return data;
}

int car_envmap_file(const char *path, uint8_t *image)
{
    if (path == NULL || image == NULL) return 0;
    size_t size = 0;
    uint8_t *file = read_file(path, &size);
    if (file == NULL) return 0;
    PngImage img;
    const int read = png_read(file, size, &img) == PNG_OK;
    free(file);
    if (!read) return 0;
    if (img.width == 0 || img.height == 0 || img.width > MAX_SIDE || img.height > MAX_SIDE) {
        png_free(img.pixels);
        return 0;
    }
    for (int y = 0; y < SIZE; y++) {
        const size_t sy = (size_t)y * img.height / SIZE;
        for (int x = 0; x < SIZE; x++) {
            const size_t sx = (size_t)x * img.width / SIZE;
            image[y * SIZE + x] = grey_at(&img, sy * img.width + sx);
        }
    }
    png_free(img.pixels);
    return 1;
}

void car_envmap_build(uint8_t *image)
{
    const char *env = getenv("TNFS_CAR_ENVMAP");
    if (env != NULL && env[0] != '\0' && car_envmap_file(env, image)) return;
    if (car_envmap_file("data/car_envmap.png", image)) return;
#ifdef TNFS_SOURCE_DIR
    char path[PATH];
    snprintf(path, sizeof path, "%s/data/car_envmap.png", TNFS_SOURCE_DIR);
    if (car_envmap_file(path, image)) return;
#endif
    make(image);
}

const uint8_t *car_envmap_image(void)
{
    static uint8_t image[SIZE * SIZE];
    static int made;
    if (!made) car_envmap_build(image), made = 1;
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
    const float e = clampf(((float)sample - 128.0f) / 127.0f, -1.0f, 1.0f);
    const float level = e >= 0
        ? CAR_SHINE_NEUTRAL + (CAR_SHINE_LEVELS - 1 - CAR_SHINE_NEUTRAL) * e * gain
        : CAR_SHINE_NEUTRAL + CAR_SHINE_NEUTRAL * e * CAR_ENVMAP_SHADE;
    return clampf(level, 0.0f, (float)(CAR_SHINE_LEVELS - 1));
}
