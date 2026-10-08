#include "game/sys/fixed_math.h"

/* 004c6b84: sin over a quarter turn, 257 entries, 16.16. */
static const int32_t QUARTER_SINE[257] = {
    0, 402, 804, 1206, 1608, 2010, 2412, 2814, 3215, 3617,
    4018, 4420, 4821, 5222, 5622, 6023, 6423, 6823, 7223, 7623,
    8022, 8421, 8819, 9218, 9616, 10013, 10410, 10807, 11204, 11600,
    11995, 12390, 12785, 13179, 13573, 13966, 14359, 14751, 15142, 15533,
    15923, 16313, 16702, 17091, 17479, 17866, 18253, 18638, 19024, 19408,
    19792, 20175, 20557, 20938, 21319, 21699, 22078, 22456, 22833, 23210,
    23586, 23960, 24334, 24707, 25079, 25450, 25820, 26189, 26557, 26925,
    27291, 27656, 28020, 28383, 28745, 29105, 29465, 29824, 30181, 30538,
    30893, 31247, 31600, 31952, 32302, 32651, 32999, 33346, 33692, 34036,
    34379, 34721, 35061, 35400, 35738, 36074, 36409, 36743, 37075, 37406,
    37736, 38064, 38390, 38716, 39039, 39361, 39682, 40002, 40319, 40636,
    40950, 41263, 41575, 41885, 42194, 42501, 42806, 43110, 43412, 43712,
    44011, 44308, 44603, 44897, 45189, 45480, 45768, 46055, 46340, 46624,
    46906, 47186, 47464, 47740, 48015, 48288, 48558, 48828, 49095, 49360,
    49624, 49886, 50145, 50403, 50659, 50914, 51166, 51416, 51665, 51911,
    52155, 52398, 52639, 52877, 53114, 53348, 53581, 53811, 54040, 54266,
    54491, 54713, 54933, 55152, 55368, 55582, 55794, 56004, 56212, 56417,
    56621, 56822, 57022, 57219, 57414, 57606, 57797, 57986, 58172, 58356,
    58538, 58718, 58895, 59070, 59243, 59414, 59583, 59749, 59913, 60075,
    60235, 60392, 60547, 60700, 60850, 60998, 61144, 61288, 61429, 61568,
    61705, 61839, 61971, 62100, 62228, 62353, 62475, 62596, 62714, 62829,
    62942, 63053, 63162, 63268, 63371, 63473, 63571, 63668, 63762, 63854,
    63943, 64030, 64114, 64197, 64276, 64353, 64428, 64501, 64571, 64638,
    64703, 64766, 64826, 64884, 64939, 64992, 65043, 65091, 65136, 65179,
    65220, 65258, 65294, 65327, 65358, 65386, 65412, 65436, 65457, 65475,
    65491, 65505, 65516, 65524, 65531, 65534, 65536
};

/* The inline fold the original uses everywhere: bit 8 mirrors the quarter,
 * bit 9 negates. */
int32_t fx_sin(uint32_t angle)
{
    const uint32_t x = angle & 0xFF;
    const int32_t v = QUARTER_SINE[(angle & 0x100) ? 0x100 - x : x];
    return (angle & 0x200) ? -v : v;
}

int32_t fx_cos(uint32_t angle)
{
    return fx_sin(angle + 0x100);
}

/* 00472fc0 / 00473030: 16-bit angles (0x10000 per turn). The table gives the
 * 10-bit step; the low 6 bits interpolate along the derivative, with
 * 0x6487e = 2 pi in 16.16. Shifts as in the original, which drop bits. */
static int32_t interpolate(int32_t base, int32_t slope, uint32_t angle)
{
    const int32_t frac = (int32_t)((angle & 0x3F) * 0x6487Eu) >> 9;
    return base + ((int32_t)((uint32_t)(slope >> 2) * (uint32_t)frac) >> 21);
}

int32_t fx_sin16(int32_t angle)
{
    const uint32_t a10 = (uint32_t)(angle >> 6);
    return interpolate(fx_sin(a10), fx_cos(a10), (uint32_t)angle);
}

int32_t fx_cos16(int32_t angle)
{
    const uint32_t a10 = (uint32_t)(angle >> 6);
    return interpolate(fx_cos(a10), -fx_sin(a10), (uint32_t)angle);
}

/* 00475d4c: out = a * b, each product shifted down before the sum. */
void mat3_mul(const int32_t a[9], const int32_t b[9], int32_t out[9])
{
    int32_t r[9];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            r[i * 3 + j] = fx_mul(a[i * 3], b[j]) + fx_mul(a[i * 3 + 1], b[3 + j]) + fx_mul(a[i * 3 + 2], b[6 + j]);
    for (int i = 0; i < 9; i++) out[i] = r[i];
}

/* 00475da8: row vector times matrix, the three products summed in 64 bits. */
void mat3_apply(const int32_t v[3], const int32_t m[9], int32_t out[3])
{
    int32_t r[3];
    for (int j = 0; j < 3; j++) {
        const int64_t sum = (int64_t)v[0] * m[j] + (int64_t)v[1] * m[3 + j] + (int64_t)v[2] * m[6 + j];
        r[j] = (int32_t)(uint32_t)((uint64_t)sum >> 16);
    }
    out[0] = r[0], out[1] = r[1], out[2] = r[2];
}

/* imul + shrd 16: the low 32 bits of the product shifted right (floor). */
int32_t fx_mul(int32_t a, int32_t b)
{
    return (int32_t)(uint32_t)((uint64_t)((int64_t)a * b) >> 16);
}
