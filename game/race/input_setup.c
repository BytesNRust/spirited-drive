/* The followed player's controls from config.dat (input_setup.h). */
#include "game/race/input_setup.h"

#include "game/sys/key_codes.h"

enum { PAD = 1, MOUSE = 2, PAD_BUTTONS = 3, FIRST_MOUSE = 4, FIRST_KEY = 3 };

static int32_t steering_device(int16_t word)
{
    if (word == 0) return 0;
    return word >= FIRST_MOUSE ? MOUSE : PAD;
}

static int32_t button_device(int16_t word)
{
    if (word == 0) return 0;
    if (word >= FIRST_MOUSE) return MOUSE;
    return word == PAD_BUTTONS ? PAD_BUTTONS : PAD;
}

void input_setup(const int16_t *config, int16_t german, int32_t player, ControlSetup *controls, int32_t *codes,
                 uint8_t *w4fb788)
{
    /* config word 3 + i goes to codes[SLOT[i]]: the hold key's word comes before the handbrake's */
    static const int SLOT[INPUT_KEY_CODES] = { 0, 1, 2, 3, 4, 5, 6, INPUT_KEY_HOLD, 7, INPUT_KEY_PARTS };
    ControlSetup *setup = &controls[player];
    setup->analog_steering = steering_device(config[0]);
    setup->pedals = button_device(config[1]);
    setup->gears = button_device(config[2]);
    for (int i = 0; i < INPUT_KEY_CODES; i++)
        codes[SLOT[i]] = key_code((uint16_t)key_layout(config[FIRST_KEY + i], german));
    *w4fb788 = 0;
}
