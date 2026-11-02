#include "game/sys/key_latch.h"

static unsigned char steps_left[KEY_LATCH_KEYS], pumps_left[KEY_LATCH_KEYS];

void key_latch_press(int key)
{
    if (key > 0 && key < KEY_LATCH_KEYS) steps_left[key] = KEY_LATCH_STEPS, pumps_left[key] = KEY_LATCH_PUMPS;
}

bool key_latch_down(int key) { return key > 0 && key < KEY_LATCH_KEYS && steps_left[key] != 0; }

void key_latch_pump(void)
{
    for (int i = 0; i < KEY_LATCH_KEYS; i++)
        if (pumps_left[i] != 0 && --pumps_left[i] == 0) steps_left[i] = 0;
}

void key_latch_step(void)
{
    for (int i = 0; i < KEY_LATCH_KEYS; i++)
        if (steps_left[i] != 0) steps_left[i]--;
}

void key_latch_clear(void)
{
    for (int i = 0; i < KEY_LATCH_KEYS; i++) steps_left[i] = pumps_left[i] = 0;
}
