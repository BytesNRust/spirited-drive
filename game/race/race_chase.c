/* The camera that follows a car (race_chase.h). */
#include "game/race/race_chase.h"

#include "game/sys/fixed_math.h"
#include "game/sys/rigid_math.h"

enum {
    SLOT2_FLAGS = 0x42400,      /* flags_174 bits 0043f8f8 looks for */
    NO_SECOND_CAR = 9           /* entries[1].car: 0043f948 never true */
};

static int32_t sub(int32_t a, int32_t b)
{
    return (int32_t)((uint32_t)a - (uint32_t)b);
}

void race_chase_glide(RaceChase *chase, const int32_t to[3], int32_t divide, int32_t ticks)
{
    if (ticks == 0) return;
    for (int i = 0; i < 3; i++) {
        const int32_t gap = sub(to[i], chase->target[i]);
        if (divide == 0) chase->step[i] = gap >> (ticks & 31);
        else if (ticks == -1 && gap == INT32_MIN) chase->step[i] = gap;  /* port guard: idiv faults there */
        else chase->step[i] = gap / ticks;
    }
}

int32_t race_chase_distance(const int32_t a[3], const int32_t b[3])
{
    const int32_t dx = sub(a[0], b[0]), dz = sub(a[2], b[2]);
    const int32_t sum = (int32_t)((uint32_t)fx_mul_round(dx, dx) + (uint32_t)fx_mul_round(dz, dz));
    return rm_sqrt(sum, 8, sum);
}

static int32_t neg(int32_t a)
{
    return (int32_t)(0u - (uint32_t)a);
}

void race_chase_unturn(const int32_t from[3], int32_t out[3], int32_t pitch, int32_t roll, int32_t yaw)
{
    int32_t x[9], z[9], y[9], xz[9], m[9];
    mat3_rotate_x(x, neg(pitch));
    mat3_rotate_z(z, neg(roll));
    mat3_rotate_y(y, neg(yaw));
    mat3_mul(x, z, xz);
    mat3_mul(xz, y, m);
    const int32_t v[3] = { from[0] >> 8, from[1] >> 8, from[2] >> 8 };
    mat3_apply(v, m, out);
    for (int i = 0; i < 3; i++) out[i] = (int32_t)((uint32_t)out[i] << 8);
}

/* port guard: a car slot outside the table or empty is none */
static const Car *slot_car(const RaceState *race, int32_t slot)
{
    return slot >= 0 && slot < RACE_PLAYERS ? race->cars[slot] : NULL;
}

static int ahead_by(const RaceState *race, const Car *other, int32_t after, int32_t upto)
{
    const Car *car = slot_car(race, race->player);
    if (car == NULL || other == NULL) return 0;
    const int32_t gap = sub(car->node, other->node);
    return after < gap && gap <= upto;
}

int race_chase_ahead_of_slot2(const RaceState *race, int32_t after, int32_t upto)
{
    const Car *other = slot_car(race, 2);
    if (race->unknown_4fbab8 == 0 || other == NULL) return 0;
    /* the word at +0x174: flags_174..flags_176[1] */
    const uint32_t flags = (uint32_t)other->flags_174 | (uint32_t)other->flags_175 << 8 |
                           (uint32_t)other->flags_176[0] << 16 | (uint32_t)other->flags_176[1] << 24;
    if ((flags & SLOT2_FLAGS) == 0) return 0;
    return ahead_by(race, other, after, upto);
}

int race_chase_ahead_of_other(const RaceState *race, int32_t after, int32_t upto)
{
    if (race->entries[1].car == NO_SECOND_CAR) return 0;
    return ahead_by(race, slot_car(race, (int32_t)(1u - (uint32_t)race->player)), after, upto);
}

int32_t race_chase_road_height(const RaceState *race, int32_t node, const int32_t point[3])
{
    const TriNode here = race_node(race, node), next = race_node(race, (int32_t)((uint32_t)node + 1u));
    const int32_t at[3] = { here.position.x, here.position.y, here.position.z };
    const int32_t to[3] = { next.position.x, next.position.y, next.position.z };
    const int32_t side_at[3] = { at[0] + here.z_orientation * 2, at[1] + here.slant_b * 2, at[2] + here.x_orientation * 2 };
    int32_t along[3], side[3], normal[3];
    for (int i = 0; i < 3; i++) {
        along[i] = sub(to[i], at[i]);
        side[i] = sub(side_at[i], at[i]);
    }
    fx_cross(along, side, normal);
    const int32_t rise = (int32_t)(0u - (uint32_t)fx_mul_round(normal[2], sub(point[2], to[2])) -
                                   (uint32_t)fx_mul_round(normal[0], sub(point[0], to[0])));
    if (normal[1] == 0 || (normal[1] == -1 && rise == INT32_MIN)) return to[1];  /* port guard: idiv faults */
    return (int32_t)((uint32_t)fx_div(rise, normal[1]) + (uint32_t)to[1]);
}
