#include "game/race/car_load.h"

#include "game/race/car_setup.h"
#include "game/race/car_specs_word.h"
#include "game/sys/fixed_math.h"

#include <string.h>

enum {
    CHECKED_BYTES = 0x758,
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

const char *const car_model_names[CAR_MODELS] = {
    "TSupra", "LDiabl", "P911", "CZR1", "F512TR", "DVIPER", "ANSX", "MRX7", "MRX7", "MRX7", "P911T", "Traffc"
};

static int32_t checksum(const CarSpecs *specs)
{
    const uint8_t *bytes = (const uint8_t *)specs;
    int32_t sum = 0;
    for (int i = 0; i < CHECKED_BYTES; ++i) sum += bytes[i];
    return sum;
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
        if (checksum(specs) != specs->checksum) specs->power = 0;
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
