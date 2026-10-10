#include "game/race/car_control.h"

#include "game/sys/fixed_math.h"

enum {
    INPUT_STEER_CENTRE = 0x20,
    INPUT_PEDAL_REST = 0x1e,
    KEY_GEAR_UP = 1,              /* input >> 12 */
    KEY_GEAR_DOWN = 2,
    KEY_LOOK_BACK = 4,
    KEY_HANDBRAKE = 8,
    GEAR_DOWN = 1,                /* the gear key codes of 00438c84 */
    GEAR_UP = 2,
    GEAR_KEYS_FROM = 300,         /* ticks before the gear keys work */
    FULL_LOCK = 0x1b0000,
    AIM_AHEAD = 6,                /* nodes */
    AIM_OFFSET = 0x20000,
    AIM_DEAD_BAND = 0xb333,
    AIM_MAX = 0x40,
    PEDAL_STEP = 0x889,
    THROTTLE_RISE = 0x4c,
    VERGE_UNIT = 0x2000,
    AUTO_LOCK = 0x1e0000,
    AUTO_BRAKE = 0xfa,
    ANGLE_TURN = 0x1000000,
    ANGLE_HALF = 0x800000,
    ANGLE_QUARTER = 0x400000
};

/* 004baff0: wheel or stick steering by distance from the centre. */
static const int32_t ANALOG_STEER[33] = {
    0x3333, 0x6666, 0xb333, 0x10000, 0x16666, 0x1cccc, 0x24ccc, 0x2cccc, 0x36666, 0x40000, 0x4cccc,
    0x5b333, 0x69999, 0x78000, 0x88000, 0x98000, 0xa8000, 0xb8000, 0xc8000, 0xd8000, 0xe8000, 0xf8000,
    0x108000, 0x118000, 0x128000, 0x138000, 0x14b333, 0x15e666, 0x173333, 0x184ccc, 0x199999, 0x1ae666,
    0x1c8000
};

static int32_t abs32(int32_t v)
{
    return v > 0 ? v : -v;
}

static int32_t shift_left(int32_t v, int32_t by)
{
    return (int32_t)((uint32_t)v << (by & 31));
}

/* An input field around its centre, with no zero step on the high side. */
static int32_t centred(uint32_t bits, int32_t centre)
{
    const int32_t v = (int32_t)(bits & 0x3f) - centre;
    return v > 0 ? v + 1 : v;
}

/* 00401010: the turn from the car's heading to a point six nodes on,
 * moved two units to the side the car is already on. */
static int32_t road_aim(const Car *car, const RaceState *race)
{
    const TriNode here = race_node(race, car->node);
    const int32_t a = here.orientation * 4;
    const int32_t side = fx_mul_round(here.position.z - car->z, fx_sin16(a)) -
                         fx_mul_round(here.position.x - car->x, fx_cos16(a));
    const int32_t offset = side < -AIM_DEAD_BAND ? -AIM_OFFSET : side > AIM_DEAD_BAND ? AIM_OFFSET : 0;
    const TriNode ahead = race_node(race, car->node + AIM_AHEAD);
    const int32_t b = ahead.orientation * 4;
    const int32_t x = ahead.position.x + fx_mul_round(fx_cos16(b), offset);
    const int32_t z = ahead.position.z - fx_mul_round(fx_sin16(b), offset);
    int32_t turn = (fx_atan2(x - car->x, z - car->z) << 8) - car->yaw;
    if (turn > ANGLE_HALF) {
        turn -= ANGLE_TURN;
    } else if (turn < -ANGLE_HALF) {
        turn += ANGLE_TURN;
    }
    return turn;
}

/* 00438954: keyboard steering eases to full lock, or back to the road's
 * aim when going straight; faster towards the aim than away from it. */
static void key_steering(Car *car, const CarSpecs *specs, uint32_t input, int32_t aim)
{
    const int32_t keys = centred(input, INPUT_STEER_CENTRE);
    int32_t target = keys >= 0x10 ? FULL_LOCK : keys < -0xf ? -FULL_LOCK : 0;
    if (car->local_velocity_z >= 0 && target == 0 &&
        abs32(aim) < specs->steer_assist * (car->local_velocity_z >> 16))
        target = aim;
    int32_t slow = (abs32(car->local_velocity_z) >> 16) * specs->steer_rate_speed;
    if (slow > 0x18000) slow = 0x18000;
    int32_t rate = specs->steer_rate_base - slow;
    if (rate > 0x19999) rate = 0x19999;
    rate *= specs->steer_rate;
    if (car->local_velocity_z > 0 && abs32(aim) < ANGLE_QUARTER) {
        const int32_t to_aim = aim - car->steer, to_target = target - car->steer;
        if ((to_aim > 0 && to_target > 0) || (to_aim < 0 && to_target < 0)) {
            rate = shift_left(rate, specs->steer_fast_shift);
            if (target == 0) rate *= 2;
        } else {
            rate >>= specs->steer_slow_shift & 31;
            if (target == 0) rate /= 2;
        }
    }
    const int32_t gap = abs32(car->steer - target);
    if (gap < rate) rate = gap;
    if (target > car->steer) {
        car->steer += rate;
    } else if (target < car->steer) {
        car->steer -= rate;
    } else {
        car->steer = target;
    }
}

/* 00438aac: a wheel or stick sets the steering through a curve; turning
 * it quickly is limited, less so at speed, and the road's aim helps. */
static void analog_steering(Car *car, uint32_t input, int32_t aim)
{
    const int32_t pos = centred(input, INPUT_STEER_CENTRE);
    const int32_t target = pos < 0 ? -ANALOG_STEER[-pos] : ANALOG_STEER[pos];
    if (car->local_velocity_z < 0) aim = 0;
    const int32_t ease = car->local_velocity_z <= 0x120000 ? 0 : (car->local_velocity_z - 0x120000) / 128;
    const int32_t to_target = target - car->steer, to_aim = aim - car->steer;
    int32_t step = to_target;
    if (to_target > 0 && to_aim > 0) {
        if (to_aim < to_target) step = to_aim;
    } else if (to_target < 0 && to_aim < 0) {
        if (to_target < to_aim) step = to_aim;
    } else if (to_target < 1) {
        const int32_t limit = -((to_target < -0xc0000 ? 0x40000 : 0x18000) - ease);
        if (to_target <= limit) step = limit;
    } else {
        const int32_t limit = (to_target > 0xc0000 ? 0x40000 : 0x18000) - ease;
        if (limit <= to_target) step = limit;
    }
    car->steer += step;
}

/* 00438c84: gear keys act on a press, not while held. Manual: a gear
 * change cuts the throttle for a while; automatic: the selector moves
 * 1 drive, 2 neutral, 3 reverse... as the original counts it. */
static void gear_keys(Car *car, const CarSpecs *specs, const RaceState *race, int32_t key)
{
    if (race->tick < GEAR_KEYS_FROM) {
        car->gear_key = key;
        return;
    }
    if (car->transmission >= 1) {
        if (car->gear_key != GEAR_DOWN && key == GEAR_DOWN && car->transmission >= 2) {
            car->transmission -= 1;
            if (car->transmission == 2) {
                car->gear = -1;
                car->engaged = 0;
            } else {
                car->gear = 0;
                car->engaged = 1;
                car->stalled = 0;
            }
        } else if (car->gear_key != GEAR_UP && key == GEAR_UP && car->transmission <= 2) {
            car->transmission += 1;
            if (car->transmission == 2) {
                car->engaged = 0;
                car->gear = -1;
            } else {
                car->gear = 0;
                car->engaged = 0;
                car->stalled = 1;
                car->shift_timer = specs->shift_ticks + 1;
                car->saved_throttle = car->throttle;
            }
        }
    } else if (car->gear_key != GEAR_DOWN && key == GEAR_DOWN && car->gear >= -1) {
        car->engaged = 0;
        car->gear -= 1;
        car->stalled = 1;
        car->shift_timer = specs->shift_ticks / 2;
        car->saved_throttle = car->throttle;
    } else if (car->gear_key != GEAR_UP && key == GEAR_UP && specs->gear_limit - 3 > car->gear) {
        car->engaged = 0;
        car->gear += 1;
        const int slow = car->model == 4 && (race->tick & 0x31) == 0x10;
        car->shift_timer = specs->shift_ticks + (slow ? 3 : 0);
        car->stalled = 1;
        car->saved_throttle = car->throttle;
    }
    car->gear_key = key;
    if (car->transmission == 0 && car->shift_timer > -1) {
        if (car->shift_timer == 0) {
            car->stalled = 0;
            car->throttle = car->saved_throttle;
            if (car->gear != -1) car->engaged = 1;
        }
        car->shift_timer -= 1;
    }
}

/* The gear, look back and handbrake keys; returns the pedal's position. */
static int32_t pedal_keys(Car *car, const CarSpecs *specs, const RaceState *race, uint32_t input)
{
    const uint32_t keys = input >> 12;
    gear_keys(car, specs, race, keys & KEY_GEAR_DOWN ? GEAR_DOWN : keys & KEY_GEAR_UP ? GEAR_UP : 0);
    car->handbrake = (keys & KEY_HANDBRAKE) != 0;
    car->look_back = (keys & KEY_LOOK_BACK) != 0;
    return centred(input >> 6, INPUT_PEDAL_REST);
}

/* 00438b70 */
static void throttle_towards(Car *car, const CarSpecs *specs, int32_t target)
{
    if (car->throttle < target) {
        if (specs->throttle_up < target - car->throttle) {
            car->throttle += specs->throttle_up;
            return;
        }
    } else if (car->throttle > target) {
        if (specs->throttle_down < car->throttle - target) {
            car->throttle -= specs->throttle_down;
            return;
        }
    } else {
        return;
    }
    car->throttle = target;
}

/* 00438bc8 */
static void brake_towards(Car *car, const CarSpecs *specs, int32_t target)
{
    const int hard = car->brake >= 0x90;
    if (car->brake < target) {
        const int32_t up = hard ? specs->brake_up_hard : specs->brake_up;
        const int32_t step = up / 4 + up;
        if (step < target - car->brake) {
            car->brake += step;
            return;
        }
    } else if (car->brake > target) {
        const int32_t step = hard ? specs->brake_down_hard : specs->brake_down;
        if (step < car->brake - target) {
            car->brake -= step;
            return;
        }
    } else {
        return;
    }
    car->brake = target;
}

/* 00438f00: the pedal keys push the throttle or the brake all the way. */
static void key_pedals(Car *car, const CarSpecs *specs, const RaceState *race, uint32_t input)
{
    const int32_t pedal = pedal_keys(car, specs, race, input);
    if (pedal < -0xf) {
        car->brake = 0;
        throttle_towards(car, specs, 0xff);
    } else if (pedal > 0xf) {
        brake_towards(car, specs, 0xff);
        throttle_towards(car, specs, 0);
    } else {
        if (car->brake > 100) car->brake = 100;
        if (car->throttle != 0) {
            throttle_towards(car, specs, 0);
        } else if (car->brake != 0) {
            brake_towards(car, specs, 0);
        }
    }
}

/* 00438ff0: analog pedals; the throttle rises by 0x4c a tick and falls
 * faster with the clutch in. */
static void analog_pedals(Car *car, const CarSpecs *specs, const RaceState *race, uint32_t input)
{
    const int32_t pedal = pedal_keys(car, specs, race, input);
    int32_t target = 0;
    if (pedal < 1) {
        target = pedal * -PEDAL_STEP / 256;
        car->brake = 0;
    } else {
        const int32_t brake = pedal * PEDAL_STEP / 256;
        car->throttle = 0;
        if (car->anti_lock == 0) {
            brake_towards(car, specs, brake);
        } else {
            car->brake = brake;
        }
    }
    const int32_t fall = car->engaged == 0 ? 0xc : 0x33;
    if (car->throttle < target) {
        car->throttle = target - car->throttle > THROTTLE_RISE ? car->throttle + THROTTLE_RISE : target;
    } else if (car->throttle > target) {
        car->throttle = car->throttle - target > fall ? car->throttle - fall : target;
    }
    if (car->throttle > 0xff) car->throttle = 0xff;
    if (car->brake > 0xff) car->brake = 0xff;
}

int car_race_window(Car *car, RaceState *race)
{
    if (race->playback != 0 && car->node < 0xe && car->lap == 1 && race->replay == 0 && car->z < 0x510000 &&
        car->player == race->player && race->sounds.start_line != NULL)
        race->sounds.start_line(race->sounds.user, car->player);
    if (race->circuit == 0) {
        if (car->node < 0xc) {
            if (car->z < 0x120000) car->z = 0x120000;
            return 1;
        }
        if (race->loop_nodes < car->node) return 2;
    } else {
        if (car->lap < 1) return 1;
        if (race->laps < car->lap) return 2;
    }
    return 0;
}

/* The autopilot stopped across the road: brake and wind the wheel. */
static void auto_stop(Car *car)
{
    car->handbrake = 0;
    car->brake = AUTO_BRAKE;
    car->throttle = 0;
    car->local_velocity_z = 0;
    car->yaw_rate -= car->yaw_rate / 8;
    if (car->local_velocity_x < 1) {
        if (car->auto_steer < 0x190000) {
            const uint16_t high = (uint16_t)((uint32_t)car->auto_steer >> 16);
            car->auto_steer = (int32_t)((uint32_t)(uint16_t)(high + 3) << 16 | ((uint32_t)car->auto_steer & 0xffff));
        }
    } else if (-0x190000 < car->auto_steer) {
        car->auto_steer -= 0x30000;
    }
    car->steer = car->auto_steer;
    car->auto_mode = 2;
}

/* 00401180: before the start and after the finish the car slides to the
 * nearer verge on the handbrake, then stops across the road. */
static void autopilot(Car *car, const RaceState *race)
{
    const TriNode n = race_node(race, car->node);
    const int32_t heading = n.orientation * 0x400;
    int32_t off = abs32(heading - car->yaw);
    if (off > ANGLE_HALF) off = ANGLE_TURN - off;
    off = abs32(off - ANGLE_QUARTER);
    if (off < 0x80000 || car->auto_mode == 2) {
        auto_stop(car);
    } else {
        if (car->auto_mode == 0) {
            car->auto_steer = 0;
            const int32_t a = heading >> 8;
            car->auto_offset = ((n.position.z - car->z) >> 8) * (fx_sin16(a) >> 8) -
                               (fx_cos16(a) >> 8) * ((n.position.x - car->x) >> 8);
            car->auto_ticks = 0;
        }
        car->throttle = 0;
        car->handbrake = 1;
        car->auto_mode = 1;
        const int32_t left = abs32(n.left_barrier * VERGE_UNIT + car->auto_offset);
        const int32_t right = abs32(car->auto_offset - n.right_barrier * VERGE_UNIT);
        const int lock_right = left < right ? car->auto_ticks > 5 : car->auto_ticks < 6;
        car->steer = lock_right ? AUTO_LOCK : -AUTO_LOCK;
    }
    car->auto_ticks += 1;
    car->local_velocity_x -= car->local_velocity_x / 8;
    car->local_velocity_z -= car->local_velocity_z / 8;
}

void car_player_control(Car *car, const CarSpecs *specs, RaceState *race)
{
    int32_t aim = road_aim(car, race);
    if (abs32(aim) > AIM_MAX) aim = 0;
    const uint32_t input = race->inputs[car->player & (RACE_PLAYERS - 1)];
    const ControlSetup *setup = &race->controls[car->player & (RACE_PLAYERS - 1)];
    if (setup->analog_steering == 0) {
        key_steering(car, specs, input, aim);
    } else {
        analog_steering(car, input, aim);
    }
    if (setup->pedals == 1) {
        analog_pedals(car, specs, race, input);
    } else {
        key_pedals(car, specs, race, input);
    }
    if (car_race_window(car, race) != 0) {
        autopilot(car, race);
    } else {
        car->auto_mode = 0;
    }
}

int32_t car_past_finish(const Car *car, const RaceState *race)
{
    if (race->circuit == 0) return car->node > race->loop_nodes ? car->node - race->loop_nodes : 0;
    return car->lap > race->laps ? car->node : 0;
}
