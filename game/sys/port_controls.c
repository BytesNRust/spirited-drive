/* port: the port's control bindings (controls.cfg). */
#include "game/sys/port_controls.h"
#include "game/platform/platform.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONTROLS_CFG_NAME "controls.cfg"

enum {
    MAX_BUTTON = 63, MAX_AXIS = 15, MAX_HAT = 7, MAX_HAT_MASK = 15,
    DEADZONE_MAX = 50, SENSITIVITY_MIN = 50, SENSITIVITY_MAX = 200, STRENGTH_MAX = 100,
    LINE_SIZE = 160, SOURCE_SIZE = 40,
    /* SDL_Gamepad's numbers (SDL_GamepadButton / SDL_GamepadAxis) */
    PAD_SOUTH = 0, PAD_EAST = 1, PAD_WEST = 2, PAD_NORTH = 3, PAD_DPAD_LEFT = 13, PAD_DPAD_RIGHT = 14,
    PAD_AXIS_LEFTX = 0, PAD_AXIS_LEFT_TRIGGER = 4, PAD_AXIS_RIGHT_TRIGGER = 5
};

static const char *const ACTION_KEY[ACT_COUNT] = {
    "steer", "steer_left", "steer_right", "gas", "brake", "gear_up", "gear_down", "camera", "handbrake", "horn",
    "pause", "view" };

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

static void apply_number(int *field, const char *value, long min, long max)
{
    long n;
    const char *at = value;
    if (parse_number(&at, min, max, &n) && *at == '\0') *field = (int)n;
}

static void apply(PortControls *c, const char *key, const char *value)
{
    for (int act = 0; act < ACT_COUNT; act++)
        if (strcmp(key, ACTION_KEY[act]) == 0) {
            parse_sources(value, c->bind[act]);
            return;
        }
    if (strcmp(key, "deadzone") == 0) apply_number(&c->deadzone, value, 0, DEADZONE_MAX);
    else if (strcmp(key, "sensitivity") == 0) apply_number(&c->sensitivity, value, SENSITIVITY_MIN, SENSITIVITY_MAX);
    else if (strcmp(key, "ffb_strength") == 0) apply_number(&c->ffb_strength, value, 0, STRENGTH_MAX);
}

PortControls port_controls_parse(const char *text, size_t size)
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
            apply(&c, line, eq + 1);
        }
    }
    return c;
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
    const int n = snprintf(out + used, size - used, "deadzone=%d\nsensitivity=%d\nffb_strength=%d\n", c->deadzone,
                           c->sensitivity, c->ffb_strength);
    if (n < 0 || (size_t)n >= size - used) return 0;
    return used + (size_t)n;
}

PortControls port_controls_load(void)
{
    size_t size = 0;
    uint8_t *data = platform_read_user_file(CONTROLS_CFG_NAME, &size);
    if (data == NULL) return port_controls_defaults();
    PortControls c = port_controls_parse((const char *)data, size);
    platform_free_file(data);
    c.custom = 1;
    return c;
}

bool port_controls_save(const PortControls *c)
{
    char text[1024];
    const size_t n = port_controls_format(c, text, sizeof text);
    return n > 0 && platform_write_user_file(CONTROLS_CFG_NAME, (const uint8_t *)text, n);
}
