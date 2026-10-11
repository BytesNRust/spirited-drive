#include "game/race/car_load.h"

#include "formats/pbs.h"

#include "game/race/car_setup.h"
#include "game/race/car_specs_word.h"
#include "game/sys/fixed_math.h"

#include <string.h>

enum {
    START_Z = 0x600000,
    START_NODE = 0x10,
    CONTROLLER_PLAYER = 1,
    PHYSICS_DRIVING = 2,
    TRANSMISSION_AUTOMATIC = 2,
    AUTOMATIC_POWER = 0xf8,
    ARCADE_POWER = 0x133,
    ARCADE_POWER_STRIP = 0x14c,   /* track 3, the drag strip */
    TORQUE_STRIDE = 8
};

typedef char pbs_matches_car_specs[PBS_SIZE == CAR_SPECS_SIZE ? 1 : -1];

const char *const car_model_names[CAR_MODELS] = {
    "TSupra", "LDiabl", "P911", "CZR1", "F512TR", "DVIPER", "ANSX", "MRX7", "MRX7", "MRX7", "P911T", "Traffc"
};

int car_specs_read(CarSpecs *specs, const uint8_t *file, size_t size)
{
    uint8_t record[PBS_SIZE];
    if (!pbs_read(record, file, size)) return 0;
    memcpy(specs, record, sizeof *specs);
    return 1;
}

/* Arcade mode: the rear tyres grip as the front, the axles' weights and
 * some forces halved, and more power. */
static void arcade_tuning(CarSpecs *specs, int32_t track)
{
    memcpy(specs->grip_rear, specs->grip_front, sizeof specs->grip_rear);
    specs->unknown_320 /= 2;
    specs->unknown_324 = specs->unknown_320 == 0 ? 0 : 0x10000 / specs->unknown_320;
    specs->force_scale /= 2;
    specs->unknown_32c = specs->force_scale == 0 ? 0 : 0x10000 / specs->force_scale;
    specs->unknown_2dc /= 2;
    specs->unknown_2e0 /= 2;
    specs->unknown_028 /= 2;
    specs->unknown_02c /= 2;
    specs->unknown_024 = specs->unknown_024 * 3 / 2;
    specs->power = specs->power * (track == 3 ? ARCADE_POWER_STRIP : ARCADE_POWER) / 256;
}

void car_specs_scale_torque(CarSpecs *specs, int automatic)
{
    specs->power = specs->power * (automatic ? AUTOMATIC_POWER : 0x100) / 256;
    const int32_t first = (int32_t)offsetof(CarSpecs, torque_curve);
    for (int32_t i = 0; i < specs->torque_points; ++i) {
        const int32_t at = first + i * TORQUE_STRIDE;
        const int32_t torque = car_specs_word(specs, at) << 16;
        const int32_t scaled = fx_mul_round(fx_mul_round(fx_mul_round(torque, specs->unknown_05c),
                                                         fx_mul_round(specs->power, specs->unknown_064)),
                                            specs->mass);
        car_specs_set_word(specs, at, scaled);
    }
}

/* The entry's name, up to and with its terminator. */
static void copy_name(Car *car, const char *name)
{
    for (size_t i = 0; i < sizeof car->driver_name; ++i) {
        car->driver_name[i] = (uint8_t)name[i];
        if (name[i] == '\0') break;
    }
}

/* entries[slot].car as 004406d0 reads it (004fb803 + 0x4b x slot): slot 8
 * reads 9 bytes past the eight entries, into the grid places (004fba5b:
 * the top byte of grid[2], the low three of grid[3]). */
static int32_t entry_car(const RaceState *race, int32_t slot)
{
    if (slot >= 0 && slot < RACE_PLAYERS) return race->entries[slot].car;
    if (slot == RACE_PLAYERS) return (int32_t)((uint32_t)race->grid[2] >> 24 | (uint32_t)race->grid[3] << 8);
    return 0;  /* port guard: further slots read other globals */
}

void car_load(Car *car, CarSpecs *specs, RaceState *race)
{
    const RaceEntry *entry = &race->entries[car->index];
    if (car == race->cars[0]) race->leader = car;
    car->controller = CONTROLLER_PLAYER;
    car->physics = PHYSICS_DRIVING;
    car->z = START_Z;
    car->node = START_NODE;
    car->progress = START_NODE;
    car->lap = 1;
    if (race->specs_loaded == 0) {
        if (!pbs_checksum_ok((const uint8_t *)specs)) specs->power = 0;
        if (race->arcade != 0) arcade_tuning(specs, race->track);
    }
    car_specs_scale_torque(specs, entry->automatic != 0);
    car->half_length = specs->half_length;
    car->half_width = specs->half_width;
    car->anti_lock = entry->anti_lock > 0 && specs->unknown_334 > 0;
    car->traction_control = entry->traction > 0 && specs->unknown_338 > 0;
    car->downforce_a = 0;
    car->downforce_b = 0;
    car->model = entry->car;
    copy_name(car, entry->name);
    car->transmission = entry->automatic != 0 ? TRANSMISSION_AUTOMATIC : 0;
    car_setup_physics(car, specs, race);
}

enum {
    START_X = 0x10000,
    BODY = 0xc4,                  /* the collision body inside the record */
    COMPUTER_CONTROLLER = 2,
    COMPUTER_PHYSICS = 3,
    COMPUTER_HALF_LENGTH = 0x50000,
    COMPUTER_HALF_WIDTH = 0x28000,
    COMPUTER_Z = 0x10000,
    ENTERED = 3                   /* flags_51a */
};

void car_keep_draw_state(Car *car)
{
    car->draw_node = car->node;
    car->draw_node2 = car->node;
    car->draw_unknown_349 = car->lateral;
    car->draw_pitch = car->pitch;
    car->heading = car->yaw;
    car->draw_roll = car->roll;
}

void car_reset(Car *car, uint32_t handle)
{
    car->speed = 0;
    car->x = START_X;
    car->y = 0;
    car->lap = 1;
    car->velocity_x = car->velocity_y = car->velocity_z = 0;
    car->local_velocity_x = car->local_velocity_y = car->local_velocity_z = 0;
    car->wheel_speed = 0;
    car->yaw_rate = 0;
    car->skid_front = car->skid_rear = 0;
    car->unknown_40d = 0;
    car->target_rpm = 1;
    car->rpm = 1;
    car->drive_force = 0;
    car->last_gear = 0;
    car->gear_change_ticks = 0;
    car->steer = 0;
    car->slip_steer = 0;
    car->lane_bias = 0;
    car->throttle = car->saved_throttle = car->brake = 0;
    car->handbrake = 0;
    car->gear = -1;
    car->stalled = 0;
    car->engaged = 0;
    car->shift_timer = -1;
    car->on_ground = 1;
    car->airborne_ticks = 0;
    car->zone = 0;
    car->ai_speed = car->ai_target_speed = car->ai_target_lateral = 0;
    car->use_matrix = 0;
    memset(car->road_axes, 0, sizeof car->road_axes);
    memset(car->road_centre, 0, sizeof car->road_centre);
    memset(car->matrix, 0, sizeof car->matrix);
    car->unknown_439 = 0;
    car->unknown_431 = 0x111eb;
    car->unknown_435 = 0x111eb;
    car->gear_key = 0;
    car->pitch = car->roll = car->yaw = 0;
    car->tilt_z = car->tilt_x = 0;
    car->unknown_3d9 = car->unknown_3dd = car->unknown_3e1 = 0;
    car->scenery_node = car->scenery_index = 0;
    car->crashed_hard = 0;
    car->flags_174 = car->flags_175 = 0;
    memset(car->flags_176, 0, sizeof car->flags_176);
    car->unknown_088 = handle;
    car->unknown_0c0 = handle + BODY;
    car->body_extent_a = 0x457;
    car->body_extent_b = 0x8ae;
    car->body_extent_c = 0xd05;
    car->unknown_340 = 0;
    car->surface = car->off_road = 0;
    car->node_flag_4aa = car->node_flag_4ae = 0;
    car->traction_active = car->anti_lock_active = 0;
    car->suspension_z_index = car->suspension_x_index = 0;
    car->suspension_z = car->suspension_x = 0;
    car->unknown_4d2 = 0x40;
    memset(car->unknown_4d6, 0, sizeof car->unknown_4d6);
    memset(car->unknown_4be, 0, sizeof car->unknown_4be);
    car->node_flag_4c6 = car->node_flag_4ca = car->node_flag_4ce = 0;
    car->auto_offset = car->auto_ticks = car->auto_mode = car->auto_steer = 0;
    car->lateral = car->index * 10;
}

void car_enter(Car *car, CarSpecs *specs, uint32_t handle, RaceState *race)
{
    car->flags_51a = ENTERED;
    memset(car->unknown_51b, 0, sizeof car->unknown_51b);
    if (car == race->cars[1]) race->second_car = car;
    if (car->index < 0 || car->index >= race->humans) {
        car->controller = COMPUTER_CONTROLLER;
        car->physics = COMPUTER_PHYSICS;
        car->half_length = COMPUTER_HALF_LENGTH;
        car->half_width = COMPUTER_HALF_WIDTH;
        car->z = COMPUTER_Z;
        car->node = 0;
        car->progress = 0;
        car->unknown_3a1 = 0;
        car->unknown_3ad = 0;
        if (race->player == 0) car->model = entry_car(race, car->index);
    } else {
        car_load(car, specs, race);
    }
    car_reset(car, handle);
    car_keep_draw_state(car);
}
