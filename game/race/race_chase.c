/* The camera that follows a car (race_chase.h). */
#include "game/race/race_chase.h"

#include "game/race/car_draw.h"
#include "game/race/car_progress.h"
#include "game/race/race_camera.h"
#include "game/race/race_follow.h"
#include "game/sys/fixed_math.h"
#include "game/sys/rigid_math.h"

#include <string.h>

enum {
    SLOT2_FLAGS = 0x42400,      /* flags_174 bits 0043f8f8 looks for */
    NO_SECOND_CAR = 9,          /* entries[1].car: 0043f948 never true */
    KIND_NO_ROAD = 13,          /* the car does not face the road (00405de8) */
    KIND_STILL = 9,             /* the eye stays at the row's target */
    KIND_PASSED = 6,            /* the other player's car just behind */
    KIND_PASSED_COP = 7,        /* slot 2's car just behind */
    KIND_REPLAY = 10,
    KIND_ALT = 8,               /* takes alt_target whatever the last kind */
    CAMERA2 = 2,                /* 004b78e8 */
    LOOK_NODES = 12,            /* the road's direction from 12 nodes either side */
    NEAR_NODES = 0x10,          /* the eye jumps to the car beyond this many nodes */
    BLEND_FULL = 0x600,         /* car +0x39d >> 8 against the two node spans */
    HALF_TURN = 0x800000,       /* 24-bit angles */
    FULL_TURN = 0x1000000,
    HEADING_SCALE = 0x400,      /* node heading (0x4000 a turn) to 24 bits */
    VERGE_UNIT = 0x2000,        /* node verge and barrier bytes to 16.16 */
    GLIDE_NEAR = 0x1999,        /* a dividing glide ends this close in z */
    ROAD_CLEARANCE = 0x60000,   /* the eye stays this far above the road at physics 4 */
    PHYSICS_ROAD = 4,
    AIM_MASK = 0xfffc00
};

/* 004b6bb0 and 004b78a8 as the exe holds them */
static const ChaseKind KINDS[CHASE_KINDS] = {
    { 0, { -0x6666, 0xe666, 0 }, YAW_UNSET, 0x10000, 0, { 1, 1, 1, 0 }, 0, 0, 0, 1, 0, -1, 0, { 0, 0, 0 }, 0, NULL },
    { 1, { 0, 0xe666, 0 }, YAW_UNSET, 0x140000, 0, { 2, 2, 2, 0 }, 3, 0, 0, 0, 32, -1, 0, { 0, 0, 0 }, 0, NULL },
    { 1, { 0, 0x1b333, -0x40000 }, YAW_UNSET, 0x140000, 3, { 3, 3, 3, 1 }, 3, 0, 0, 0, 32, -1, 0, { 0, 0x14ccc, -0x2b333 }, 1, NULL },
    { 1, { 0, 0x2cccc, -0x63333 }, YAW_UNSET, 0x140000, 3, { 4, 4, 10, 0 }, 3, 0, 0, 0, 32, -1, 0, { 0, 0, 0 }, 0, NULL },
    { 1, { 0, 0x40000, -0x100000 }, YAW_UNSET, 0x140000, 1, { 5, 5, 10, 1 }, 5, 0, 0, 0, 32, -1, 0, { 0, 0, 0 }, 0, NULL },
    { 1, { 0, 0x3cccc, -0x80000 }, YAW_UNSET, 0xc0000, 1, { 11, 6, 10, 1 }, 5, 0, 0, 0, 32, -1, 0, { 0, 0, 0 }, 0, NULL },
    { 1, { 0, 0x3cccc, -0x80000 }, YAW_UNSET, 0x140000, 1, { 7, 7, 10, 1 }, 5, 0, 0, 0, 32, -1, 0, { 0, 0x18000, -0x30000 }, 1, NULL },
    { 1, { 0, 0x30000, -0x63333 }, YAW_UNSET, 0xc0000, 1, { 11, 10, 10, 1 }, 5, 0, 0, 0, 32, -1, 0, { 0, 0x18000, -0x30000 }, 1, NULL },
    { 1, { -0x6666, 0xe666, 0 }, YAW_UNSET, 0x140000, 4, { 1, 0, 0, 0 }, 5, 0, 0, 0, 90, 0, -0x38000, { 0, 0x90000, -0x260000 }, 1, NULL },
    { 1, { 0, 0x30000, -0xc0000 }, YAW_UNSET, 0x10000, 0, { 11, 10, 10, 0 }, 0, 0, 0, 1, 32, -1, 0, { 0, 0, 0 }, 0, NULL },
    { 1, { -0x6666, 0xe666, 0 }, YAW_UNSET, 0x140000, 3, { 1, 0, 0, 0 }, 4, 0, 0, 0, 40, 0, -0x38000, { 0, 0, 0 }, 0, NULL },
    { 1, { -0x6666, 0xe666, 0 }, YAW_UNSET, 0x140000, 3, { 1, 0, 0, 0 }, 4, 0, 0, 0, 40, 1, -0x38000, { 0, 0, 0 }, 0, NULL },
    { 1, { 0, 0x60000, -0x100000 }, YAW_UNSET, 0xc0000, 4, { 11, 10, 10, 1 }, 5, 0, 0, 0, 32, -1, 0, { 0, 0x18000, -0x30000 }, 1, NULL },
    { 1, { 0, 0x2cccc, -0xa0000 }, YAW_UNSET, 0xc0000, 4, { 11, 10, 10, 1 }, 4, 0, 0, 0, 40, -1, 0, { 0, 0, 0 }, 0, NULL },
};

static const RaceCamera CAMERAS[RACE_CAMERAS] = {
    { 85, 0x10000, 0, 0, 0, 0, NULL, NULL },
    { 96, 0x10000, 1, 1, 1, 0, NULL, NULL },
    { 70, 0x8000, 4, 4, 4, 0, NULL, NULL }
};

static int32_t sub(int32_t a, int32_t b)
{
    return (int32_t)((uint32_t)a - (uint32_t)b);
}

static int32_t add(int32_t a, int32_t b)
{
    return (int32_t)((uint32_t)a + (uint32_t)b);
}

static int32_t mul(int32_t a, int32_t b)
{
    return (int32_t)((uint32_t)a * (uint32_t)b);
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
    return slot >= 0 && slot < RACE_CARS ? race->cars[slot] : NULL;
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

void race_chase_init(RaceState *race)
{
    memcpy(race->chase_kinds, KINDS, sizeof KINDS);
    memcpy(race->cameras, CAMERAS, sizeof CAMERAS);
}

/* 0042ebbc(eax, edx) */
static int32_t angle_of(int32_t eax, int32_t edx)
{
    return (int32_t)((uint32_t)fx_atan2(edx, eax) << 8);
}

/* port guard: no car does not face the road */
static int faces_road(const RaceState *race, const Car *car)
{
    return car != NULL && camera_faces_road(car, race);
}

static int32_t node_heading(const RaceState *race, int32_t node)
{
    return race_node(race, node).orientation * HEADING_SCALE;
}

void race_chase_road_yaw(const RaceState *race, RaceChase *chase) { chase->yaw = node_heading(race, chase->node); }

static int32_t magnitude(int32_t v)
{
    return v < 1 ? neg(v) : v;
}

static void place_eye(RaceChase *chase)
{
    const Car *base = chase->base;
    chase->eye[0] = add(base->x, chase->offset[0]);
    chase->eye[1] = add(base->y, chase->offset[1]);
    chase->eye[2] = add(base->z, chase->offset[2]);
}

void race_chase_follow(RaceState *race, RaceChase *chase, Car *car, int32_t kind)
{
    if (car == NULL) return;
    /* 00404450 */
    chase->base = car;
    chase->eye[0] = car->x, chase->eye[1] = car->y, chase->eye[2] = car->z;
    chase->car_angles = car;
    chase->aim[0] = car->pitch, chase->aim[1] = car->yaw, chase->aim[2] = car->roll;
    memcpy(chase->angles, chase->aim, sizeof chase->angles);
    memset(chase->offset, 0, sizeof chase->offset);
    memset(chase->turn, 0, sizeof chase->turn);
    chase->node = car->node;
    chase->flags = 1;
    chase->unknown_08 = 0;
    chase->next_kind = kind;
    if (car != chase->car) chase->yaw = node_heading(race, car->node);
    chase->car = car;
    race_chase_take(race, chase, kind);
}

enum {
    KIND_OTHER = 6, KIND_POLICE = 7, KIND_ATTACHED = 5, KIND_LAST_OUTSIDE = 3, KIND_NEAR_A = 10, KIND_NEAR_B = 11,
    POLICE_SLOT = 2, AHEAD_POLICE = -1, AHEAD_OTHER = -10, AHEAD_UPTO = 12, RACING = 1
};

/* 0040594c's choice of the next kind */
int32_t race_chase_pick(RaceState *race, const RaceChase *chase)
{
    const int32_t *cycle = race->chase_kinds[chase->kind].cycle;
    if (race->phase == RACING) {
        if (race->replay == 0 && race->viewed == race->player && race->unknown_513e6c == 0) {
            if (race->port_replay_cameras == 0) return cycle[CYCLE_OWN];
            /* port: the extra takes the replay's cycle, but from 5 (whose next, 11, is a fly-in that hands over to 1)
             * the own car's 10, the fly-in that ends in the dash; a press during 11's glide goes there too */
            if (chase->kind == KIND_ATTACHED || chase->kind == KIND_NEAR_B) return KIND_NEAR_A;
            if (chase->kind == KIND_NEAR_A) return cycle[CYCLE_OWN];
        }
        return cycle[CYCLE_RACING];
    }
    if (chase->kind != KIND_POLICE && race->cops != 0 && race_chase_ahead_of_slot2(race, AHEAD_POLICE, AHEAD_UPTO) &&
        race->chase_auto != 0) {
        race->chase_auto = 0;
        return KIND_POLICE;
    }
    if (chase->kind != KIND_OTHER && race_chase_ahead_of_other(race, AHEAD_OTHER, AHEAD_UPTO) && race->chase_auto != 0 &&
        chase->kind != KIND_NEAR_A) {
        race->chase_auto = 0;
        return KIND_OTHER;
    }
    if (cycle[CYCLE_IDLE] == KIND_POLICE && race->cops == 0) return race->cops;
    return cycle[CYCLE_IDLE];
}

static Car *cycle_car(RaceState *race, int32_t slot)
{
    return slot >= 0 && slot < RACE_CARS ? race->cars[slot] : NULL;  /* port guard */
}

void race_chase_cycle(RaceState *race, RaceChase *chase)
{
    const int32_t was = chase->next_kind;
    /* port guard: a table whose kinds lead back to a 6 or 7 not ahead runs on
     * for ever in the original; the exe's own table does not */
    for (int turns = 0; turns <= CHASE_KINDS; turns++) {
        if (chase->kind < 0 || chase->kind >= CHASE_KINDS) return;  /* port guard: the original indexes unchecked */
        chase->next_kind = race_chase_pick(race, chase);
        const int32_t kind = chase->next_kind;
        Car *car;
        if (kind == KIND_OTHER) {
            if (!race_chase_ahead_of_other(race, AHEAD_OTHER, AHEAD_UPTO)) {
                chase->kind = kind;
                continue;
            }
            car = cycle_car(race, race->player == 0);
        } else if (kind == KIND_POLICE && race->unknown_4fbab8 != 0) {
            if (!race_chase_ahead_of_slot2(race, AHEAD_POLICE, AHEAD_UPTO)) {
                chase->kind = kind;
                continue;
            }
            car = cycle_car(race, POLICE_SLOT);
        } else {
            if ((kind == KIND_NEAR_A || kind == KIND_NEAR_B || kind <= KIND_LAST_OUTSIDE) && race->camera_hold != 0) {
                chase->next_kind = was;
                return;
            }
            car = cycle_car(race, race->viewed);
        }
        race_chase_follow(race, chase, car, kind);
        if (chase->next_kind == KIND_ATTACHED) race->chase_auto = 1;
        race_chase_take(race, chase, chase->next_kind);
        return;
    }
}

void race_chase_take(RaceState *race, RaceChase *chase, int32_t kind)
{
    if (chase->car == NULL) return;  /* port guard: the original reads the car unchecked */
    if ((kind < 4 || kind == KIND_REPLAY || kind == KIND_REPLAY + 1) && !faces_road(race, chase->car)) {
        kind = KIND_NO_ROAD;
        chase->yaw = node_heading(race, chase->car->node);
    }
    if (race->replay != 0 || race->unknown_513e6c != 0) {
        if (kind == 0) {
            chase->next_kind = 1;
            kind = 1;
        } else if (kind == KIND_REPLAY) {
            chase->next_kind = KIND_REPLAY + 1;
            kind = KIND_REPLAY + 1;
        }
    }
    const int32_t last = chase->kind;
    chase->kind = kind;
    chase->unknown_08 = 0;
    race->view_taken = 1;
    if (kind < 0 || kind >= CHASE_KINDS) return;  /* port guard: the original reads past 004b6bb0 */
    const ChaseKind *row = &race->chase_kinds[kind];
    if (row->camera < 0 || row->camera >= RACE_CAMERAS) return;  /* port guard: past 004b78a8 */
    chase->camera = &race->cameras[row->camera];
    chase->camera2 = &race->cameras[CAMERA2];
    chase->flags = 1;
    chase->unknown_68 = row->unknown_38;
    chase->gliding = row->glide_ticks;
    if (chase->gliding == 0) memcpy(chase->target, row->target, sizeof chase->target);
    if (row->alt_flag != 0 && kind != last && race->camera_hold == 0 &&
        faces_road(race, race->player >= 0 && race->player < RACE_CARS ? race->cars[race->player] : NULL) &&
        (last < 4 || kind == KIND_ALT))
        memcpy(chase->target, row->alt_target, sizeof chase->target);
    memset(chase->turn, 0, sizeof chase->turn);
    race_chase_tick(race, chase);
    if (chase->camera != NULL && chase->camera->view >= 0 && race->select_view != NULL)
        race->select_view(race->view_user, race->chase.camera, chase->camera->view, race->window_size);
}

/* What the kind's +0x10 points at; road_yaw is 004046b0's local. */
static int32_t source_yaw(const ChaseKind *row, int32_t road_yaw)
{
    return row->yaw_source == YAW_ROAD ? road_yaw : row->yaw_car->yaw;
}

/* Kinds 0 and 1: the eye rides in the car. */
static void in_car(RaceState *race, RaceChase *chase, ChaseKind *row)
{
    const Car *car = chase->car;
    chase->turn[0] = sub(add(race->camera_pitch, mul(race->camera_tilt >> 8, car->tilt_x >> 8)),
                         car_suspension_wobble(car->suspension_x, car->suspension_x_index));
    chase->turn[2] = sub(mul(race->camera_tilt >> 9, car->tilt_z >> 8),
                         car_suspension_wobble(car->suspension_z, car->suspension_z_index));
    const Car *a = chase->car_angles;
    race_chase_unturn(chase->target, chase->offset, a->pitch, a->roll, a->yaw);
    place_eye(chase);
    if (car->node != race->chase.node) {
        race->chase.flags |= 1;
        race->chase.node = car->node;
    }
    row->yaw_source = YAW_CAR;
    row->yaw_car = car;
    chase->yaw = car->yaw;
    memcpy(chase->target, row->target, sizeof chase->target);
}

/* The eye jumps to the car when its node is more than 16 nodes off, round
 * the ring too. */
static void keep_near(const RaceState *race, RaceChase *chase, int32_t node)
{
    int32_t off = magnitude(sub(node, chase->node));
    if (off <= NEAR_NODES) return;
    const int32_t ring = (int32_t)((uint32_t)race->segments << 2);
    if (off > ring / 2) off = sub(off, ring);
    if (magnitude(off) <= NEAR_NODES) return;
    chase->node = node;
    chase->eye[0] = chase->car->x, chase->eye[1] = chase->car->y, chase->eye[2] = chase->car->z;
}

/* How far sideways the car is from where the road's middle runs, less half
 * the verges' difference; 0 on a node with slant_a >> 6 set. */
static int32_t sideways(const RaceState *race, const Car *car)
{
    const TriNode n = race_node(race, car->node);
    const int32_t angle = node_heading(race, car->node) >> 8;
    const int32_t s = fx_sin16(angle), c = fx_cos16(angle);
    int32_t off = sub(mul(sub(n.position.z, car->z) >> 8, s >> 8), mul(c >> 8, sub(n.position.x, car->x) >> 8));
    off = sub(off, ((int32_t)n.right_verge * VERGE_UNIT - (int32_t)n.left_verge * VERGE_UNIT) / 2);
    if ((n.slant_a * 1024) >> 16 != 0) off = 0;
    return off;
}

/* The row's mode moves its target or yaw source and starts a glide. */
static void start_glide(const RaceState *race, RaceChase *chase, ChaseKind *row)
{
    const int32_t off = sideways(race, chase->car);
    switch (row->mode) {
    case 1: {
        const int32_t v = add(off >> 1, off);
        row->target[0] = v;
        row->target[0] = neg(v);
        break;
    }
    case 2:
        row->target[0] = neg(off) >> 1;
        break;
    case 3:
        row->yaw_source = YAW_CAR;
        row->yaw_car = chase->car;
        break;
    case 4:
        row->yaw_source = YAW_ROAD;
        break;
    default:
        return;
    }
    race_chase_glide(chase, row->target, row->divide, row->shift);
    chase->gliding = 2;
}

/* The road's slope under the car, blended across its node and the next. */
static int32_t slope_lift(const RaceState *race, const RaceChase *chase, int32_t blend, int32_t rest)
{
    int32_t a = race_node(race, chase->car->node).slope * HEADING_SCALE;
    if (a > HALF_TURN) a -= FULL_TURN;
    int32_t b = race_node(race, add(chase->car->node, 1)).slope * HEADING_SCALE;
    if (b > HALF_TURN) b -= FULL_TURN;
    int32_t t = add(mul(a >> 8, rest), mul(b >> 8, blend)) >> 2;
    if (t > 0) t /= 2;
    return mul(fx_sin16(t >> 8) >> 8, chase->target[2] >> 9);
}

/* The eye is kept between the node's barriers. */
static void inside_barriers(const RaceState *race, RaceChase *chase)
{
    const TriNode n = race_node(race, chase->node);
    const int32_t dx = sub(chase->eye[0], n.position.x), dz = sub(chase->eye[2], n.position.z);
    const int32_t heading = neg(n.orientation * HEADING_SCALE), angle = heading >> 8;
    int32_t across = add(mul(dz >> 8, fx_sin16(angle)), mul(fx_cos16(angle), dx >> 8)) >> 8;
    const int32_t right = (int32_t)n.right_barrier * VERGE_UNIT, left = neg((int32_t)n.left_barrier * VERGE_UNIT);
    if (right < across) across = right;
    else if (left > across) across = left;
    else return;
    /* the original leaves the middle word as it was on the stack; with no
     * pitch or roll it does not reach x or z */
    const int32_t along = sub(mul(dz >> 8, fx_cos16(angle)), mul(fx_sin16(angle), dx >> 8)) >> 8;
    const int32_t local[3] = { across, 0, along };
    int32_t out[3];
    race_chase_unturn(local, out, 0, 0, n.orientation * HEADING_SCALE);
    chase->eye[0] = add(out[0], n.position.x);
    chase->eye[2] = add(out[2], n.position.z);
}

/* The rest of 004046b0: the eye behind the car looks at a point ahead along
 * the road. Returns 1 when the glide ended into the row's next kind. */
static int follow_road(RaceState *race, RaceChase *chase, ChaseKind *row)
{
    const Car *car = chase->car, *a = chase->car_angles;
    row->yaw_source = YAW_CAR;
    row->yaw_car = car;
    const int32_t blend = car->lateral >> 8, node = car->node, rest = sub(BLEND_FULL, blend);
    keep_near(race, chase, node);
    const TriNode ahead = race_node(race, add(node, LOOK_NODES)), behind = race_node(race, sub(node, LOOK_NODES));
    const TriNode ahead2 = race_node(race, add(node, LOOK_NODES + 1)), behind2 = race_node(race, sub(node, LOOK_NODES - 1));
    int32_t dir[3];
    dir[0] = add(mul(blend, sub(ahead2.position.x, behind2.position.x) >> 8),
                 mul(sub(ahead.position.x, behind.position.x) >> 8, rest)) / BLEND_FULL;
    dir[2] = add(mul(sub(ahead.position.z, behind.position.z) >> 8, rest),
                 mul(blend, sub(ahead2.position.z, behind2.position.z) >> 8)) / BLEND_FULL;
    int32_t road_yaw = angle_of(dir[2], dir[0]);
    if (race->port_follow_camera) {   /* port: the camera's heading is the car's, not the road's */
        road_yaw = race_follow_yaw(chase, car);
        race_follow_direction(road_yaw, dir);
    }
    dir[1] = 0;
    rm_unit_rounded(dir);
    const int32_t point[3] = { add(car->x, mul(dir[0] >> 8, row->scale) >> 8),
                               add(car->y, mul(dir[1] >> 8, row->scale) >> 8),
                               add(car->z, mul(row->scale, dir[2] >> 8) >> 8) };
    const int32_t aim_yaw = angle_of(sub(point[2], chase->eye[2]), sub(point[0], chase->eye[0]));
    int32_t pitch = neg(angle_of(race_chase_distance(point, chase->eye), sub(point[1], chase->eye[1])));
    if (row->mode != 0) start_glide(race, chase, row);
    if (chase->gliding != 0) {
        for (int i = 0; i < 3; i++) chase->target[i] = add(chase->target[i], chase->step[i]);
        int32_t d = sub(source_yaw(row, road_yaw), chase->yaw);
        while (d > HALF_TURN) d -= FULL_TURN;
        while (d < -HALF_TURN) d += FULL_TURN;
        chase->gliding = sub(chase->gliding, 1);
        chase->yaw = add(chase->yaw, d >> (row->shift & 31));
        if ((magnitude(sub(row->target[2], chase->target[2])) < GLIDE_NEAR && row->divide != 0) ||
            chase->kind == 0 || chase->kind == 1 || chase->target[2] >= row->z_limit || chase->gliding == 0) {
            memcpy(chase->target, row->target, sizeof chase->target);
            chase->yaw = source_yaw(row, road_yaw);
            chase->gliding = 0;
            if (row->next >= 0) {
                chase->next_kind = row->next;
                race_chase_take(race, chase, row->next);
                return 1;
            }
        }
    }
    const int32_t yaw = (row->mode == 3 || row->mode == 4) && !race->port_follow_camera ? chase->yaw : road_yaw;
    if (row->mode == 3) race_chase_unturn(chase->target, chase->offset, a->pitch, a->roll, yaw);
    else if (chase->kind == KIND_NO_ROAD) race_chase_unturn(chase->target, chase->offset, pitch, 0, yaw);
    else race_chase_unturn(chase->target, chase->offset, 0, 0, yaw);
    chase->eye[0] = add(chase->base->x, chase->offset[0]);
    chase->offset[1] = add(chase->offset[1], slope_lift(race, chase, blend, rest));
    chase->eye[1] = add(chase->base->y, chase->offset[1]);
    chase->eye[2] = add(chase->base->z, chase->offset[2]);
    const Car *held = race_car_by_handle(race, car->unknown_088);
    if (held != NULL && held->physics == PHYSICS_ROAD) {  /* port guard: no such car is not on the road model */
        const int32_t floor = add(race_chase_road_height(race, chase->node, chase->eye), ROAD_CLEARANCE);
        if (floor < chase->eye[1]) chase->eye[1] = floor;
    }
    chase->flags |= point_find_node(chase->eye, race, &chase->node);
    const int32_t unclamped[2] = { chase->eye[0], chase->eye[2] };
    inside_barriers(race, chase);
    if (race->port_follow_camera) {   /* port: the barrier slides the eye along it instead of dragging it across the car when the camera swings round */
        const int32_t car_xz[2] = { chase->base->x, chase->base->z };   /* the eye is the base car's plus an offset */
        int32_t clamped[2] = { chase->eye[0], chase->eye[2] };
        race_follow_keep_radius(car_xz, unclamped, clamped);
        chase->eye[0] = clamped[0], chase->eye[2] = clamped[1];
    }
    int32_t turn_yaw = aim_yaw;
    if (!race->port_follow_camera &&
        (row->mode == 3 || row->mode == 4 || chase->kind == KIND_REPLAY || chase->kind == KIND_NO_ROAD)) {
        turn_yaw = chase->yaw;
    }
    /* the chase rows (mode 3) tilt with the car by a quarter more than it pitches, with or without the follow camera:
     * its look-at point at the car's height gave a different tilt on the crests and the dips (the camera sat lower) */
    if (row->mode == 3) pitch = mul(a->pitch, 5) / 4;
    chase->turn[1] = sub(turn_yaw, a->yaw);
    chase->turn[0] = sub(pitch, a->pitch);
    if (row->mode != 3) chase->turn[2] = neg(a->roll);
    return 0;
}

void race_chase_tick(RaceState *race, RaceChase *chase)
{
    if (chase->kind < 0 || chase->kind >= CHASE_KINDS || chase->car == NULL || chase->base == NULL ||
        chase->car_angles == NULL)
        return;  /* port guard: the original reads past 004b6bb0 or through NULL */
    ChaseKind *row = &race->chase_kinds[chase->kind];
    Car *car = chase->car;
    row->yaw_source = YAW_CAR;
    row->yaw_car = car;
    const Car *viewed = race->player >= 0 && race->player < RACE_CARS ? race->cars[race->player] : NULL;
    if (chase->kind == KIND_PASSED) {
        if (!race_chase_ahead_of_other(race, -10, 12) || !faces_road(race, race->cars[1])) {
            race_chase_follow(race, chase, (Car *)viewed, KIND_NO_ROAD);
            return;
        }
    } else if (chase->kind == KIND_PASSED_COP) {
        if (!race_chase_ahead_of_slot2(race, -1, 12) || !faces_road(race, race->cars[2])) {
            race_chase_follow(race, chase, (Car *)viewed, KIND_NO_ROAD);
            return;
        }
    }
    if (chase->kind == 0 || chase->kind == 1) {
        in_car(race, chase, row);
    } else if (chase->kind == KIND_STILL) {
        memcpy(chase->target, row->target, sizeof chase->target);
        memcpy(chase->offset, chase->target, sizeof chase->offset);
        place_eye(chase);
        chase->flags |= point_find_node(chase->eye, race, &chase->node);
    } else if (follow_road(race, chase, row)) {
        return;
    }
    const Car *a = chase->car_angles;
    chase->aim[0] = (int32_t)((uint32_t)add(a->pitch, chase->turn[0]) & AIM_MASK);
    chase->aim[2] = (int32_t)((uint32_t)add(a->roll, chase->turn[2]) & AIM_MASK);
    chase->aim[1] = (int32_t)((uint32_t)add(a->yaw, chase->turn[1]) & AIM_MASK);
    memcpy(chase->position, chase->eye, sizeof chase->position);
    memcpy(chase->angles, chase->aim, sizeof chase->angles);
    if (chase->kind > 1) chase->angles[2] = add(chase->angles[2], race->camera_roll_add);
}

enum { ROAD_NODE_BYTES = 36, ROAD_HEADING_AT = 0x16, HEADING_SHIFT = 10, FIRST_LAYOUT = 0, FRONT = 0 };

void race_chase_start_view(const RaceChaseStartOps *ops, RaceChase *chase, const uint8_t *nodes, int32_t mask)
{
    int32_t word;
    memcpy(&word, nodes + (size_t)(uint32_t)(chase->node & mask) * ROAD_NODE_BYTES + ROAD_HEADING_AT, sizeof word);
    chase->yaw = (int32_t)((uint32_t)(word >> 16) << HEADING_SHIFT);
    ops->views(ops->ctx);
    ops->layout(ops->ctx, chase->camera, FIRST_LAYOUT);
    ops->view(ops->ctx, FRONT);
}
