#include "game/race/weather.h"
#include "game/race/weather_snow.h"
#include "game/race/headlight_pool.h"
#include "game/race/wiper.h"
#include "game/sys/fixed_math.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum {
    DROPS_640x480 = 240,       /* the count in a 640 x 480 view; more pixels, more drops */
    DROPS_FX_640x480 = 45,      /* rain_fx: few, a hint of rain and not its main element (the references' streaks are barely there) */
    DROPS_MAX = 2000,
    FALL_MIN = 5200,           /* a streak's fall a tick, of 65536 box heights: 7 to 12 ticks across */
    FALL_RANGE = 3600,
    LENGTH_MIN = 28,           /* a streak's length in 480ths of the box height */
    LENGTH_RANGE = 24,
    SLANT_DIVISOR = 5          /* the tail's push to the right: length / 5 */
};

int weather_named(const char *name)
{
    char lower[8];
    size_t i = 0;
    for (; name[i] != '\0' && i < sizeof lower - 1; i++) lower[i] = (char)(name[i] >= 'A' && name[i] <= 'Z' ? name[i] + 32 : name[i]);
    lower[i] = '\0';
    if (name[i] != '\0') return -1;
    for (int w = 0; w < WEATHER_COUNT; w++)
        if (strcmp(lower, weather_name(w)) == 0) return w;
    return -1;
}

int weather_wet(int weather) { return weather == WEATHER_RAIN || weather == WEATHER_STORM; }

const char *weather_name(int weather)
{
    static const char *const NAMES[WEATHER_COUNT] = { "clear", "fog", "rain", "storm" };
    return weather >= 0 && weather < WEATHER_COUNT ? NAMES[weather] : NAMES[0];
}

static void mix(const uint8_t a[3], const uint8_t b[3], int percent, uint8_t out[3])
{
    for (int k = 0; k < 3; k++) out[k] = (uint8_t)((a[k] * (100 - percent) + b[k] * percent + 50) / 100);
}

void weather_fog_colour(int weather, const uint8_t base[3], uint8_t out[3])
{
    static const uint8_t FOG_GREY[3] = { 196, 198, 204 }, RAIN_GREY[3] = { 104, 110, 120 };
    mix(base, weather_wet(weather) ? RAIN_GREY : FOG_GREY, weather_wet(weather) ? 60 : 55, out);
}

void weather_sky_colour(int weather, const uint8_t fog[3], uint8_t out[3])
{
    static const uint8_t RAIN_SKY[3] = { 62, 68, 78 };
    if (weather_wet(weather)) memcpy(out, RAIN_SKY, 3);
    else memcpy(out, fog, 3);
}

static int whiter_snow(int snow, int variant) { return variant >= 1 && snow > 0; }

void weather_fog_colour_for(int weather, int snow, int variant, const uint8_t base[3], uint8_t out[3])
{
    static const uint8_t SNOW_WHITE[3] = { 230, 234, 242 };
    if (weather_wet(weather) && whiter_snow(snow, variant)) mix(base, SNOW_WHITE, snow >= SNOW_HEAVY ? 80 : 72, out);
    else weather_fog_colour(weather, base, out);
}

void weather_sky_colour_for(int weather, int snow, int variant, const uint8_t fog[3], uint8_t out[3])
{
    if (weather_wet(weather) && whiter_snow(snow, variant)) memcpy(out, fog, 3);   /* a white sky, as the fog's own */
    else weather_sky_colour(weather, fog, out);
}

int weather_sky_is_wet_dark(int weather, int snow, int variant) { return weather_wet(weather) && !(whiter_snow(snow, variant)); }

int weather_fog_snow_mood(int snow, int variant) { return !(snow > 0 && variant >= 2); }

void weather_fog_shape(int weather, int snow, int variant, WeatherFogShape *out)
{
    if (weather == WEATHER_FOG) {
        *out = (WeatherFogShape){ WEATHER_FOG_START, WEATHER_FOG_END, 0, 0 };
        if (weather_fog_curve()) out->exp = WEATHER_FOG_EXP_SCALE, out->cap = WEATHER_FOG_EXP_CAP_PERCENT / 100.0f;
        return;
    }
    *out = (WeatherFogShape){ WEATHER_RAIN_FOG_START, WEATHER_RAIN_FOG_END, 0, 0 };
    if (variant < 1 || !weather_wet(weather)) return;
    if (snow >= SNOW_HEAVY) *out = (WeatherFogShape){ 150, 3000, 1500, 0.95f };   /* the snow storm: a wall of white */
    else if (snow > 0) *out = (WeatherFogShape){ 250, 6000, 2600, 0.90f };        /* snow: the far is lost in white */
    else if (weather == WEATHER_STORM) *out = (WeatherFogShape){ 900, 7000, 0, 0 };   /* the rain storm: denser than the rain's, a line */
}

void weather_fog_for_tint(const uint8_t fog[3], const float tint[3], uint8_t out[3])
{
    for (int k = 0; k < 3; k++) {
        const float t = tint[k] < 0.05f ? 0.05f : tint[k];
        const float v = (float)fog[k] * (1.0f + t) / (2.0f * t) + 0.5f;
        out[k] = (uint8_t)(v > 255.0f ? 255 : v);
    }
}

int weather_fog_curve(void)
{
    const char *v = getenv("TNFS_FOG_CURVE");
    return v == NULL || strcmp(v, "line") != 0;
}

int weather_sky_level(int32_t x, int32_t y, int32_t cx, int32_t cy, float focal, const float up[3], int low, int high)
{
    static const float BAYER[4][4] = { { 0.0f / 16, 8.0f / 16, 2.0f / 16, 10.0f / 16 }, { 12.0f / 16, 4.0f / 16, 14.0f / 16, 6.0f / 16 },
                                       { 3.0f / 16, 11.0f / 16, 1.0f / 16, 9.0f / 16 }, { 15.0f / 16, 7.0f / 16, 13.0f / 16, 5.0f / 16 } };
    if (high <= low) return low;
    const float rise = ((float)(x - cx) * up[0] + (float)(cy - y) * up[1] + focal * up[2]) / focal;   /* the ray's rise over its run */
    const float t = rise <= 0 ? 0.0f : rise >= WEATHER_SKY_FADE_TAN / 100.0f ? 1.0f : rise * 100.0f / WEATHER_SKY_FADE_TAN;
    const float level = (float)high - (float)(high - low) * t;
    const int l = (int)(level + BAYER[y & 3][x & 3]);
    return l < low ? low : l > high ? high : l;
}

void weather_rain_colour(uint8_t out[3])
{
    out[0] = 214, out[1] = 220, out[2] = 232;
}

/* a 32-bit hash (the finaliser of murmur3) of a drop and which of its numbers it is */
static uint32_t hash(uint32_t drop, uint32_t which)
{
    uint32_t h = drop * 0x9e3779b1u + which * 0x85ebca6bu + 0x27d4eb2fu;
    h ^= h >> 16, h *= 0x85ebca6bu, h ^= h >> 13, h *= 0xc2b2ae35u, h ^= h >> 16;
    return h;
}

int weather_rain_count(int32_t box_w, int32_t box_h)
{
    if (box_w <= 0 || box_h <= 0) return 0;
    const int64_t n = (int64_t)DROPS_640x480 * box_w * box_h / (640 * 480);
    return n > DROPS_MAX ? DROPS_MAX : n < 1 ? 1 : (int)n;
}

int weather_rain_count_fx(int32_t box_w, int32_t box_h)
{
    if (box_w <= 0 || box_h <= 0) return 0;
    const int64_t n = (int64_t)DROPS_FX_640x480 * box_w * box_h / (640 * 480);
    return n > DROPS_MAX ? DROPS_MAX : n < 1 ? 1 : (int)n;
}

int weather_rain_width(int scale)
{
    return scale < 1 ? 1 : scale;
}

void weather_rain_streak(int drop, uint32_t tick, int32_t box_w, int32_t box_h, int32_t out[4])
{
    const uint32_t d = (uint32_t)drop;
    const uint32_t fall = FALL_MIN + hash(d, 1) % FALL_RANGE;
    const uint32_t phase = hash(d, 2) & 0xffff;
    const int32_t column = (int32_t)((int64_t)(hash(d, 3) & 0xffff) * box_w >> 16);
    const int32_t length = box_h * (LENGTH_MIN + (int32_t)(hash(d, 4) % LENGTH_RANGE)) / 480;
    const uint32_t along = (phase + fall * tick) & 0xffff;   /* the head's way down the box, of 65536 */
    const int32_t head = (int32_t)((int64_t)along * box_h >> 16);
    out[2] = column - head / SLANT_DIVISOR;
    out[3] = head;
    out[0] = out[2] + length / SLANT_DIVISOR;
    out[1] = head - length;
}

/* ---- rain_fx: streaks by speed and light ---- */

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

/* the eye's place along the car's nose from its centre (the cockpit's and the bumper's; a chase camera sits behind, but the drops are taken near the lens either way) */
static const float EYE_FORWARD_M = 0.5f;

int weather_rain_cone_weight(float ahead, float lateral, float height)
{
    return headlight_cone_weight_local((double)ahead + (double)EYE_FORWARD_M, (double)lateral, (double)height);   /* the cone's own, so the rain is lit where the road is */
}

int weather_rain_level_fog(int level)
{
    static const int FOG[WEATHER_RAIN_LEVELS] = { 3, 5, 7, 10 };
    return FOG[level < 0 ? 0 : level >= WEATHER_RAIN_LEVELS ? WEATHER_RAIN_LEVELS - 1 : level];
}

float weather_rain_blend(int32_t speed)
{
    const float metres = (float)((speed < 0 ? -(int64_t)speed : speed) >> 16);
    const float t = clampf(metres / WEATHER_SPOKE_TURNED, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);   /* smooth: a streak's direction turns to the spoke's by WEATHER_SPOKE_TURNED m/s, so they do not cross at the speeds between */
}

/* where the ray from (vx, vy) along the unit direction (ux, uy) is inside the box w x h: from `*enter` (0 when the point is inside it: where a spoke is born) to `*leave` (where it
 * leaves the view). The point may be off the box (a vanishing point off the screen: the outside cameras, the helicopter's): the spokes are then nearly parallel, and live between the two. */
static void box_run(float vx, float vy, float ux, float uy, float w, float h, float *enter, float *leave)
{
    float t0 = 0.0f, t1 = 1e9f;
    const float v[2] = { vx, vy }, u[2] = { ux, uy }, size[2] = { w, h };
    for (int k = 0; k < 2; k++) {
        if (fabsf(u[k]) < 1e-4f) continue;
        const float a = (0.0f - v[k]) / u[k], b = (size[k] - v[k]) / u[k];
        t0 = fmaxf(t0, fminf(a, b)), t1 = fminf(t1, fmaxf(a, b));
    }
    if (t1 < t0 + 1.0f) t1 = t0 + 1.0f;
    *enter = t0, *leave = t1;
}

/* the light level of a drop (-1 when it is gone): the day's, or at night what the headlights' cone gives where its head is; a wedge drop leaves as the car speeds up */
static int rain_level(uint32_t d, int night, float focal, float fvx, float fvy, const int32_t out[4], float keep, float b)
{
    int l = WEATHER_RAIN_DAY_LEVEL;
    if (night && focal > 0.0f) {
        const float z = 4.0f + 36.0f * (float)(hash(d, 6) & 0xffff) / 65536.0f * (float)(hash(d, 6) & 0xffff) / 65536.0f;   /* the depth the drop is taken at: most near */
        const float rx = ((float)out[2] - fvx) / focal, ry = (fvy - (float)out[3]) / focal;
        const int w = weather_rain_cone_weight(z, rx * z, 1.2f + ry * z);
        l = w < 24 ? 0 : w < 96 ? 1 : w < 176 ? 2 : 3;
    }
    /* a wedge drop leaves as the car speeds up, each at a speed of its own (no pop of them all at once) */
    if (keep < 1.0f && b * (1.0f - keep) > (float)(hash(d, 8) & 0xffff) / 65536.0f) l = -1;
    return l;
}

void weather_rain_spoke(int drop, uint32_t tick, int32_t speed, int32_t box_w, int32_t box_h, int32_t vx, int32_t vy, float focal, int night, int32_t out[4], int *level)
{
    const uint32_t d = (uint32_t)drop;
    const uint32_t fall = FALL_MIN + hash(d, 1) % FALL_RANGE, phase = hash(d, 2) & 0xffff;
    const float W = (float)box_w, H = (float)box_h, fvx = (float)vx, fvy = (float)vy;
    const float base = (float)(box_h * (LENGTH_MIN + (int32_t)(hash(d, 4) % LENGTH_RANGE)) / 480);
    const float metres = (float)((speed < 0 ? -(int64_t)speed : speed) >> 16);
    const float s = clampf(metres / WEATHER_SPOKE_FULL, 0.0f, 1.0f);   /* how stretched */
    const float b = weather_rain_blend(speed);                          /* how far turned to the spoke */
    const float p = (float)((phase + fall * tick) & 0xffff) / 65536.0f;
    /* The drop's own place: a column over the box's whole width widened by the slant (a falling streak leans left, so columns only over the box left its right edge dry: a gap on a wide
     * view) and a height. A standing car's streak falls from the top of that column; a fast car's is a spoke from the vanishing point toward where that place is: the same side of the
     * vanishing point it began on, so the drops turn as the speed grows and none crosses to the other side. */
    const float lean = 0.196f, reach = H * (1.0f + 0.6f * s);
    const float column = (float)(hash(d, 3) & 0xffff) / 65536.0f * (W + lean * H), height = (float)(hash(d, 5) & 0xffff) / 65536.0f * H;
    float ux = column - lean * height * 0.5f - fvx, uy = height - fvy;
    float len2 = ux * ux + uy * uy;
    if (len2 < 100.0f) {   /* a place on the vanishing point itself has no direction of its own: any */
        const float a = 6.2831853f * (float)(hash(d, 9) & 0xffff) / 65536.0f;
        ux = cosf(a), uy = sinf(a), len2 = 1.0f;
    }
    const float inv = 1.0f / sqrtf(len2);
    ux *= inv, uy *= inv;
    float enter, leave;
    box_run(fvx, fvy, ux, uy, W, H, &enter, &leave);
    const float run = leave - enter;
    /* the spoke's head runs out to the edge; short, stretching a little with the speed and with the way out */
    const float rad_head = enter + run * powf(p, 1.5f);
    float rad_len = base * (0.2f + 0.7f * s * p);
    float keep = 1.0f;
    if (uy > 0.8f) {   /* the wedge straight down, the road ahead: some drops, shorter, not none (the lower the sparser) */
        rad_len *= 0.5f;
        keep = 0.5f;
    }
    const float fall_head = reach * p;
    const float fall_len = base * 0.45f;
    const float dxn = -lean, dyn = 1.0f;   /* a standing streak's direction down the box (not unit: y runs 1, x leans) */
    const float hx = (1.0f - b) * (column + dxn * fall_head) + b * (fvx + ux * rad_head), hy = (1.0f - b) * (dyn * fall_head) + b * (fvy + uy * rad_head);
    const float fall_tail = fall_head > fall_len ? fall_head - fall_len : 0.0f, rad_tail = rad_head - rad_len > enter ? rad_head - rad_len : enter;
    const float tx = (1.0f - b) * (column + dxn * fall_tail) + b * (fvx + ux * rad_tail), ty = (1.0f - b) * (dyn * fall_tail) + b * (fvy + uy * rad_tail);
    out[0] = (int32_t)tx, out[1] = (int32_t)ty, out[2] = (int32_t)hx, out[3] = (int32_t)hy;
    *level = rain_level(d, night, focal, fvx, fvy, out, keep, b);
}

int weather_lens_count(int32_t box_w, int32_t box_h)
{
    if (box_w <= 0 || box_h <= 0) return 0;
    const int64_t n = (int64_t)WEATHER_LENS_PER_640x480 * box_w * box_h / (640 * 480);
    return n > WEATHER_LENS_MAX ? WEATHER_LENS_MAX : n < 1 ? 1 : (int)n;
}

int weather_lens_drop(int drop, uint32_t tick, int32_t speed, int32_t box_w, int32_t box_h, int32_t vx, int32_t vy, int32_t out[4])
{
    const uint32_t d = (uint32_t)drop;
    const uint32_t period = 150 + hash(d, 20) % 150;
    const uint32_t t = tick + hash(d, 21) % period, cycle = t / period;
    const float u = (float)(t % period) / (float)period;
    const uint32_t life = d * 977u + cycle * 7919u;   /* a place and a size of its own every life */
    if (hash(life, 22) % 8 >= 6) return 0;             /* some lives are empty: the glass is not covered at all times */
    const float W = (float)box_w, H = (float)box_h;
    const float full = (float)(3 + hash(life, 23) % 5) * H / 480.0f;
    float x = (0.04f + 0.92f * (float)(hash(life, 24) & 0xffff) / 65536.0f) * W, y = (0.06f + 0.74f * (float)(hash(life, 25) & 0xffff) / 65536.0f) * H;
    float r = full, keep = 16.0f;
    if (u < 0.05f) r = full * (0.25f + 0.75f * u / 0.05f);   /* it lands: a few frames of growing */
    if (u > 0.7f) {                                           /* it goes: smaller and thinner */
        const float f = (u - 0.7f) / 0.3f;
        r = full * (1.0f - 0.3f * f), keep = 16.0f * (1.0f - f);
    }
    if (u > 0.15f) {   /* it slides a little: down on the glass, out from the vanishing point when the car goes */
        const float b = weather_rain_blend(speed), slide = H * 0.06f * (u - 0.15f);
        float ux = x - (float)vx, uy = y - (float)vy;
        const float n = sqrtf(ux * ux + uy * uy);
        if (n > 1.0f) ux /= n, uy /= n; else ux = 0.0f, uy = 1.0f;
        x += b * ux * slide, y += (1.0f - b) * slide + b * uy * slide;
    }
    if (keep < 1.0f || r < 1.0f) return 0;
    if (x < 0 || y < 0 || x >= W || y >= H) return 0;
    out[0] = (int32_t)x, out[1] = (int32_t)y, out[2] = (int32_t)(r + 0.5f), out[3] = (int32_t)keep;
    return 1;
}

int weather_lens_drop_wiped(int drop, uint32_t tick, int32_t speed, int32_t box_w, int32_t box_h, int32_t vx, int32_t vy, int32_t out[4], const struct WiperPass *wipers)
{
    if (!weather_lens_drop(drop, tick, speed, box_w, box_h, vx, vy, out)) return 0;
    if (wipers == NULL) return 1;
    const uint32_t d = (uint32_t)drop, period = 150 + hash(d, 20) % 150, t = tick + hash(d, 21) % period;
    const int64_t landed = (int64_t)tick - (int64_t)(t % period);   /* when this life began */
    const int64_t pass = wiper_pass_latest(wipers, tick, (float)out[0], (float)out[1]);
    return pass < 0 || pass < landed;
}

int weather_rain_spray_puff(int k, int side, uint32_t tick, int32_t speed, int wet_road, int32_t out[3], int32_t *size)
{
    const int64_t v = speed < 0 ? -(int64_t)speed : speed;
    const float metres = (float)(v >> 16);
    *size = 0;
    if (!wet_road || metres < 8.0f || k < 0 || k >= WEATHER_SPRAY_PUFFS) return 0;
    const uint32_t h = hash((uint32_t)k * 2u + (side > 0 ? 1u : 0u), 7u + tick);
    const float jitter = (float)(h & 0xff) / 255.0f - 0.5f, jitter2 = (float)((h >> 8) & 0xff) / 255.0f - 0.5f;
    const float reach = clampf(metres / 30.0f, 0.4f, 1.6f);                  /* the plume's length grows with the speed */
    const float t = (float)k / (float)(WEATHER_SPRAY_PUFFS - 1);            /* 0 at the wheel .. 1 at the plume's end */
    const float behind = 0.3f + 3.0f * reach * t + jitter * 0.3f;   /* a chase camera sits only a few metres behind: a plume longer than that is behind the lens */
    const float sideways = (float)side * (0.3f + 1.6f * t) + jitter2 * 0.4f;
    const float up = 0.2f + 1.2f * t + jitter * 0.2f;
    out[0] = (int32_t)(behind * 65536.0f), out[1] = (int32_t)(sideways * 65536.0f), out[2] = (int32_t)(up * 65536.0f);
    const float grow = fminf(0.45f + 1.5f * t, WEATHER_SPRAY_CAP), thin = t > 0.85f ? 1.0f - (t - 0.85f) / 0.15f * 0.6f : 1.0f;   /* a puff grows as it goes from the wheel, up to a cap, and fades at the trail's end */
    const float speedy = clampf(metres / 40.0f, 0.5f, 1.0f);
    *size = (int32_t)(256.0f * grow * thin * speedy);
    return *size > 0;
}

int weather_rain_spray_level(int night)
{
    return night ? 1 : WEATHER_RAIN_LEVELS - 1;
}

/* the lightning's brightness a tick after its strike: full at once, then decaying (of WEATHER_FLASH_PEAK) */
static const uint8_t FLASH_DECAY[WEATHER_FLASH_TICKS] = { 15, 14, 11, 8, 6, 4, 3, 2, 1, 1 };

int weather_rain_flash(uint32_t tick)
{
    if (tick < 150) return 0;
    const uint32_t slot = tick / WEATHER_STORM_SLOT, at = tick - slot * WEATHER_STORM_SLOT;
    const uint32_t h = hash(slot, 11);
    if ((h & 3) == 0) return 0;                                     /* one slot in four is quiet */
    const uint32_t start = (h >> 4) % (WEATHER_STORM_SLOT - WEATHER_STORM_DOUBLE - WEATHER_FLASH_TICKS - 1);
    const int twice = (h >> 12) % 3 == 0;                           /* one strike in three flashes a second time, a little weaker */
    if (at >= start && at - start < WEATHER_FLASH_TICKS) return FLASH_DECAY[at - start];
    if (twice && at >= start + WEATHER_STORM_DOUBLE && at - start - WEATHER_STORM_DOUBLE < WEATHER_FLASH_TICKS) return FLASH_DECAY[at - start - WEATHER_STORM_DOUBLE] * 3 / 4;
    return 0;
}

int weather_thunder_strike(uint32_t tick)
{
    return tick > 0 && weather_rain_flash(tick) > weather_rain_flash(tick - 1);
}

int weather_thunder_delay(uint32_t tick)
{
    return WEATHER_THUNDER_MIN_TICKS + (int)(hash(tick, 23) % (uint32_t)(WEATHER_THUNDER_MAX_TICKS - WEATHER_THUNDER_MIN_TICKS + 1));
}

int weather_thunder_volume(int delay)
{
    const int d = delay < WEATHER_THUNDER_MIN_TICKS ? WEATHER_THUNDER_MIN_TICKS : delay > WEATHER_THUNDER_MAX_TICKS ? WEATHER_THUNDER_MAX_TICKS : delay;
    return 100 - (100 - WEATHER_THUNDER_FAR_PERCENT) * (d - WEATHER_THUNDER_MIN_TICKS) / (WEATHER_THUNDER_MAX_TICKS - WEATHER_THUNDER_MIN_TICKS);
}

int weather_open_for_distance(int nearest)
{
    return nearest < 0 || nearest >= WEATHER_MOUTH_NODES ? 16 : nearest * 16 / WEATHER_MOUTH_NODES;
}

int weather_flash_open(int level, int open)
{
    return level * (open < 0 ? 0 : open > 16 ? 16 : open) / 16;
}

int weather_thunder_inside(int volume, int open)
{
    const int o = open < 0 ? 0 : open > 16 ? 16 : open;
    return volume * (WEATHER_THUNDER_INSIDE_PERCENT + (100 - WEATHER_THUNDER_INSIDE_PERCENT) * o / 16) / 100;
}

int weather_fog_open(int open, float start, float *end, float *cap)
{
    if (open >= 16) return 1;
    if (open <= 0) return 0;
    *end = start + (*end - start) * 16.0f / (float)open;   /* the same fog over a longer reach */
    *cap = *cap * (float)open / 16.0f;
    return 1;
}

void weather_open_nodes(const uint8_t *closed, uint32_t nodes, uint8_t *open_out)
{
    for (uint32_t n = 0; n < nodes; n++) {
        int nearest = -1;
        for (int d = 0; d < WEATHER_MOUTH_NODES && nearest < 0; d++)
            if (closed[(n + (uint32_t)d) % nodes] || closed[(n + nodes - (uint32_t)d % nodes) % nodes]) nearest = d;
        open_out[n] = (uint8_t)weather_open_for_distance(nearest);
    }
}

int32_t weather_wet_grip(int32_t dry, int32_t wet, int open)
{
    const int o = open < 0 ? 0 : open > 16 ? 16 : open;
    return dry + (wet - dry) * o / 16;
}

int weather_rain_percent(int storm, int v2)
{
    if (v2) return storm ? WEATHER_STORM_V2_PERCENT : WEATHER_RAIN_V2_PERCENT;
    return storm ? 100 * WEATHER_STORM_DROPS : 100;
}

int weather_rain_scaled(int base, int storm, int v2)
{
    const int n = (int)((int64_t)base * weather_rain_percent(storm, v2) / 100);
    return n < 1 && base > 0 ? 1 : n;
}

void weather_flash_colour(uint8_t out[3])
{
    out[0] = 248, out[1] = 250, out[2] = 255;
}

static float smooth_step(float a, float b, float x)
{
    const float t = x <= a ? 0.0f : x >= b ? 1.0f : (x - a) / (b - a);
    return t * t * (3.0f - 2.0f * t);
}

int weather_on_carriageway(int32_t lateral, int left_verge, int right_verge)
{
    const int64_t verge = (int64_t)(lateral < 0 ? left_verge : right_verge) * 0x2000;
    const int64_t side = lateral < 0 ? -(int64_t)lateral : lateral;
    return verge <= 0 || side <= verge;
}

int32_t weather_across_road(int32_t angle, int32_t dx, int32_t dz)
{
    return fx_mul_round(fx_cos16(angle), dx) - fx_mul_round(fx_sin16(angle), dz);
}

int weather_reflection_weight(float dot, float distance)
{
    const float facing = smooth_step(0.3f, 0.7f, dot < 0.0f ? -dot : dot);
    const float near = 1.0f - smooth_step((float)WEATHER_REFLECT_NEAR_M, (float)WEATHER_REFLECT_FAR_M, distance);
    return (int)(100.0f * facing * near + 0.5f);
}
