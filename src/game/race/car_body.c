#include "game/race/car_body.h"

#include "formats/qfs.h"
#include "game/sys/fixed_math.h"
#include "game/sys/rigid_math.h"

#include <stdlib.h>
#include <string.h>

enum { ONE = 0x10000 };

int car_pdn_read(CarPdn *pdn, const uint8_t *file, size_t size)
{
    size_t unpacked = 0;
    if (!qfs_is_compressed(file, size)) {
        if (size < sizeof *pdn) return 0;
        memcpy(pdn, file, sizeof *pdn);
        return 1;
    }
    if (qfs_decompressed_size(file, size, &unpacked) != QFS_OK || unpacked < sizeof *pdn) return 0;
    uint8_t *data = malloc(unpacked);
    const int ok = data != NULL && qfs_decompress(file, size, data, unpacked) == QFS_OK;
    if (ok) memcpy(pdn, data, sizeof *pdn);
    free(data);
    return ok;
}

/* 0042ebcc: 1 / x for an unsigned 16.16 x, in x87 doubles truncated;
 * exactly 2^32 / x. Past 32 bits (x below 2) fistp gives 0x80000000. */
static int32_t inverse(int32_t x)
{
    const uint32_t u = (uint32_t)x;
    if (u < 2) return INT32_MIN;
    return (int32_t)(uint32_t)(((uint64_t)1 << 32) / u);
}

/* 0041da00: the body at rest with its inertia and size. */
static void body_reset(Car *car, int32_t a, int32_t b, const int32_t extents[3])
{
    car->body_pdn_10 = a;
    car->body_inverse_10 = inverse(a);
    car->body_pdn_0c = b;
    car->body_inverse_0c = inverse(b);
    car->body_extent_a = extents[0];
    car->body_extent_b = extents[1];
    car->body_extent_c = extents[2];
    int32_t m[9];
    rm_identity(m);
    memcpy(car->body_matrix, m, sizeof m);
    memset(car->body_position, 0, sizeof car->body_position);
    memset(car->body_velocity, 0, sizeof car->body_velocity);
    memset(car->body_spin, 0, sizeof car->body_spin);
    memset(car->body_force, 0, sizeof car->body_force);
    memset(car->body_unknown_54, 0, sizeof car->body_unknown_54);
    car->ai_countdown = 0;
}

static int32_t body_radius(const Car *car)
{
    const int32_t e[3] = { car->body_extent_a, car->body_extent_b, car->body_extent_c };
    return rm_length(e);
}

/* 0040fa3c */
static void body_from_pdn(Car *car, const CarPdn *pdn)
{
    body_reset(car, pdn->unknown_10, pdn->unknown_0c, pdn->extents);
    car->body_radius = body_radius(car);
    car->speed_factor = 0;
    car->pdn_mass = pdn->mass;
    car->pdn_inverse_mass = fx_div(ONE, pdn->mass);
    car->pdn_18 = pdn->unknown_18;
    for (int i = 0; i < CAR_PDN_FACTORS; i++) {
        car->pdn_1ac[i] = pdn->factors[i];
        car->pdn_1ac_inverse[i] = pdn->factors[i] != 0 ? fx_div(ONE, pdn->factors[i]) : 0;
    }
    car->pdn_1c4 = pdn->unknown_1c4;
    car->pdn_1c8 = pdn->unknown_1c8;
    memcpy(car->ai_speed_table, pdn->speeds, sizeof car->ai_speed_table);
    car->unknown_0bc = car->body_extent_b;
    car->unknown_084 = car->body_extent_a;
    car->road_lateral = 0;
    car->ai_countdown = 0;
    car->unknown_148 = 0;
    car->slide_timer = 0;
    car->coast_timer = 0;
    int32_t m[9];
    rm_identity(m);
    memcpy(car->matrix, m, sizeof m);
}

void car_body_init(Car *car, const CarPdn *pdn)
{
    body_from_pdn(car, pdn);
    if (car->body_extent_a != 0 && car->body_extent_b != 0 && car->body_extent_c != 0) {
        car->half_length = car->body_extent_c * 2;
        car->half_width = car->body_extent_a * 2;
        return;
    }
    car->half_length = car->model_length;
    car->half_width = car->model_width;
    car->body_extent_a = car->model_width >> 1;
    car->body_extent_b = car->model_height >> 1;
    car->body_extent_c = car->model_length >> 1;
    car->unknown_0bc = car->body_extent_b;
    car->body_radius = body_radius(car);
    car->unknown_084 = car->body_extent_a;
}
