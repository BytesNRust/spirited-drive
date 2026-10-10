#include "game/race/car_walls.h"

#include "game/sys/fixed_math.h"

enum {
    VERGE_UNIT = 0x2000,        /* node verge and barrier bytes, world 16.16 */
    WALL_GAP = 0x4ccc,
    HARD_HIT = 0x60000,
    NOTED_HIT = 0x140000,
    SOFT_HIT = 0x180000,
    MAX_PUSH_BACK = 0x20000,
    SLIDE_SPEED = 0x30000,
    MAX_SHAKE = 0xa0000,
    BOUNCE_TURN = 0x20000,
    SUSPENSION_KICK = 0x1e
};

static int32_t abs32(int32_t v)
{
    return v > 0 ? v : -v;
}

/* 004305a8: how far the car reaches sideways, from its length and width
 * by its angle to the road. */
static int32_t reach(const Car *car, int32_t road_heading)
{
    int32_t d = abs32(road_heading - car->yaw) >> 16;
    if (d > 0xc0) {
        d = 0x100 - d;
    } else if (d > 0x80) {
        d -= 0x80;
    } else if (d > 0x40) {
        d = 0x80 - d;
    }
    return (((d * (car->half_length - car->half_width)) >> 6) + car->half_width) >> 1;
}

void race_sound_position(RaceState *race, const int32_t position[3], int32_t *angle, int32_t *distance)
{
    const int32_t dx = position[0] - race->listener[0], dz = position[2] - race->listener[2];
    const int32_t dy = abs32(position[1] - race->listener[1]);
    if (race->listener_heading < 0) race->listener_heading += 0x1000000;
    *angle = race->listener_heading - (fx_atan2(dx, dz) << 8);
    if (*angle < 0) *angle += 0x1000000;
    if (*angle > 0x1000000) *angle -= 0x1000000;
    const int32_t ax = abs32(dx), az = abs32(dz);
    const int32_t flat = az < ax ? (az >> 2) + ax : az + (ax >> 2);
    *distance = flat < dy ? (flat >> 2) + dy : flat + (dy >> 2);
}

static void scrape_tone(const Car *car, const RaceState *race, int right)
{
    (void)car;
    if (race->sounds.tone != NULL) race->sounds.tone(race->sounds.user, 0x46, right ? 0x5a : 0x10e, 0xb4);
}

/* 004302a4: a hard hit at speed spins the car off the wall through the crash
 * physics; a glancing one turns it along the wall. */
static void glance(Car *car, RaceState *race, int32_t heading, int32_t hit, int32_t side, int32_t kind,
                   int32_t turn)
{
    heading += side > 0 ? -turn : turn;
    int32_t off = heading - car->yaw;
    if (off < 0) off += 0x1000000;
    int32_t size = off > 0x800000 ? 0x1000000 - off : off;
    if (size > 0x400000) {
        off += 0x800000;  /* the original folds the angle itself here too */
        size = 0x1000000 - off;
    }
    if (off > 0x1000000) off -= 0x1000000;
    const int crash = race->arcade == 0 ? hit >= 0x180000 && car->speed > 0x230000
                                        : hit >= 0x220000 && car->speed > 0x2d0000;
    if (crash) {
        hit -= 0x60000;
        off = off < 0x800000 ? off / 8 : 0x1000000 - (0x1000000 - off) / 8;
        if (off > 0x800000) off -= 0x1000000;
        if (hit > 0xe0000) hit = 0xe0000;
        const int32_t spin = (size >> 16) * hit / 64;
        int32_t y = hit - spin;
        if (off > 0) {
            off = y;
        } else {
            hit = -hit;
            off = -y;
        }
        if (kind == 0) {
            off = -off;
            race->crash_spin = spin;
        } else {
            race->crash_spin = -spin;
        }
        if (hit != 0 && race->sounds.crash != NULL)
            race->sounds.crash(race->sounds.user, car->unknown_088, hit / 4, off, race->crash_spin * 3 / 4);
        return;
    }
    if (abs32(size) < 0x130000 && car->local_velocity_z > 0) {
        if (side > 0 ? off > 0xed0000 : off < 0x130000) car->yaw += off;
    }
}

/* The hit's sound, panned by where it is when several views listen. */
static void hit_sound(const Car *car, RaceState *race, int32_t across, int32_t hit)
{
    if (race->viewed != car->player || race->sounds.sound == NULL) return;
    int32_t pan, volume;
    if (race->views <= 1) {
        pan = across > 0 ? 0xd70000 : 0x280000;
        volume = 1;
    } else {
        const int32_t at[3] = { car->x, car->y, car->z };
        race_sound_position(race, at, &pan, &volume);
    }
    race->sounds.sound(race->sounds.user, -1, 2, 9, hit * 20, volume, pan);
}

/* The speed along the wall drops with the hit. */
static int32_t scrub(const RaceState *race, int32_t along, int32_t hit)
{
    const int32_t half = hit / 2, quarter = hit / 4;
    if (along > 0) {
        if (along > SLIDE_SPEED) {
            along = race->arcade ? along - half - hit / 8 : along - hit - quarter;
            if (along < 0) along = 0;
        }
    } else if (along < -SLIDE_SPEED) {
        along = race->arcade ? along + half : along + hit + quarter;
        if (along > 0) along = 0;
    }
    return along;
}

static void bounce(Car *car, RaceState *race, int32_t heading, int32_t across, int32_t depth, int32_t side,
                   int32_t kind)
{
    const int32_t push = side * abs32(depth) / 256;
    int32_t dx, dz;
    fx_rotate(push, 0, heading, &dx, &dz);
    car->x -= dx;
    car->z += dz;
    int32_t into, along;
    fx_rotate(car->velocity_x, car->velocity_z, -heading, &into, &along);
    const int32_t hit = abs32(into);
    if (kind == 0 && hit > HARD_HIT) {
        hit_sound(car, race, across, hit);
        if (hit > NOTED_HIT) race_note(race, 0x32);
    }
    if (hit < SOFT_HIT) {
        if (into > MAX_PUSH_BACK) {
            into = MAX_PUSH_BACK;
        } else if (into < -MAX_PUSH_BACK) {
            into = -MAX_PUSH_BACK;
        }
    } else {
        into /= 4;
    }
    along = scrub(race, along, hit);
    int32_t shake = hit / 4;
    if (shake > MAX_SHAKE) shake = MAX_SHAKE;
    if (side > 0) {
        car->suspension_z = shake;
        car->suspension_x = -shake;
        heading -= BOUNCE_TURN;
    } else {
        car->suspension_z = -shake;
        car->suspension_x = -shake;
        heading += BOUNCE_TURN;
    }
    car->suspension_z_index = SUSPENSION_KICK;
    car->suspension_x_index = SUSPENSION_KICK;
    int32_t vx, vz;
    fx_rotate(into, along, heading, &vx, &vz);
    car->velocity_x = vx;
    car->velocity_z = vz;
    glance(car, race, heading, hit, side, kind, kind == 0 ? 0x30000 : 0x18000);
    if (car->velocity_y > 0) car->velocity_y = 0;
}

void car_walls(Car *car, RaceState *race)
{
    const TriNode n = race_node(race, car->node);
    const int32_t heading = n.orientation * 0x400;
    const int32_t across = ((n.position.z - car->z) >> 8) * (fx_sin16(heading >> 8) >> 8) -
                           (fx_cos16(heading >> 8) >> 8) * ((n.position.x - car->x) >> 8);
    const uint8_t *edges = &n.left_verge;  /* +0..+3: verges, then barriers */
    int32_t side = 0, depth = 0, kind = 0;
    if (-(n.left_verge * VERGE_UNIT) > across) {
        car->surface = n.flags[2] >> 4 & 0xf;
        const int32_t wall = reach(car, heading) - n.left_barrier * VERGE_UNIT;
        if (wall > across) {
            side = -0x100;
            depth = wall + WALL_GAP - across;
            kind = n.flags[1] >> 4 & 0xf;
            if (race->effects != 0 && race->viewed == car->player) scrape_tone(car, race, 0);
        }
    } else if (n.right_verge * VERGE_UNIT < across) {
        car->surface = n.flags[2] & 0xf;
        const int32_t wall = n.right_barrier * VERGE_UNIT - reach(car, heading);
        if (wall < across) {
            side = 0x100;
            depth = across - (wall - WALL_GAP);
            kind = n.flags[1] & 0xf;
            if (race->effects != 0 && race->viewed == car->player) scrape_tone(car, race, 1);
        }
    } else {
        car->surface = race->default_surface;
    }
    (void)edges;
    if (side != 0) bounce(car, race, heading, across, depth, side, kind);
}
