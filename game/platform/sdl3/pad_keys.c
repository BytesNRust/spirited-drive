/* port: the PC set-1 scancodes as SDL's (sdl3_from_set1), the key names, and the keys a pad's or wheel's bindings press.
 * Moved out of platform_sdl3.c. */
#include "platform/platform.h"

#include "platform/sdl3/pad_keys.h"

#include <SDL3/SDL.h>

SDL_Scancode sdl3_from_set1(int scancode)
{
    switch (scancode) {
    case 0x01: return SDL_SCANCODE_ESCAPE;
    case 0x02: return SDL_SCANCODE_1;
    case 0x03: return SDL_SCANCODE_2;
    case 0x04: return SDL_SCANCODE_3;
    case 0x05: return SDL_SCANCODE_4;
    case 0x06: return SDL_SCANCODE_5;
    case 0x07: return SDL_SCANCODE_6;
    case 0x08: return SDL_SCANCODE_7;
    case 0x09: return SDL_SCANCODE_8;
    case 0x0A: return SDL_SCANCODE_9;
    case 0x0B: return SDL_SCANCODE_0;
    case 0x0C: return SDL_SCANCODE_MINUS;
    case 0x0D: return SDL_SCANCODE_EQUALS;
    case 0x0E: return SDL_SCANCODE_BACKSPACE;
    case 0x0F: return SDL_SCANCODE_TAB;
    case 0x10: return SDL_SCANCODE_Q;
    case 0x11: return SDL_SCANCODE_W;
    case 0x12: return SDL_SCANCODE_E;
    case 0x13: return SDL_SCANCODE_R;
    case 0x14: return SDL_SCANCODE_T;
    case 0x15: return SDL_SCANCODE_Y;
    case 0x16: return SDL_SCANCODE_U;
    case 0x17: return SDL_SCANCODE_I;
    case 0x18: return SDL_SCANCODE_O;
    case 0x19: return SDL_SCANCODE_P;
    case 0x1A: return SDL_SCANCODE_LEFTBRACKET;
    case 0x1B: return SDL_SCANCODE_RIGHTBRACKET;
    case 0x1C: return SDL_SCANCODE_RETURN;
    case 0x1D: return SDL_SCANCODE_LCTRL;
    case 0x1E: return SDL_SCANCODE_A;
    case 0x1F: return SDL_SCANCODE_S;
    case 0x20: return SDL_SCANCODE_D;
    case 0x21: return SDL_SCANCODE_F;
    case 0x22: return SDL_SCANCODE_G;
    case 0x23: return SDL_SCANCODE_H;
    case 0x24: return SDL_SCANCODE_J;
    case 0x25: return SDL_SCANCODE_K;
    case 0x26: return SDL_SCANCODE_L;
    case 0x27: return SDL_SCANCODE_SEMICOLON;
    case 0x28: return SDL_SCANCODE_APOSTROPHE;
    case 0x29: return SDL_SCANCODE_GRAVE;
    case 0x2A: return SDL_SCANCODE_LSHIFT;
    case 0x2B: return SDL_SCANCODE_BACKSLASH;
    case 0x2C: return SDL_SCANCODE_Z;
    case 0x2D: return SDL_SCANCODE_X;
    case 0x2E: return SDL_SCANCODE_C;
    case 0x2F: return SDL_SCANCODE_V;
    case 0x30: return SDL_SCANCODE_B;
    case 0x31: return SDL_SCANCODE_N;
    case 0x32: return SDL_SCANCODE_M;
    case 0x33: return SDL_SCANCODE_COMMA;
    case 0x34: return SDL_SCANCODE_PERIOD;
    case 0x35: return SDL_SCANCODE_SLASH;
    case 0x36: return SDL_SCANCODE_RSHIFT;
    case 0x37: return SDL_SCANCODE_KP_MULTIPLY;
    case 0x38: return SDL_SCANCODE_LALT;
    case 0x39: return SDL_SCANCODE_SPACE;
    case 0x3A: return SDL_SCANCODE_CAPSLOCK;
    case 0x3B: return SDL_SCANCODE_F1;
    case 0x3C: return SDL_SCANCODE_F2;
    case 0x3D: return SDL_SCANCODE_F3;
    case 0x3E: return SDL_SCANCODE_F4;
    case 0x3F: return SDL_SCANCODE_F5;
    case 0x40: return SDL_SCANCODE_F6;
    case 0x41: return SDL_SCANCODE_F7;
    case 0x42: return SDL_SCANCODE_F8;
    case 0x43: return SDL_SCANCODE_F9;
    case 0x44: return SDL_SCANCODE_F10;
    case 0x45: return SDL_SCANCODE_NUMLOCKCLEAR;
    case 0x46: return SDL_SCANCODE_SCROLLLOCK;
    case 0x47: return SDL_SCANCODE_HOME;
    case 0x48: return SDL_SCANCODE_UP;
    case 0x49: return SDL_SCANCODE_PAGEUP;
    case 0x4A: return SDL_SCANCODE_KP_MINUS;
    case 0x4B: return SDL_SCANCODE_LEFT;
    case 0x4D: return SDL_SCANCODE_RIGHT;
    case 0x4E: return SDL_SCANCODE_KP_PLUS;
    case 0x4F: return SDL_SCANCODE_END;
    case 0x50: return SDL_SCANCODE_DOWN;
    case 0x51: return SDL_SCANCODE_PAGEDOWN;
    case 0x52: return SDL_SCANCODE_INSERT;
    case 0x53: return SDL_SCANCODE_DELETE;
    case 0x57: return SDL_SCANCODE_F11;
    case 0x58: return SDL_SCANCODE_F12;
    default: return SDL_SCANCODE_UNKNOWN;
    }
}

int platform_key_sdl(int scancode) { return (int)sdl3_from_set1(scancode); }

bool platform_key_name(int scancode, char *out, size_t size)
{
    if (size == 0) return false;
    out[0] = '\0';
    const SDL_Scancode sc = sdl3_from_set1(scancode);
    const char *name = sc != SDL_SCANCODE_UNKNOWN ? SDL_GetScancodeName(sc) : NULL;
    if (name == NULL || name[0] == '\0') return false;
    size_t n = 0;
    for (; name[n] != '\0' && n + 1 < size; n++) out[n] = (char)SDL_toupper((unsigned char)name[n]);
    out[n] = '\0';
    return true;
}

static void (*joy_refresh)(void);  /* port: reads the open pads' keys again (platform_joy_virtual_refresher) */

/* port: the keys a pad's or wheel's bindings press (platform.h): what each device holds down as of its last read, and
 * the scancode each action's key is */
enum { VIRTUAL_DEVICES = 16 };
static uint32_t virtual_keys[VIRTUAL_DEVICES];
static int virtual_scancode[ACT_COUNT] = { [ACT_PAUSE] = 0x19 };
void platform_joy_virtual_refresher(void (*refresh)(void)) { joy_refresh = refresh; }

void platform_virtual_key(int action, int scancode)
{
    if (action >= 0 && action < ACT_COUNT) virtual_scancode[action] = scancode;
}

void platform_joy_virtual_set(int device, uint32_t keys)
{
    if (device >= 0 && device < VIRTUAL_DEVICES) virtual_keys[device] = keys;
}

bool platform_joy_virtual_down(int scancode)
{
    if (scancode == 0) return false;
    for (int action = 0; action < ACT_COUNT; action++) {
        if (virtual_scancode[action] != scancode) continue;
        for (int d = 0; d < VIRTUAL_DEVICES; d++)
            if ((virtual_keys[d] >> action & 1u) != 0) return true;
    }
    return false;
}

/* a binding's key held or let go while nothing reads the pad: the pad's keys are read again (platform_poll_events) */
void pad_keys_refresh(void)
{
    if (joy_refresh != NULL) joy_refresh();
}
