/* Joysticks and their force feedback on SDL3 (platform.h): the exe's WinMM
 * joystick calls and IFORCE.DLL. The joystick subsystem starts on the first
 * call; the haptic one (slow to enumerate) only when a stick that has it
 * opens its forces, so a game without one starts as it did. */
#include "platform/platform.h"

#include <SDL3/SDL.h>
#include <string.h>

enum {
    MAX_DEVICES = 16,
    AXES = 4,
    AXIS_MIDDLE = 0x8000,
    PERCENT = 100,
    LEVEL_MAX = 0x7fff,
    RUMBLE_MAX = 0xffff,
    CENTI_DEGREES = 100,
    BUFFETING_PERIOD_MS = 60,  /* the buffeting's wave: a knock felt about 16 times a second */
    BUFFETING_RUMBLE_MS = 30000,
    VIBRATION_RUMBLE_MS = 30000,
    MS_PER_SECOND = 1000,
    VIBRATIONS = 2
};

static bool started, start_failed, haptic_started, haptic_failed;
static SDL_JoystickID ids[MAX_DEVICES];
static SDL_Joystick *opened[MAX_DEVICES];
static int devices;

/* the force-feedback device: a haptic one, else a joystick that rumbles */
static SDL_Haptic *haptic;
static SDL_Joystick *rumbler;
static SDL_HapticEffectID jolt_effect = -1, buffeting_effect = -1, vector_effect = -1;
static SDL_HapticEffectID vibration_effect[VIBRATIONS] = { -1, -1 };
static Uint16 rumble_low, rumble_high;  /* what the buffeting and the vibrations rumble with while they run */

static bool start(void)
{
    if (started) return !start_failed;
    started = true;
    if (!SDL_InitSubSystem(SDL_INIT_JOYSTICK)) {
        start_failed = true;
        return false;
    }
    int count = 0;
    SDL_JoystickID *list = SDL_GetJoysticks(&count);
    if (list != NULL) {
        devices = count < MAX_DEVICES ? count : MAX_DEVICES;
        memcpy(ids, list, sizeof ids[0] * (size_t)devices);
        SDL_free(list);
    }
    return true;
}

static bool start_haptic(void)
{
    if (!haptic_started) haptic_started = true, haptic_failed = !SDL_InitSubSystem(SDL_INIT_HAPTIC);
    return !haptic_failed;
}

static SDL_Joystick *device_of(int device)
{
    if (!start() || device < 0 || device >= devices) return NULL;
    if (opened[device] == NULL) opened[device] = SDL_OpenJoystick(ids[device]);
    return opened[device];
}

int platform_joy_count(void) { return start() ? devices : 0; }

bool platform_joy_caps(int device, char *name, size_t name_size, int *buttons, uint32_t *axes)
{
    SDL_Joystick *j = device_of(device);
    if (j == NULL) return false;
    if (name != NULL && name_size > 0) {
        const char *n = SDL_GetJoystickName(j);
        SDL_strlcpy(name, n != NULL ? n : "", name_size);
    }
    if (buttons != NULL) *buttons = SDL_GetNumJoystickButtons(j);
    if (axes != NULL) {
        const int n = SDL_GetNumJoystickAxes(j);
        *axes = 0;
        for (int k = 0; k < AXES && k < n; k++) *axes |= 1u << k;
    }
    return true;
}

bool platform_joy_read(int device, uint32_t *buttons, int32_t axis[4])
{
    SDL_Joystick *j = device_of(device);
    if (j == NULL) return false;
    SDL_UpdateJoysticks();
    const int n = SDL_GetNumJoystickButtons(j);
    *buttons = 0;
    for (int k = 0; k < n && k < 32; k++)
        if (SDL_GetJoystickButton(j, k)) *buttons |= 1u << k;
    const int axes = SDL_GetNumJoystickAxes(j);
    for (int k = 0; k < AXES; k++) axis[k] = AXIS_MIDDLE + (k < axes ? SDL_GetJoystickAxis(j, k) : 0);
    return true;
}

static Sint16 level_of(int percent)
{
    const int p = percent < -PERCENT ? -PERCENT : percent > PERCENT ? PERCENT : percent;
    return (Sint16)(p * LEVEL_MAX / PERCENT);
}

static Uint16 rumble_of(int percent)
{
    const int p = percent < 0 ? 0 : percent > PERCENT ? PERCENT : percent;
    return (Uint16)(p * RUMBLE_MAX / PERCENT);
}

/* an effect made (or remade) in its slot and run once */
static void run_effect(SDL_HapticEffectID *slot, const SDL_HapticEffect *e)
{
    if (haptic == NULL || !SDL_HapticEffectSupported(haptic, e)) return;
    if (*slot >= 0 && !SDL_UpdateHapticEffect(haptic, *slot, e)) {
        SDL_DestroyHapticEffect(haptic, *slot);
        *slot = -1;
    }
    if (*slot < 0) *slot = SDL_CreateHapticEffect(haptic, e);
    if (*slot >= 0) SDL_RunHapticEffect(haptic, *slot, 1);
}

static void stop_effect(SDL_HapticEffectID slot)
{
    if (haptic != NULL && slot >= 0) SDL_StopHapticEffect(haptic, slot);
}

static void rumble(Uint16 low, Uint16 high, Uint32 ms)
{
    if (rumbler != NULL) SDL_RumbleJoystick(rumbler, low, high, ms);
}

bool platform_forces_open(int device)
{
    platform_forces_close();
    SDL_Joystick *j = device_of(device);
    if (j == NULL) return false;
    if (start_haptic() && SDL_IsJoystickHaptic(j)) haptic = SDL_OpenHapticFromJoystick(j);
    if (haptic != NULL && (SDL_GetHapticFeatures(haptic) & (SDL_HAPTIC_CONSTANT | SDL_HAPTIC_SINE)) != 0)
        return true;
    if (haptic != NULL) SDL_CloseHaptic(haptic), haptic = NULL;
    if (SDL_GetBooleanProperty(SDL_GetJoystickProperties(j), SDL_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN, false)) {
        rumbler = j;
        return true;
    }
    return false;
}

void platform_forces_close(void)
{
    platform_forces_clear();
    if (haptic != NULL) SDL_CloseHaptic(haptic);
    haptic = NULL;
    rumbler = NULL;
    jolt_effect = buffeting_effect = vector_effect = -1;
    vibration_effect[0] = vibration_effect[1] = -1;
}

void platform_forces_jolt(int strength, int direction, int ms)
{
    if (haptic != NULL) {
        SDL_HapticEffect e;
        SDL_zero(e);
        e.constant.type = SDL_HAPTIC_CONSTANT;
        e.constant.direction.type = SDL_HAPTIC_POLAR;
        e.constant.direction.dir[0] = direction * CENTI_DEGREES;
        e.constant.length = (Uint32)(ms > 0 ? ms : 0);
        e.constant.level = level_of(strength);
        run_effect(&jolt_effect, &e);
        return;
    }
    rumble(rumble_of(strength), rumble_of(strength), (Uint32)(ms > 0 ? ms : 0));
}

void platform_forces_buffeting(int level)
{
    if (haptic != NULL) {
        SDL_HapticEffect e;
        SDL_zero(e);
        e.periodic.type = SDL_HAPTIC_SINE;
        e.periodic.direction.type = SDL_HAPTIC_CARTESIAN;
        e.periodic.direction.dir[0] = 1;
        e.periodic.length = SDL_HAPTIC_INFINITY;
        e.periodic.period = BUFFETING_PERIOD_MS;
        e.periodic.magnitude = level_of(level);
        run_effect(&buffeting_effect, &e);
        return;
    }
    rumble_low = rumble_of(level);
    rumble(rumble_low, rumble_high, BUFFETING_RUMBLE_MS);
}

void platform_forces_vibration(int axis, int a, int b, int c)
{
    if (axis < 0 || axis >= VIBRATIONS) return;
    const int strength = a > b ? a : b;
    if (haptic != NULL) {
        SDL_HapticEffect e;
        SDL_zero(e);
        e.periodic.type = SDL_HAPTIC_SINE;
        e.periodic.direction.type = SDL_HAPTIC_CARTESIAN;
        e.periodic.direction.dir[axis] = 1;
        e.periodic.length = SDL_HAPTIC_INFINITY;
        e.periodic.period = (Uint16)(MS_PER_SECOND / (c > 0 ? c : 1));
        e.periodic.magnitude = level_of(strength);
        run_effect(&vibration_effect[axis], &e);
        return;
    }
    rumble_high = rumble_of(strength);
    rumble(rumble_low, rumble_high, VIBRATION_RUMBLE_MS);
}

void platform_forces_vibration_clear(int axis)
{
    if (axis < 0 || axis >= VIBRATIONS) return;
    if (haptic != NULL) {
        stop_effect(vibration_effect[axis]);
        return;
    }
    rumble_high = 0;
    rumble(rumble_low, rumble_high, rumble_low != 0 ? BUFFETING_RUMBLE_MS : 0);
}

void platform_forces_vector(int x, int y)
{
    if (haptic == NULL) return;  /* a steady push has no rumble to stand for it */
    SDL_HapticEffect e;
    SDL_zero(e);
    e.constant.type = SDL_HAPTIC_CONSTANT;
    e.constant.direction.type = SDL_HAPTIC_CARTESIAN;
    e.constant.direction.dir[0] = x;
    e.constant.direction.dir[1] = y;
    e.constant.length = SDL_HAPTIC_INFINITY;
    const int big = SDL_abs(x) > SDL_abs(y) ? SDL_abs(x) : SDL_abs(y);
    e.constant.level = level_of(big);
    run_effect(&vector_effect, &e);
}

void platform_forces_enable(bool on)
{
    if (haptic == NULL || (SDL_GetHapticFeatures(haptic) & SDL_HAPTIC_PAUSE) == 0) return;
    if (on) SDL_ResumeHaptic(haptic);
    else SDL_PauseHaptic(haptic);
}

void platform_forces_clear(void)
{
    if (haptic != NULL) SDL_StopHapticEffects(haptic);
    rumble_low = rumble_high = 0;
    rumble(0, 0, 0);
}
