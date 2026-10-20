/* port: the port's own settings file (port.cfg). */
#include "game/sys/port_settings.h"
#include "game/platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT_CFG_NAME "port.cfg"
#define FOV_MIN (-10)
#define FOV_MAX 20

static const char *const FORMAT_NAMES[] = { "4:3", "16:9", "21:9" };

PortSettings port_settings_defaults(void)
{
    const PortSettings d = { SCREEN_4_3, 60, 0, 0, 0, 0 };
    return d;
}

static bool parse_int(const char *value, long *out)
{
    char *end;
    *out = strtol(value, &end, 10);
    return end != value && *end == '\0';
}

static void apply(PortSettings *s, const char *key, const char *value)
{
    long n;
    if (strcmp(key, "format") == 0) {
        for (int i = 0; i < 3; i++)
            if (strcmp(value, FORMAT_NAMES[i]) == 0)
                s->format = (ScreenFormat)i;
    } else if (strcmp(key, "refresh_hz") == 0) {
        if (parse_int(value, &n) && (n == 60 || n == 120 || n == 144))
            s->refresh_hz = (int)n;
    } else if (strcmp(key, "scale") == 0) {
        if (strcmp(value, "fill") == 0)
            s->scale_integer = 0;
        else if (strcmp(value, "integer") == 0)
            s->scale_integer = 1;
    } else if (strcmp(key, "fullscreen") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->fullscreen = (int)n;
    } else if (strcmp(key, "fov_adjust") == 0) {
        if (parse_int(value, &n) && n >= FOV_MIN && n <= FOV_MAX)
            s->fov_adjust = (int)n;
    } else if (strcmp(key, "view_ultra") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->view_ultra = (int)n;
    }
}

PortSettings port_settings_parse(const char *text, size_t size)
{
    PortSettings s = port_settings_defaults();
    size_t pos = 0;
    while (pos < size) {
        char line[64];
        size_t len = 0;
        while (pos < size && text[pos] != '\n') {
            if (len < sizeof line - 1)
                line[len++] = text[pos];
            pos++;
        }
        pos++;
        if (len > 0 && line[len - 1] == '\r')
            len--;
        line[len] = '\0';
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            apply(&s, line, eq + 1);
        }
    }
    return s;
}

size_t port_settings_format(const PortSettings *s, char *out, size_t size)
{
    const int n = snprintf(out, size, "format=%s\nrefresh_hz=%d\nscale=%s\nfullscreen=%d\nfov_adjust=%d\nview_ultra=%d\n",
                           FORMAT_NAMES[s->format], s->refresh_hz, s->scale_integer ? "integer" : "fill",
                           s->fullscreen, s->fov_adjust, s->view_ultra);
    return n > 0 && (size_t)n < size ? (size_t)n : 0;
}

PortSettings port_settings_load(void)
{
    size_t size = 0;
    uint8_t *data = platform_read_user_file(PORT_CFG_NAME, &size);
    if (!data)
        return port_settings_defaults();
    const PortSettings s = port_settings_parse((const char *)data, size);
    platform_free_file(data);
    return s;
}

bool port_settings_save(const PortSettings *s)
{
    char text[256];
    const size_t n = port_settings_format(s, text, sizeof text);
    return n > 0 && platform_write_user_file(PORT_CFG_NAME, (const uint8_t *)text, n);
}
