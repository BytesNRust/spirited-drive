/* The platform's set-1 keys: every letter and digit the game names (key_code, 00487b84) is read from its
 * own SDL key, so the cheat codes Control Central steps (00437258) can be typed. No window is opened. */
#include "platform/platform.h"
#include "sys/key_codes.h"
#include "test.h"

#include <SDL3/SDL.h>

int main(void)
{
    for (int c = 'A'; c <= 'Z'; c++) {
        const int32_t code = key_code((uint16_t)c);
        CHECK(code != 0);
        CHECK_EQ_INT(platform_key_sdl(code), SDL_SCANCODE_A + (c - 'A'));
    }
    CHECK_EQ_INT(platform_key_sdl(key_code('1')), SDL_SCANCODE_1);
    CHECK_EQ_INT(platform_key_sdl(key_code('0')), SDL_SCANCODE_0);
    CHECK_EQ_INT(platform_key_sdl(0x01), SDL_SCANCODE_ESCAPE);
    CHECK_EQ_INT(platform_key_sdl(0x48), SDL_SCANCODE_UP);
    CHECK_EQ_INT(platform_key_sdl(0x7f), SDL_SCANCODE_UNKNOWN);
    return TEST_REPORT();
}
