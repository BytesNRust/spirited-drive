#include "game/race/car_physics.h"

#include "game/race/car_drive.h"
#include "game/race/car_engine.h"
#include "game/race/car_gearbox.h"
#include "game/race/car_grip.h"
#include "game/race/car_progress.h"
#include "game/race/car_road.h"
#include "game/race/car_tyres.h"
#include "game/race/car_vertical.h"
#include "game/race/car_walls.h"
#include "game/sys/fixed_math.h"

#include <string.h>

enum {
    GEAR_REVERSE = -2,
    GEAR_NEUTRAL = -1,
    TICK_SCALE = 0x888,           /* 0x10000 / 30 */
    TICKS_PER_SECOND = 0x1e,
    TICK_ENGINE = 4,              /* race->tick_flags */
    TICK_AIDS = 0x10,
    FULL_BRAKE = 0x140000,
    BLOWN_BRAKE = 0x280000,
    CREEP = 0x1999,
    STILL = 0x4ccc,               /* below this the forces alone move the car */
    COASTING = 0xa0000,
    MAX_YAW_RATE = 0x960000,
    MAX_ARCADE_TURN = 0x4b0000,
    ROLL_BANKING = 0x30000,
    STRIP_END = 0x61,             /* track 3's drag strip: nodes up to here, and from STRIP_START */
    STRIP_START = 0x1d1,
    STRIP_QUARTER = 0x53,         /* nodes from the start */
    STRIP_UNSET = 99999,
    MPH_60 = 0x1ac937,
    MPH_80 = 0x23b5c2,
    MPH_100 = 0x2ca666,
    /* player record words (from 0051cc5c, 8 bytes into the record) */
    RECORD_0_60 = 0x198,
    RECORD_0_100 = 0x19c,
    RECORD_80_0 = 0x1a0,
    RECORD_60_0 = 0x1a4,
    RECORD_QUARTER_SPEED = 0x1a8,
    RECORD_QUARTER = 0x1ac,
    RECORD_TOP_SPEED = 0x1c4
};

/* One tick's forces: the axles' drive and braking, the drag, and what the
 * tyres pass on (front then rear, sideways then forwards). */
typedef struct {
    int32_t drive;
    int32_t front_drive, rear_drive, front_brake, rear_brake;
    int32_t drag_x, drag_z;
    int32_t front_lateral, front_along, rear_lateral, rear_along;
} Forces;

static int32_t abs32(int32_t v)
{
    return v > 0 ? v : -v;
}

static int32_t clamp_size(int32_t v, int32_t limit)
{
    if (abs32(v) <= limit) return v;
    return v >= 0 ? limit : -limit;
}

/* The original's player record words, at a byte offset from 0051cc5c;
 * offsets outside the records are dropped. */
static int record_offset(const Car *car, int32_t field, int32_t *offset)
{
    const int64_t at = (int64_t)car->player * (int64_t)sizeof(PlayerRecord) + 8 + field;
    if (at < 0 || at + 4 > (int64_t)sizeof(((RaceState *)0)->players)) return 0;
    *offset = (int32_t)at;
    return 1;
}

static int32_t read_record(const RaceState *race, const Car *car, int32_t field)
{
    int32_t offset, value = 0;
    if (record_offset(car, field, &offset)) memcpy(&value, (const uint8_t *)race->players + offset, sizeof value);
    return value;
}

static void write_record(RaceState *race, const Car *car, int32_t field, int32_t value)
{
    int32_t offset;
    if (record_offset(car, field, &offset)) memcpy((uint8_t *)race->players + offset, &value, sizeof value);
}

static void keep_least(RaceState *race, const Car *car, int32_t field, int32_t value)
{
    if (value < read_record(race, car, field)) write_record(race, car, field, value);
}

/* With two players the trailing car runs more ticks a second (a smaller
 * tick), half its distance behind, up to race->catch_up more. */
static void tick_rate(Car *car, const RaceState *race)
{
    car->tick_scale = TICK_SCALE;
    car->ticks_per_second = TICKS_PER_SECOND;
    if (race->humans != 2 || race->catch_up == 0) return;
    const Car *other = race->cars[car->player == 0 ? 1 : 0];
    if (other == NULL) return;
    int32_t behind = (other->progress - car->progress) / 2;
    if (behind <= 0) return;
    if (behind > race->catch_up) behind = race->catch_up;
    car->ticks_per_second = TICKS_PER_SECOND - behind;
    car->tick_scale = car->ticks_per_second == 0 ? 0 : 0x10000 / car->ticks_per_second;  /* the original faults */
}

static void driver_aids(Car *car, const RaceState *race)
{
    if ((race->tick_flags & TICK_AIDS) && car->blown == 0) {
        car->anti_lock_active = car->anti_lock;
        car->traction_active = car->traction_control;
    } else {
        car->anti_lock_active = 0;
        car->traction_active = 0;
    }
}

static void gearbox(Car *car, const CarSpecs *specs, const RaceState *race)
{
    if (!(race->tick_flags & TICK_ENGINE) && car->gear != GEAR_NEUTRAL) return;
    car_engine_revs(car, specs, race);
    if (car->transmission == 3) {
        car_gear_change(car, specs, race);
    } else if (car->transmission == 2) {
        car->gear = GEAR_NEUTRAL;
    } else if (car->transmission == 1) {
        car->gear = GEAR_REVERSE;
    }
}

/* The engine's force and the brakes, split between the axles. */
static void axle_forces(Car *car, const CarSpecs *specs, RaceState *race, Forces *f)
{
    const int32_t force = car_drive_force(car, specs, race);
    car->drive_force = force;
    f->drive = force;
    if (car->traction_active != 0 && car->gear == 0) f->drive -= force / 4;
    car->unknown_40d = 0;
    f->front_drive = (f->drive >> 8) * (specs->unknown_018 >> 8);
    f->rear_drive = f->drive - f->front_drive;
    if (car->brake > 0xf0) {
        f->front_brake = FULL_BRAKE;
        f->rear_brake = FULL_BRAKE;
    } else {
        const int32_t total = ((car->brake * 330) >> 8) * ((car->axle_load_b + car->axle_load_a) >> 8);
        f->front_brake = (total >> 8) * (specs->unknown_01c >> 8);
        f->rear_brake = total - f->front_brake;
    }
    if (car->blown == 1) {
        race_blow_up(race);
        f->rear_drive = 0;
        f->rear_brake = BLOWN_BRAKE;
    }
}

/* Rolling the wrong way for the gear at a crawl: a weak drive brakes. */
static void stop_rolling_back(const Car *car, const CarSpecs *specs, Forces *f)
{
    if (abs32(car->local_velocity_x) >= CREEP) return;
    if (!((car->local_velocity_z > 0x10000 && car->gear == GEAR_REVERSE) ||
          (car->local_velocity_z < -0x10000 && car->gear >= 0)))
        return;
    if (f->front_drive != 0 && f->front_drive < car->axle_load_b) {
        f->front_drive = 0;
        f->front_brake = specs->unknown_028;
    }
    if (f->rear_drive != 0 && f->rear_drive < car->axle_load_a) {
        f->rear_drive = 0;
        f->rear_brake = specs->unknown_028;
    }
}

static void pedal_aids(Car *car, const Forces *f)
{
    if (car->throttle < 0x28) {
        car->traction_active = 0;
    } else if (car->traction_active != 0 && abs32(f->drive) > 0x70000 && car->throttle > 0x53) {
        car->throttle -= 12;
    }
    if (car->brake < 0x28) car->anti_lock_active = 0;
}

/* A new gear shows on the dash for 16 ticks, with its tone and a click. */
static void gear_display(Car *car, const RaceState *race)
{
    if (car->gear != car->last_gear) {
        if (race->effects != 0 && race->viewed == car->player) car_gear_sound(car, race);
        car->gear_change_ticks = 0x10;
        car->gear_before_last = car->last_gear;
        car->last_gear = car->gear;
    }
    if (car->gear_change_ticks <= 0) return;
    if (car->gear_change_ticks > 12 || car->shift_timer < 1) car->gear_change_ticks--;
    if (race->viewed == car->player && car->gear_change_ticks == 11 && race->views == 0 && car->shift_timer < 1 &&
        race->sounds.sound != NULL)
        race->sounds.sound(race->sounds.user, -1, 0xd, 0, 0, 1, 0xf00000);
}

/* Air and rolling drag; past spec +0x38 (except on track 6) it holds the
 * car back at least as hard as the engine pushes. */
static void drag(Car *car, const CarSpecs *specs, const RaceState *race, Forces *f)
{
    f->drag_x = car_drag(car, specs, race, car->local_velocity_x, 0);
    f->drag_z = car_drag(car, specs, race, car->local_velocity_z, 1);
    if (car->local_velocity_z <= specs->unknown_038 || race->track == 6) return;
    if (f->drag_z > 0 && f->drag_z < f->drive) {
        f->drag_z = f->drive;
    } else if (f->drag_z < 0 && abs32(f->drag_z) < abs32(f->drive)) {
        f->drag_z = -abs32(f->drive);
    }
}

/* Less steering at speed, more at a crawl, half under hard braking. */
static int32_t wheel_angle(const Car *car)
{
    const int32_t steer = car->steer;
    if (car->speed < 0xc0000) return car->brake < 10 ? steer + steer / 2 : steer;
    if (car->brake > 200) return steer / 2;
    return steer - steer * (car->local_velocity_z / 256) / 32768;
}

/* The car's speed in its own frame becomes the forces each axle's tyres
 * resist, with half the drag on each and the turn pulling the ends apart. */
static void tyres(Car *car, const CarSpecs *specs, RaceState *race, Forces *f)
{
    int32_t lx, lz;
    fx_rotate(car->velocity_x, car->velocity_z, -car->yaw, &lx, &lz);
    car->local_velocity_x = lx;
    car->local_velocity_z = lz;
    const int32_t sx = lx * car->ticks_per_second, sz = lz * car->ticks_per_second;
    const int32_t front_x = fx_mul_round(sx, car->unknown_43d);
    const int32_t turn = fx_mul_round(car->yaw_rate * car->ticks_per_second, car->unknown_459);
    const int32_t front_z = fx_mul_round(sz, car->unknown_43d);
    const int32_t front_lateral = -front_x - f->drag_x / 2 + turn;
    const int32_t rear_lateral = front_x - sx - f->drag_x / 2 - turn;
    const int32_t front_along = -front_z - f->drag_z / 2;
    const int32_t rear_along = front_z - sz - f->drag_z / 2;
    const int32_t angle = wheel_angle(car);
    car->skid_front = 0;
    car->skid_rear = 0;
    f->front_lateral = f->front_along = f->rear_lateral = f->rear_along = 0;
    car_tyre_forces(car, specs, race, &f->front_lateral, &f->front_along, front_lateral, front_along, angle,
                    f->front_drive, f->front_brake, 1);
    car_tyre_forces(car, specs, race, &f->rear_lateral, &f->rear_along, rear_lateral, rear_along, 0, f->rear_drive,
                    f->rear_brake, 2);
}

/* Braking (or a blown engine) caps the forward force by speed; on the drag
 * strip the sideways force too. The sideways force never passes spec
 * +0x31c plus the load change. */
static void limit_forces(const Car *car, const CarSpecs *specs, const RaceState *race, int32_t *lateral,
                         int32_t *along)
{
    if (car->brake > 100 || car->blown != 0) {
        int32_t limit;
        if (car->speed < 0x1ac7ae) {
            limit = specs->unknown_028;
        } else if (car->speed < 0x280000) {
            limit = specs->unknown_02c;
        } else {
            limit = specs->unknown_02c > specs->unknown_028 ? specs->unknown_02c : specs->unknown_028;
        }
        *along = clamp_size(*along, limit);
        if (race->track == 3 && (car->node <= STRIP_END || car->node >= STRIP_START))
            *lateral = clamp_size(*lateral, limit);
    }
    int32_t most = specs->unknown_31c + car->load_shift;
    if (most < 0) most = 0;
    *lateral = clamp_size(*lateral, most);
}

/* The forward force moves weight to the rear; the surface sets the grip. */
static void axle_loads(Car *car, const RaceState *race, int32_t along)
{
    const int32_t grip = car->surface >= 0 && car->surface < RACE_SURFACES ? race->surfaces[car->surface].grip : 0;
    const int32_t moved = (along >> 8) * (car->unknown_44d >> 8);
    car->axle_load_b = (car->unknown_455 - moved) * grip / 256;
    car->axle_load_a = (car->unknown_451 + moved) * grip / 256;
    car_axle_loads(car, race);
}

static void accelerate(Car *car, const Forces *f, int32_t lateral, int32_t along)
{
    const int idle = car->gear == GEAR_NEUTRAL || car->throttle == 0;
    if (abs32(car->local_velocity_z) + abs32(car->local_velocity_x) < STILL) {
        car->local_velocity_x += fx_mul_round(lateral, car->tick_scale);
        car->local_velocity_z += fx_mul_round(along, car->tick_scale);
        if (idle) {
            car->local_velocity_z = 0;
            car->local_velocity_x = 0;
        }
        return;
    }
    car->local_velocity_x += fx_mul_round(f->drag_x + lateral + car->unknown_3d9, car->tick_scale);
    car->local_velocity_z += fx_mul_round(f->drag_z + along + car->unknown_3e1, car->tick_scale);
    if (abs32(car->local_velocity_z) < COASTING && idle) {
        car->local_velocity_z -= car->local_velocity_z / 64;
        car->local_velocity_x -= car->local_velocity_x / 64;
    }
}

/* 004badd8 (arcade 004bae24): how much of the sideways speed along the road
 * turns into forward speed, as a shift by speed. Past the end the original
 * reads on into the next table and the words after it. */
static const int32_t ROAD_PULL[38] = { 16, 10, 6, 4, 4, 4, 3, 3, 3, 3, 3, 2, 1, 1, 1, 1, 1, 1, 1,
                                       16, 16, 8, 4, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };

static int32_t road_pull(const RaceState *race, int32_t index)
{
    if (race->arcade != 0) index += 19;
    if (index >= 0 && index < 38) return ROAD_PULL[index];
    const int32_t after[] = { race->arcade_turn, race->arcade_grip, race->arcade_turn_limit,
                              race->strip_brake_80_tick, race->strip_brake_60_tick, race->strip_best_60,
                              race->strip_best_100, race->strip_best_quarter, race->strip_start_tick,
                              race->strip_start_progress, 1, 10, 100, 1000 };
    index -= 38;
    return index >= 0 && index < (int32_t)(sizeof after / sizeof after[0]) ? after[index] : 0;
}

static int32_t road_heading(const RaceState *race, int32_t node)
{
    return race_node(race, node).orientation * 0x400;
}

/* The world velocity, and the move: along the road the sideways part is
 * partly pulled into the forward part first. */
static void move(Car *car, const RaceState *race)
{
    int32_t vx, vz;
    fx_rotate(car->local_velocity_x, car->local_velocity_z, car->yaw, &vx, &vz);
    car->velocity_x = vx;
    car->velocity_z = vz;
    const int32_t heading = road_heading(race, car->node);
    int32_t across, ahead;
    fx_rotate(vx, vz, -heading, &across, &ahead);
    const int32_t shift = road_pull(race, (abs32(across) + abs32(ahead)) >> 19) & 31;
    if (ahead > 0) {
        ahead += abs32(across) >> shift;
        across -= across >> shift;
    }
    int32_t wx, wz;
    fx_rotate(across, ahead, heading, &wx, &wz);
    car->z += fx_mul_round(wz / 4 + wz, car->tick_scale);
    car->x -= fx_mul_round(wx / 4 + wx, car->tick_scale);
}

static void body_tilt(Car *car, const CarSpecs *specs)
{
    if (car->local_velocity_z + car->local_velocity_x > CREEP) {
        car->tilt_z += (-((car->lateral_force >> 8) * (specs->unknown_2d4 >> 8)) - car->tilt_z) / 2;
        car->tilt_x += (-((car->along_force >> 8) * (specs->unknown_2d8 >> 8)) - car->tilt_x) / 2;
    } else {
        car->tilt_z += -car->tilt_z / 2;
        car->tilt_x += -car->tilt_x / 2;
    }
}

/* Arcade steering: the turn rate follows the wheel, scaled by the forward
 * speed (up to 1.0 at 0x10000) and eased off by the forward force. */
static int32_t arcade_turning(const Car *car, RaceState *race)
{
    race->arcade_grip = car->local_velocity_z;
    if (race->arcade_grip > 0x10000) {
        race->arcade_grip = 0x10000;
    } else if (race->arcade_grip < -0x10000) {
        race->arcade_grip = -0x10000;
    }
    if (race->arcade_grip < 0x8000 && race->arcade_grip > 0 && car->rpm > 3000 && car->gear > 0)
        race->arcade_grip = 0x8000;
    race->arcade_turn = car->steer / 4 + car->steer;
    if (abs32(car->local_velocity_z) < abs32(car->local_velocity_x))
        race->arcade_turn += car->throttle < 10 ? car->yaw_rate / 2 : car->yaw_rate / 8;
    race->arcade_turn = (race->arcade_grip >> 10) * race->arcade_turn / 64;
    if (race->wheelspin != 0) race->arcade_turn += race->arcade_turn;
    race->arcade_turn_limit = MAX_ARCADE_TURN;
    race->arcade_turn = race->arcade_turn / 256 * ((0x280000 - car->along_force) / 256) / 40;
    const int32_t turn = race->arcade_turn;
    if (car->yaw_rate > turn) {
        const int32_t a = (turn - car->yaw_rate) * 16;
        return a < -MAX_ARCADE_TURN ? -MAX_ARCADE_TURN : a;
    }
    if (car->yaw_rate < turn) {
        const int32_t a = (turn - car->yaw_rate) * 16;
        return a > MAX_ARCADE_TURN ? MAX_ARCADE_TURN : a;
    }
    return 0;
}

/* Leaning into a bend turns the car with the road. */
static int32_t banking(const Car *car, const RaceState *race)
{
    int32_t here = road_heading(race, car->node), next = road_heading(race, car->node + 1);
    if (here > 0x800000) here -= 0x1000000;
    if (next > 0x800000) next -= 0x1000000;
    const int32_t bend = car->local_velocity_z / 256 * (next - here) / 256;
    return fx_mul_round(fx_sin((uint32_t)(car->roll >> 14)), bend) / 128;
}

static void turn(Car *car, RaceState *race, const Forces *f)
{
    int32_t accel = fx_mul_round(f->rear_lateral, car->unknown_465) - fx_mul_round(f->front_lateral, car->unknown_461);
    if (race->arcade != 0) accel = car->on_ground != 0 ? arcade_turning(car, race) : 0;
    car->yaw_rate += fx_mul_round(accel, car->tick_scale);
    if (abs32(car->yaw_rate) > MAX_YAW_RATE) car->yaw_rate = car->yaw_rate < 0 ? -MAX_YAW_RATE : MAX_YAW_RATE;
    const int32_t step = fx_mul_round(car->yaw_rate, car->tick_scale);
    int32_t yaw = car->yaw + step + step / 4;
    if (abs32(car->roll) > ROLL_BANKING) yaw -= banking(car, race);
    if (yaw < 0) yaw += 0x1000000;
    car->yaw = yaw & 0xffffff;
}

/* The replay's notes: sliding, skidding and the first move. */
static void driving_notes(const Car *car, RaceState *race)
{
    if (!(race->tick_flags & TICK_ENGINE) || race->phase != 1) return;
    if (abs32(car->local_velocity_x) > abs32(car->local_velocity_z / 2) && car->local_velocity_x > 0x8000)
        race_note(race, 0x57);
    if ((car->skid_front != 0 || car->skid_rear != 0) && car->speed > 0x140000 &&
        ((car->tyre_flags_rear & 1) || (car->tyre_flags_front & 1)))
        race_note(race, 0x28);
    if (race->first_move_tick == 0 && abs32(car->local_velocity_z) > 0xccc) {
        race_note(race, 0x78);
        race->first_move_tick = race->tick + 1;
    }
}

static void strip_acceleration(const Car *car, RaceState *race)
{
    const int32_t v = car->local_velocity_z;
    if (v < 0x3333) {
        race->strip_start_tick = race->tick;
        race->strip_start_progress = car->progress;
        race->strip_best_60 = race->strip_best_100 = race->strip_best_quarter = STRIP_UNSET;
    }
    int32_t t = race->tick - race->strip_start_tick;
    if (car->throttle > 0x32 && t < 1500 && t > 100) {
        if (v > MPH_60 && t < race->strip_best_60) {
            race->strip_best_60 = t;
            race_dash_values(race, t, 0, 0, 0);
            keep_least(race, car, RECORD_0_60, t);
        }
        if (v > MPH_100 && t < race->strip_best_100) {
            race->strip_best_100 = t;
            race_dash_values(race, 0, t, 0, 0);
            keep_least(race, car, RECORD_0_100, t);
        }
    }
    if (car->progress - race->strip_start_progress <= STRIP_QUARTER) return;
    t = race->tick - race->strip_start_tick;
    race->strip_start_progress = 999999;
    if (t >= race->strip_best_quarter || t >= 1000) return;
    if (t < read_record(race, car, RECORD_QUARTER)) {
        write_record(race, car, RECORD_QUARTER, t);
        write_record(race, car, RECORD_QUARTER_SPEED, v);
    }
    race_dash_values(race, 0, 0, t, v);
    race->strip_best_quarter = t;
}

static void strip_braking(const Car *car, RaceState *race)
{
    const int32_t v = car->local_velocity_z;
    if (car->brake <= 0x32) return;
    if (v > MPH_80) race->strip_brake_80_tick = race->tick;
    if (v > MPH_60) race->strip_brake_60_tick = race->tick;
    if (v < CREEP && race->strip_brake_60_tick > 0) {
        const int32_t t = race->tick - race->strip_brake_60_tick;
        race_dash_gauges(race, t, 0);
        race->strip_brake_60_tick = 0;
        keep_least(race, car, RECORD_60_0, t);
    }
    if (v < CREEP && race->strip_brake_80_tick > 0) {
        const int32_t t = race->tick - race->strip_brake_80_tick;
        race_dash_gauges(race, 0, t);
        race->strip_brake_80_tick = 0;
        keep_least(race, car, RECORD_80_0, t);
    }
}

/* Track 3's drag strip times 0-60, 0-100, the quarter mile and the stops
 * from 60 and 80. */
static void drag_strip(const Car *car, RaceState *race)
{
    if (race->strip_off != 0 || race->track != 3) return;
    if (car->node <= STRIP_END || car->node >= STRIP_START) {
        strip_acceleration(car, race);
        strip_braking(car, race);
    }
    if (car->tyre_flags_rear != 0 && abs32(car->yaw_rate) > 0x320000) race_note(race, 0x57);
}

static void after_move(Car *car, RaceState *race)
{
    race->tyre_force_sum = car->local_velocity_x + car->local_velocity_y + car->local_velocity_z;
    car_walls(car, race);
    if (race->player == car->player) car_node_flags(car, race);
    driving_notes(car, race);
    const int32_t forward = abs32(car->local_velocity_z);
    if (forward > read_record(race, car, RECORD_TOP_SPEED)) write_record(race, car, RECORD_TOP_SPEED, forward);
    drag_strip(car, race);
    race->tyre_force_sum = car->x + car->y + car->z;
    if (car->suspension_z_index > 1) car->suspension_z_index -= 2;
    if (car->suspension_x_index > 0) car->suspension_x_index--;
}

void car_driving_physics(Car *car, const CarSpecs *specs, RaceState *race)
{
    race->wheelspin = 0;
    const int32_t ax = abs32(car->velocity_x), az = abs32(car->velocity_z);
    car->speed = ax > az ? ax + (az >> 2) : az + (ax >> 2);
    if (car->blown == 1 && race->arcade != 0) {
        car->blown = 0;
        car->brake = 0xfa;
    }
    tick_rate(car, race);
    driver_aids(car, race);
    gearbox(car, specs, race);
    Forces f;
    axle_forces(car, specs, race, &f);
    stop_rolling_back(car, specs, &f);
    pedal_aids(car, &f);
    gear_display(car, race);
    drag(car, specs, race, &f);
    tyres(car, specs, race, &f);
    int32_t lateral = f.front_lateral + f.rear_lateral, along = f.front_along + f.rear_along;
    limit_forces(car, specs, race, &lateral, &along);
    axle_loads(car, race, along);
    along = specs->unknown_32c * along / 256;
    car->lateral_force = lateral;
    car->along_force = along;
    accelerate(car, &f, lateral, along);
    move(car, race);
    body_tilt(car, specs);
    turn(car, race, &f);
    after_move(car, race);
}

void car_driving_tick(Car *car, const CarSpecs *specs, RaceState *race)
{
    car_driving_physics(car, specs, race);
    car_progress(car, race);
    car_vertical(car, race, 1);
}
