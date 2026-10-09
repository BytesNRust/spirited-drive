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

/* imul + shrd 16 + adc 0: the low 32 bits of the product shifted right,
 * plus bit 15 (round half up). */
int32_t fx_mul_round(int32_t a, int32_t b)
{
    const uint64_t p = (uint64_t)((int64_t)a * b);
    return (int32_t)((uint32_t)(p >> 16) + (uint32_t)((p >> 15) & 1));
}

/* 004f4074: atan over [0, 1] in 256 steps, 16-bit angles. */
static const uint16_t ATAN_TABLE[257] = {
    0, 41, 81, 122, 163, 204, 244, 285, 326, 367, 407, 448,
    489, 529, 570, 610, 651, 692, 732, 773, 813, 854, 894, 935,
    975, 1015, 1056, 1096, 1136, 1177, 1217, 1257, 1297, 1337, 1377, 1417,
    1457, 1497, 1537, 1577, 1617, 1656, 1696, 1736, 1775, 1815, 1854, 1894,
    1933, 1973, 2012, 2051, 2090, 2129, 2168, 2207, 2246, 2285, 2324, 2363,
    2401, 2440, 2478, 2517, 2555, 2594, 2632, 2670, 2708, 2746, 2784, 2822,
    2860, 2897, 2935, 2973, 3010, 3047, 3085, 3122, 3159, 3196, 3233, 3270,
    3307, 3344, 3380, 3417, 3453, 3490, 3526, 3562, 3599, 3635, 3670, 3706,
    3742, 3778, 3813, 3849, 3884, 3920, 3955, 3990, 4025, 4060, 4095, 4129,
    4164, 4199, 4233, 4267, 4302, 4336, 4370, 4404, 4438, 4471, 4505, 4539,
    4572, 4605, 4639, 4672, 4705, 4738, 4771, 4803, 4836, 4869, 4901, 4933,
    4966, 4998, 5030, 5062, 5094, 5125, 5157, 5188, 5220, 5251, 5282, 5313,
    5344, 5375, 5406, 5437, 5467, 5498, 5528, 5559, 5589, 5619, 5649, 5679,
    5708, 5738, 5768, 5797, 5826, 5856, 5885, 5914, 5943, 5972, 6000, 6029,
    6058, 6086, 6114, 6142, 6171, 6199, 6227, 6254, 6282, 6310, 6337, 6365,
    6392, 6419, 6446, 6473, 6500, 6527, 6554, 6580, 6607, 6633, 6660, 6686,
    6712, 6738, 6764, 6790, 6815, 6841, 6867, 6892, 6917, 6943, 6968, 6993,
    7018, 7043, 7068, 7092, 7117, 7141, 7166, 7190, 7214, 7238, 7262, 7286,
    7310, 7334, 7358, 7381, 7405, 7428, 7451, 7475, 7498, 7521, 7544, 7566,
    7589, 7612, 7635, 7657, 7679, 7702, 7724, 7746, 7768, 7790, 7812, 7834,
    7856, 7877, 7899, 7920, 7942, 7963, 7984, 8005, 8026, 8047, 8068, 8089,
    8110, 8131, 8151, 8172, 8192
};

/* 004890f0: atan2 of (y, x) in 16-bit angles (0x10000 per turn), from the
 * ratio of the smaller to the larger magnitude as a 0.32 fraction (004a1704),
 * interpolated in the table, then moved to its octant. */
int32_t fx_atan2(int32_t y, int32_t x)
{
    unsigned octant = 0;
    uint32_t a = (uint32_t)y, b = (uint32_t)x;
    if (x < 0) octant |= 2, b = 0u - b;
    if (y < 0) octant |= 4, a = 0u - a;
    int32_t angle;
    if (a == b) {
        angle = 0x2000;
    } else {
        if (b < a) {
            const uint32_t t = a;
            octant |= 1, a = b, b = t;
        }
        const uint32_t ratio = (uint32_t)(((uint64_t)a << 32) / b);
        const uint32_t i = ratio >> 24, frac = (ratio >> 8) & 0xFFFF;
        angle = (int32_t)(ATAN_TABLE[i] + (((uint32_t)(ATAN_TABLE[i + 1] - ATAN_TABLE[i]) * frac) >> 16));
    }
    switch (octant) {
    case 1: return 0x4000 - angle;
    case 2: return 0x8000 - angle;
    case 3: return angle + 0x4000;
    case 4: return -angle;
    case 5: return angle - 0x4000;
    case 6: return angle - 0x8000;
    case 7: return -angle - 0x4000;
    default: return angle;
    }
}

/* The 16.16 division the 3D code inlines: idiv, then the remainder << 16. */
int32_t fx_div(int32_t a, int32_t b)
{
    if (b == 0) return 0;
    const int32_t q = a / b, r = a % b;
    return (int32_t)((uint32_t)q << 16) + (int32_t)(((int64_t)r << 16) / b);
}

/* 004f3e9c: atan over [0, 1] in 256 steps, 10-bit angles (0x80 at 1). */
static const uint8_t ATAN8_TABLE[257] = {
    0, 1, 1, 2, 3, 3, 4, 4, 5, 6, 6, 7, 8, 8, 9, 10,
    10, 11, 11, 12, 13, 13, 14, 15, 15, 16, 16, 17, 18, 18, 19, 20,
    20, 21, 22, 22, 23, 23, 24, 25, 25, 26, 27, 27, 28, 28, 29, 30,
    30, 31, 31, 32, 33, 33, 34, 34, 35, 36, 36, 37, 38, 38, 39, 39,
    40, 41, 41, 42, 42, 43, 44, 44, 45, 45, 46, 46, 47, 48, 48, 49,
    49, 50, 51, 51, 52, 52, 53, 53, 54, 55, 55, 56, 56, 57, 57, 58,
    58, 59, 60, 60, 61, 61, 62, 62, 63, 63, 64, 65, 65, 66, 66, 67,
    67, 68, 68, 69, 69, 70, 70, 71, 71, 72, 72, 73, 74, 74, 75, 75,
    76, 76, 77, 77, 78, 78, 79, 79, 80, 80, 81, 81, 82, 82, 83, 83,
    84, 84, 84, 85, 85, 86, 86, 87, 87, 88, 88, 89, 89, 90, 90, 91,
    91, 91, 92, 92, 93, 93, 94, 94, 95, 95, 96, 96, 96, 97, 97, 98,
    98, 99, 99, 99, 100, 100, 101, 101, 102, 102, 102, 103, 103, 104, 104, 104,
    105, 105, 106, 106, 106, 107, 107, 108, 108, 108, 109, 109, 110, 110, 110, 111,
    111, 112, 112, 112, 113, 113, 113, 114, 114, 115, 115, 115, 116, 116, 116, 117,
    117, 118, 118, 118, 119, 119, 119, 120, 120, 120, 121, 121, 121, 122, 122, 122,
    123, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127, 127, 127, 128,
    128
};

/* 00484294: the octant of (y, x) by signs and which is larger, the ratio of
 * the smaller to the larger as a 32-bit fraction rounded to 8 bits (the bit
 * shifted out is added back), then the table. */
int32_t fx_atan2_10(int32_t y, int32_t x)
{
    unsigned octant = 0;
    uint32_t a = (uint32_t)y, b = (uint32_t)x;
    if (x < 0) octant |= 8, b = 0u - b;
    if (y < 0) octant |= 0x10, a = 0u - a;
    int32_t t;
    if (a == b) {
        t = 0x80;
    } else {
        if (a > b) {
            const uint32_t swap = a;
            a = b, b = swap;
            octant |= 4;
        }
        const uint32_t ratio = (uint32_t)(((uint64_t)a << 32) / b);
        t = ATAN8_TABLE[(ratio >> 24) + ((ratio >> 23) & 1)];
    }
    switch (octant) {
    case 0x04: return 0x100 - t;
    case 0x08: return 0x200 - t;
    case 0x0c: return t + 0x100;
    case 0x10: return -t;
    case 0x14: return t - 0x100;
    case 0x18: return t - 0x200;
    case 0x1c: return -t - 0x100;
    default: return t;
    }
}

/* 0042ece8 */
void fx_rotate(int32_t x, int32_t z, int32_t angle, int32_t *out_x, int32_t *out_z)
{
    const int32_t c = fx_cos16(angle >> 8), s = fx_sin16(angle >> 8);
    *out_x = (int32_t)((uint32_t)fx_mul_round(c, x) - (uint32_t)fx_mul_round(s, z));
    *out_z = (int32_t)((uint32_t)fx_mul_round(s, x) + (uint32_t)fx_mul_round(c, z));
}

/* 0042ec50 */
void fx_cross(const int32_t a[3], const int32_t b[3], int32_t out[3])
{
    out[0] = (int32_t)((uint32_t)fx_mul_round(a[1], b[2]) - (uint32_t)fx_mul_round(a[2], b[1]));
    out[1] = (int32_t)((uint32_t)fx_mul_round(a[2], b[0]) - (uint32_t)fx_mul_round(a[0], b[2]));
    out[2] = (int32_t)((uint32_t)fx_mul_round(a[0], b[1]) - (uint32_t)fx_mul_round(a[1], b[0]));
}
