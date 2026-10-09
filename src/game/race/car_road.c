#include "game/race/car_road.h"

#include "game/sys/fixed_math.h"

/* 00428804 (yaw +0x10) and 0042888c (heading +0x351): the half length and
 * half width of the car turned to the world, flat (their y comes from the
 * road plane, 004286c0). */
static void body_axes(const Car *car, int32_t angle, int32_t forward[3], int32_t side[3])
{
    fx_rotate(0, car->half_length, -angle, &forward[0], &forward[2]);
    forward[1] = 0;
    fx_rotate(-car->half_width, 0, -angle, &side[0], &side[2]);
    side[1] = 0;
}

/* 004286c0: the plane through node, node + lateral and next. forward and side
 * get the rise the plane gives them, point the plane's height at its x, z.
 * The normal's y is inverted in two divisions to 16.16 (a zero y would fault
 * in the original; a flat-out vertical road never occurs). */
static void road_plane(int32_t forward[3], int32_t side[3], int32_t point[3], const int32_t node[3],
                       const int32_t lateral[3], const int32_t next[3])
{
    const int32_t u[3] = { node[0] - lateral[0], node[1] - lateral[1], node[2] - lateral[2] };
    const int32_t v[3] = { next[0] - lateral[0], next[1] - lateral[1], next[2] - lateral[2] };
    int32_t n[3];
    fx_cross(u, v, n);
    if (n[1] == 0) return;
    const int32_t whole = 0x10000 / n[1], rest = 0x10000 % n[1];
    const int32_t inverse = (int32_t)(((int64_t)rest << 16) / n[1]) + (int32_t)((uint32_t)whole << 16);
    const int32_t nx = n[0] >> 8, nz = n[2] >> 8, inv = inverse >> 8;
    forward[1] = ((-(forward[0] >> 8) * nx - (forward[2] >> 8) * nz) >> 8) * inv;
    side[1] = ((-(side[0] >> 8) * nx - (side[2] >> 8) * nz) >> 8) * inv;
    point[1] = node[1] + ((-((point[0] - node[0]) >> 8) * nx - ((point[2] - node[2]) >> 8) * nz) >> 8) * inv;
}

void car_road_contact(Car *car, const RaceState *race, int use_heading)
{
    int32_t forward[3], side[3];
    body_axes(car, use_heading ? car->heading : car->yaw, forward, side);
    int32_t point[3] = { car->x, car->y, car->z };
    const TriNode a = race_node(race, car->node), b = race_node(race, car->node + 1);
    const int32_t node[3] = { a.position.x, a.position.y, a.position.z };
    /* +0x1c, +0x1e, +0x20: the road's sideways vector, doubled */
    const int32_t lateral[3] = { node[0] + a.z_orientation * 2, node[1] + a.slant_b * 2,
                                 node[2] + a.x_orientation * 2 };
    const int32_t next[3] = { b.position.x, b.position.y, b.position.z };
    road_plane(forward, side, point, node, lateral, next);
    for (int i = 0; i < 3; i++) {
        car->road_forward[i] = forward[i];
        car->road_side[i] = side[i];
        if (use_heading) {
            car->road_point_heading[i] = point[i];
        } else {
            car->road_point[i] = point[i];
        }
    }
}

void car_lateral(Car *car, const RaceState *race)
{
    const TriNode n = race_node(race, car->node);
    const int32_t angle = n.orientation * 4;
    const int32_t across = fx_mul_round(fx_sin16(angle), n.position.x - car->x);
    car->lateral = -across - fx_mul_round(fx_cos16(angle), n.position.z - car->z);
}
