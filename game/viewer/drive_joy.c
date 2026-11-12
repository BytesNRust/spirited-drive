/* The joystick and its force-feedback stick (drive_joy.h). */
#include "viewer/drive_joy.h"

#include "platform/platform.h"
#include "game/frontend/save_files.h"
#include "game/race/race_camera.h"
#include "game/race/race_forces.h"
#include "game/sys/port_follow.h"
#include "game/sys/port_settings.h"
#include "game/sys/joy_calibrate.h"
#include "game/sys/joy_pad.h"
#include "game/sys/resource.h"

#include <stdbool.h>

enum { AXIS_MIN = 0, AXIS_MIDDLE = 0x8000, AXIS_MAX = 0xffff, Z_AXIS = 2, R_AXIS = 3 };

static JoyState joy;
static JoyStart start;
static RaceForces forces;
static bool started, forces_open;
static RaceState *quiet_race;   /* port (replay_no_ffb): the race whose replay leaves the stick still */
static int replay_no_ffb, was_quiet;

/* a replay that starts silent clears what was already running (a vibration holds until cleared), once */
static bool quiet(void)
{
    const int silent = race_forces_replay_silent(quiet_race, replay_no_ffb);
    if (race_forces_edge(&was_quiet, silent)) race_forces_clear(quiet_race);
    return silent != 0;
}

static int32_t joy_read(void *ctx, int32_t device, uint32_t *buttons, int32_t axis[JOY_AXES])
{
    (void)ctx;
    return platform_joy_read(device, buttons, axis) ? 1 : 0;
}

static const JoyOps JOY = { NULL, joy_read };

/* 0043bc00's place: the stick measured as resting at its mids, with the
 * whole of each axis's travel */
static void calibrate(void *ctx, int32_t mode)
{
    (void)ctx, (void)mode;
    uint32_t buttons = 0;
    int32_t rest[JOY_AXES] = { AXIS_MIDDLE, AXIS_MIDDLE, AXIS_MIDDLE, AXIS_MIDDLE };
    if (!platform_joy_read(start.device, &buttons, rest))
        for (int k = 0; k < JOY_AXES; k++) rest[k] = AXIS_MIDDLE;
    joy.x_min = joy.y_min = joy.z_min = joy.r_min = AXIS_MIN;
    joy.x_max = joy.y_max = joy.z_max = joy.r_max = AXIS_MAX;
    joy.x_mid = rest[0], joy.y_mid = rest[1], joy.z_mid = rest[Z_AXIS];
    joy_calibrate(&joy, start.has_axis[Z_AXIS] != 0 || start.has_axis[R_AXIS] != 0);
    start.present = 1;
}

/* 0043cb50: the stick's device rather than ports 1..4 */
static void open_forces(void *ctx)
{
    (void)ctx;
    if (!forces_open) forces_open = platform_forces_open(start.device);
}

static int32_t count(void *ctx)
{
    (void)ctx;
    return platform_joy_count();
}

static int32_t caps(void *ctx, int32_t device, char *name, int32_t *buttons, uint32_t *flags)
{
    (void)ctx;
    int n = 0;
    if (!platform_joy_caps(device, name, JOY_NAME_SIZE, &n, flags)) return 0;
    *buttons = n;
    return 1;
}

void drive_joy_start(int16_t *controls, int16_t german)
{
    if (started) return;
    started = true;
    const JoyStartOps ops = { NULL, calibrate, open_forces, count, caps };
    joy_start(&start, &joy, controls, german, resource_dir(PATH_CONFIG), &save_files_ops, &JOY, &ops);
    start.brake_wins = port_brake_over_gas();   /* port (brake_over_gas) */
}

const JoyStart *drive_joy_state(void) { return &start; }

void drive_joy_menu(JoyState **j, int32_t *present, int32_t *device, const JoyOps **ops)
{
    *j = &joy, *present = start.present, *device = start.device, *ops = &JOY;
}

void drive_joy_pad_read(InputPad *pad)
{
    if (forces_open) (void)quiet();   /* port (replay_no_ffb): the edge, seen even when nothing asks for a force */
    static PortFollow brake;   /* port (brake_over_gas): the start runs once a game; the mode's or the row's change with no restart is followed */
    start.brake_wins = port_follow(&brake, port_brake_over_gas);
    joy_pad_read(&joy, &start, &JOY);
    pad->x = joy.pad_x;
    pad->y = joy.pad_y;
    pad->buttons = (uint8_t)joy.pad_buttons;
}

/* RaceSounds' force ops, user the race */
static void jolt_tone(void *user, int32_t strength, int32_t direction, int32_t ms)
{
    (void)user;
    if (quiet()) return;
    platform_forces_jolt(strength, direction, ms);
}

static void buffet(void *user, int32_t speed, int32_t kick)
{
    (void)speed, (void)kick;
    if (quiet()) return;
    race_buffet(user, 1);
}

static void jolt(void *user, int32_t car_a, int32_t car_b, int32_t x, int32_t y, int32_t z, int32_t impulse)
{
    (void)car_b, (void)y, (void)z;
    const RaceState *race = user;
    race_jolt(&forces, race->start_clock, race->viewed, race->cars[car_a]->player, x, impulse, &race->sounds);
}

static void vector(void *user, int32_t x, int32_t y)
{
    (void)user;
    if (!quiet()) platform_forces_vector(x, y);
}
static void release(void *user) { race_buffet(user, 0); }

static void vibration(void *user, int32_t axis, int32_t a, int32_t b, int32_t c)
{
    (void)user;
    if (quiet()) return;
    platform_forces_vibration(axis, a, b, c);
}

static void vibration_clear(void *user, int32_t axis) { (void)user; platform_forces_vibration_clear(axis); }
static void enable(void *user) { (void)user; platform_forces_enable(true); }
static void disable(void *user) { (void)user; platform_forces_enable(false); }
static void clear(void *user) { (void)user; platform_forces_clear(); }
static void buffeting(void *user, int32_t level)
{
    (void)user;
    if (!quiet()) platform_forces_buffeting(level);
}

void drive_joy_install(RaceState *race)
{
    quiet_race = race;
    was_quiet = 0;
    replay_no_ffb = port_replay_no_ffb();
    race->effects = forces_open ? 1 : 0;
    if (!forces_open) return;
    race->sounds.tone = jolt_tone;
    race->sounds.buffet = buffet;
    race->sounds.jolt = jolt;
    race->sounds.force = vector;
    race->sounds.force_release = release;
    race->sounds.vibration = vibration;
    race->sounds.vibration_clear = vibration_clear;
    race->sounds.forces_enable = enable;
    race->sounds.forces_disable = disable;
    race->sounds.forces_clear = clear;
    race->sounds.buffeting = buffeting;
}
