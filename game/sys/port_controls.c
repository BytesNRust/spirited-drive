/* port: the port's control bindings (controls.cfg). */
#include "game/sys/port_controls.h"
#include "game/platform/platform.h"
#include "game/sys/key_codes.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONTROLS_CFG_NAME "controls.cfg"

enum {
    MAX_BUTTON = 63, MAX_AXIS = 15, MAX_HAT = 7, MAX_HAT_MASK = 15,
    DEADZONE_MAX = 50, SENSITIVITY_MIN = 50, SENSITIVITY_MAX = 200, STRENGTH_MAX = 100,
    LINE_SIZE = 160, SOURCE_SIZE = 40, MAX_SCANCODE = 0x7f,
    /* SDL_Gamepad's numbers (SDL_GamepadButton / SDL_GamepadAxis) */
    PAD_SOUTH = 0, PAD_EAST = 1, PAD_WEST = 2, PAD_NORTH = 3, PAD_RIGHT_SHOULDER = 10, PAD_DPAD_LEFT = 13,
    PAD_DPAD_RIGHT = 14,
    PAD_AXIS_LEFTX = 0, PAD_AXIS_LEFT_TRIGGER = 4, PAD_AXIS_RIGHT_TRIGGER = 5
};

static const char *const ACTION_KEY[ACT_COUNT] = {
    "steer", "steer_left", "steer_right", "gas", "brake", "gear_up", "gear_down", "camera", "handbrake", "horn",
    "pause", "view", "nitro" };

static const char *const KEY_KEY[PKEY_COUNT] = {
    "key_left", "key_right", "key_gas", "key_brake", "key_gear_up", "key_gear_down", "key_handbrake", "key_horn",
    "key_camera" };
static const char *const KEY_WORD[PKEY_COUNT] = {
    "Steer left", "Steer right", "Accelerate", "Brake", "Gear up", "Gear down", "Handbrake", "Horn", "Camera" };
/* the config.dat short of each (input_setup.c: words 3..12 go to the codes in this order, the hold key's before the
 * handbrake's) */
static const int KEY_WORD_AT[PKEY_COUNT] = { 3, 4, 5, 6, 7, 8, 9, 11, 10 };

const char *port_controls_key_name(PortKey key) { return key >= 0 && key < PKEY_COUNT ? KEY_WORD[key] : ""; }

PortControls port_controls_defaults(void)
{
    PortControls c = { .deadzone = 0, .sensitivity = 100, .ffb_strength = 100 };
    c.bind[ACT_STEER][0] = (ControlSource){ SRC_AXIS_FULL, PAD_AXIS_LEFTX, 0, 0 };
    c.bind[ACT_STEER_LEFT][0] = (ControlSource){ SRC_BUTTON, PAD_DPAD_LEFT, 0, 0 };
    c.bind[ACT_STEER_RIGHT][0] = (ControlSource){ SRC_BUTTON, PAD_DPAD_RIGHT, 0, 0 };
    c.bind[ACT_GAS][0] = (ControlSource){ SRC_AXIS_POS, PAD_AXIS_RIGHT_TRIGGER, 0, 0 };
    c.bind[ACT_BRAKE][0] = (ControlSource){ SRC_AXIS_POS, PAD_AXIS_LEFT_TRIGGER, 0, 0 };
    c.bind[ACT_GEAR_UP][0] = (ControlSource){ SRC_BUTTON, PAD_SOUTH, 0, 0 };
    c.bind[ACT_GEAR_DOWN][0] = (ControlSource){ SRC_BUTTON, PAD_EAST, 0, 0 };
    c.bind[ACT_CAMERA][0] = (ControlSource){ SRC_BUTTON, PAD_WEST, 0, 0 };
    c.bind[ACT_HANDBRAKE][0] = (ControlSource){ SRC_BUTTON, PAD_NORTH, 0, 0 };
    c.bind[ACT_NITRO][0] = (ControlSource){ SRC_BUTTON, PAD_RIGHT_SHOULDER, 0, 0 };   /* the face buttons are taken */
    return c;
}

static int parse_number(const char **at, long min, long max, long *out)
{
    char *end;
    const long v = strtol(*at, &end, 10);
    if (end == *at || v < min || v > max) return 0;
    *at = end;
    *out = v;
    return 1;
}

/* one source, the whole of `text`; 0 when it is not one */
static int parse_source(const char *text, ControlSource *out)
{
    ControlSource s = { SRC_NONE, 0, 0, 0 };
    long n, rest;
    if (*text == '~') s.invert = 1, text++;
    if (strncmp(text, "button", 6) == 0) {
        text += 6;
        s.kind = SRC_BUTTON;
        if (!parse_number(&text, 0, MAX_BUTTON, &n)) return 0;
        s.index = (int)n;
    } else if (strncmp(text, "axis", 4) == 0) {
        text += 4;
        s.kind = SRC_AXIS_FULL;
        if (!parse_number(&text, 0, MAX_AXIS, &n)) return 0;
        s.index = (int)n;
        if (*text == '+') s.kind = SRC_AXIS_POS, text++;
        else if (*text == '-') s.kind = SRC_AXIS_NEG, text++;
        if (*text == '@') {
            text++;
            if (!parse_number(&text, -32768, 32767, &rest)) return 0;
            s.rest = (int)rest;
        }
    } else if (strncmp(text, "hat", 3) == 0) {
        text += 3;
        s.kind = SRC_HAT;
        if (!parse_number(&text, 0, MAX_HAT, &n)) return 0;
        s.index = (int)n;
        if (*text++ != '.' || !parse_number(&text, 1, MAX_HAT_MASK, &rest)) return 0;
        s.rest = (int)rest;
    } else {
        return 0;
    }
    if (*text != '\0') return 0;
    *out = s;
    return 1;
}

/* "a", "a,b" or "" into two sources; 0 when one is bad */
static int parse_sources(const char *value, ControlSource out[2])
{
    ControlSource got[2] = { { SRC_NONE, 0, 0, 0 }, { SRC_NONE, 0, 0, 0 } };
    if (*value != '\0') {
        char first[SOURCE_SIZE];
        const char *comma = strchr(value, ',');
        const size_t length = comma != NULL ? (size_t)(comma - value) : strlen(value);
        if (length >= sizeof first) return 0;
        memcpy(first, value, length);
        first[length] = '\0';
        if (!parse_source(first, &got[0])) return 0;
        if (comma != NULL && !parse_source(comma + 1, &got[1])) return 0;
    }
    out[0] = got[0];
    out[1] = got[1];
    return 1;
}

/* a scancode: decimal or 0x.. hex, 0 or empty for none; 0 when it is not one (the key stays as it was) */
static int parse_scancode(const char *value, int *out)
{
    if (*value == '\0') {
        *out = 0;
        return 1;
    }
    char *end;
    const long v = strtol(value, &end, 0);
    if (end == value || *end != '\0' || v < 0 || v > MAX_SCANCODE) return 0;
    *out = (int)v;
    return 1;
}

static void apply_number(int *field, const char *value, long min, long max)
{
    long n;
    const char *at = value;
    if (parse_number(&at, min, max, &n) && *at == '\0') *field = (int)n;
}

static void apply(PortControls *c, int *plain, const char *key, const char *value)
{
    if (strcmp(key, "custom") == 0 && strcmp(value, "0") == 0) *plain = 1;
    for (int act = 0; act < ACT_COUNT; act++)
        if (strcmp(key, ACTION_KEY[act]) == 0) {
            parse_sources(value, c->bind[act]);
            return;
        }
    for (int k = 0; k < PKEY_COUNT; k++)
        if (strcmp(key, KEY_KEY[k]) == 0) {
            int code;
            if (parse_scancode(value, &code)) c->key[k] = code, c->keys_custom = 1;
            return;
        }
    if (strcmp(key, "deadzone") == 0) apply_number(&c->deadzone, value, 0, DEADZONE_MAX);
    else if (strcmp(key, "sensitivity") == 0) apply_number(&c->sensitivity, value, SENSITIVITY_MIN, SENSITIVITY_MAX);
    else if (strcmp(key, "ffb_strength") == 0) apply_number(&c->ffb_strength, value, 0, STRENGTH_MAX);
}

/* `plain`: the text said "custom=0" */
static PortControls parse(const char *text, size_t size, int *plain)
{
    PortControls c = port_controls_defaults();
    size_t pos = 0;
    while (pos < size) {
        char line[LINE_SIZE];
        size_t len = 0;
        while (pos < size && text[pos] != '\n') {
            if (len < sizeof line - 1) line[len++] = text[pos];
            pos++;
        }
        pos++;
        if (len > 0 && line[len - 1] == '\r') len--;
        line[len] = '\0';
        char *eq = strchr(line, '=');
        if (eq != NULL) {
            *eq = '\0';
            apply(&c, plain, line, eq + 1);
        }
    }
    return c;
}

PortControls port_controls_parse(const char *text, size_t size)
{
    int plain = 0;
    return parse(text, size, &plain);
}

static int format_source(const ControlSource *s, char *out, size_t size)
{
    const char *invert = s->invert ? "~" : "";
    char rest[16] = "";
    if (s->kind == SRC_AXIS_POS || s->kind == SRC_AXIS_NEG || s->kind == SRC_AXIS_FULL)
        if (s->rest != 0) snprintf(rest, sizeof rest, "@%d", s->rest);
    switch (s->kind) {
    case SRC_BUTTON: return snprintf(out, size, "%sbutton%d", invert, s->index);
    case SRC_AXIS_POS: return snprintf(out, size, "%saxis%d+%s", invert, s->index, rest);
    case SRC_AXIS_NEG: return snprintf(out, size, "%saxis%d-%s", invert, s->index, rest);
    case SRC_AXIS_FULL: return snprintf(out, size, "%saxis%d%s", invert, s->index, rest);
    case SRC_HAT: return snprintf(out, size, "%shat%d.%d", invert, s->index, s->rest);
    default: return snprintf(out, size, "%s", "");
    }
}

size_t port_controls_format(const PortControls *c, char *out, size_t size)
{
    size_t used = 0;
    for (int act = 0; act < ACT_COUNT; act++) {
        char text[2 * SOURCE_SIZE] = "", one[SOURCE_SIZE];
        for (int slot = 0; slot < 2; slot++) {
            if (c->bind[act][slot].kind == SRC_NONE) continue;
            format_source(&c->bind[act][slot], one, sizeof one);
            if (text[0] != '\0') strcat(text, ",");
            strcat(text, one);
        }
        const int n = snprintf(out + used, size - used, "%s=%s\n", ACTION_KEY[act], text);
        if (n < 0 || (size_t)n >= size - used) return 0;
        used += (size_t)n;
    }
    for (int k = 0; c->keys_custom && k < PKEY_COUNT; k++) {
        char code[8] = "";
        if (c->key[k] != 0) snprintf(code, sizeof code, "%d", c->key[k]);
        const int m = snprintf(out + used, size - used, "%s=%s\n", KEY_KEY[k], code);
        if (m < 0 || (size_t)m >= size - used) return 0;
        used += (size_t)m;
    }
    const int n = snprintf(out + used, size - used, "%sdeadzone=%d\nsensitivity=%d\nffb_strength=%d\n",
                           c->custom ? "" : "custom=0\n", c->deadzone, c->sensitivity, c->ffb_strength);
    if (n < 0 || (size_t)n >= size - used) return 0;
    return used + (size_t)n;
}

void port_controls_key_defaults(const int16_t *config, int german, int out[PKEY_COUNT])
{
    for (int k = 0; k < PKEY_COUNT; k++)
        out[k] = key_code((uint16_t)key_layout(config[KEY_WORD_AT[k]], german));
}

int port_controls_key(const PortControls *c, const int defaults[PKEY_COUNT], PortKey key)
{
    if (key < 0 || key >= PKEY_COUNT) return 0;
    return c->keys_custom ? c->key[key] : defaults[key];
}

int port_controls_set_key(PortControls *c, const int defaults[PKEY_COUNT], PortKey key, int scancode)
{
    if (key < 0 || key >= PKEY_COUNT) return -1;
    if (!c->keys_custom) {
        memcpy(c->key, defaults, sizeof c->key);
        c->keys_custom = 1;
    }
    int taken = -1;
    for (int k = 0; scancode != 0 && k < PKEY_COUNT; k++)
        if (k != (int)key && c->key[k] == scancode) c->key[k] = 0, taken = k;
    c->key[key] = scancode;
    return taken;
}

void port_controls_reset_keys(PortControls *c)
{
    memset(c->key, 0, sizeof c->key);
    c->keys_custom = 0;
}

bool port_controls_key_reserved(int scancode)
{
    enum { ESCAPE = 0x01, F1 = 0x3b, F10 = 0x44, F11 = 0x57, F12 = 0x58, P = 0x19, VIRTUAL_FROM = 0x59 };
    return scancode <= 0 || scancode == ESCAPE || (scancode >= F1 && scancode <= F10) || scancode == F11 ||
           scancode == F12 || scancode == P || scancode >= VIRTUAL_FROM;
}

void port_controls_apply_keys(const PortControls *c, int32_t codes[PKEY_COUNT])
{
    if (!c->keys_custom) return;
    for (int k = 0; k < PKEY_COUNT; k++) codes[k] = c->key[k];
}

PortControls port_controls_load(void)
{
    size_t size = 0;
    uint8_t *data = platform_read_user_file(CONTROLS_CFG_NAME, &size);
    if (data == NULL) return port_controls_defaults();
    int plain = 0;
    PortControls c = parse((const char *)data, size, &plain);
    platform_free_file(data);
    c.custom = !plain;
    return c;
}

bool port_controls_save(const PortControls *c)
{
    char text[2048];
    const size_t n = port_controls_format(c, text, sizeof text);
    return n > 0 && platform_write_user_file(CONTROLS_CFG_NAME, (const uint8_t *)text, n);
}
