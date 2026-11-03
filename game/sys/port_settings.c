/* port: the port's own settings file (port.cfg). */
#include "game/sys/port_settings.h"
#include "game/platform/platform.h"
#include "game/race/weather.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_profile.h"
#include "game/viewer/screen_layout.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT_CFG_NAME "port.cfg"
#define FOV_MIN (-10)
#define FOV_MAX 20

static const char *const FORMAT_NAMES[] = { "4:3", "16:9", "21:9" };
static const char *const PSX_TIME_NAMES[] = { "day", "dawn", "dusk", "night" };   /* by PSX_TIME_* */

/* the PSX_TIME_* a name stands for ("dawn" or "a", "day" or "b", "dusk" or "c", "night", any case), or -1 */
static int psx_time_named(const char *name)
{
    char lower[8];
    size_t i = 0;
    for (; name[i] != '\0' && i < sizeof lower - 1; i++) lower[i] = (char)(name[i] >= 'A' && name[i] <= 'Z' ? name[i] + 32 : name[i]);
    lower[i] = '\0';
    if (name[i] != '\0') return -1;
    if (strcmp(lower, "day") == 0 || strcmp(lower, "b") == 0) return PSX_TIME_DAY;
    if (strcmp(lower, "dawn") == 0 || strcmp(lower, "a") == 0) return PSX_TIME_DAWN;
    if (strcmp(lower, "dusk") == 0 || strcmp(lower, "c") == 0) return PSX_TIME_DUSK;
    if (strcmp(lower, "night") == 0) return PSX_TIME_NIGHT;
    return -1;
}

PortSettings port_settings_defaults(void)
{
    /* the render threads are Auto by default: as many as the machine's logical cores, at most eight. The picture is the
     * same with any number of threads, and the wall is 1.7-3.5x quicker with them (re/notes/render-timing.md). */
    const PortSettings d = { .format = SCREEN_4_3, .refresh_hz = 60, .f3_ultra = 1, .render_scale = 1, .psx_time = PSX_TIME_DAY,
                             .weather = WEATHER_CLEAR, .render_bands = -1, .profile_ui = 1 };
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
    } else if (strcmp(key, "f3_ultra") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->f3_ultra = (int)n;
    } else if (strcmp(key, "render_scale") == 0) {
        if (parse_int(value, &n) && n >= 1 && n <= 3)
            s->render_scale = (int)n;
    } else if (strcmp(key, "laps") == 0) {
        if (parse_int(value, &n) && n >= 0 && n <= 99)
            s->laps = (int)n;
    } else if (strcmp(key, "weather") == 0) {
        const int w = weather_named(value);
        if (w >= 0) s->weather = w;
    } else if (strcmp(key, "perspective") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->perspective = (int)n;
    } else if (strcmp(key, "texture_filter") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->texture_filter = (int)n;
    } else if (strcmp(key, "fog") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->fog = (int)n;
    } else if (strcmp(key, "psx_light") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->psx_light = (int)n;
    } else if (strcmp(key, "psx_headlights") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->psx_headlights = (int)n;
    } else if (strcmp(key, "psx_shadow") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->psx_shadow = (int)n;
    } else if (strcmp(key, "psx_time") == 0) {
        const int t = psx_time_named(value);
        if (t >= 0) s->psx_time = t;
    } else if (strcmp(key, "psx_dir") == 0) {
        snprintf(s->psx_dir, sizeof s->psx_dir, "%s", value);
    } else if (strcmp(key, "free_records") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->free_records = (int)n;
    } else if (strcmp(key, "fog_sample") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->fog_sample = (int)n;
    } else if (strcmp(key, "psx_flares") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->psx_flares = (int)n;
    } else if (strcmp(key, "port_profile_ui") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->profile_ui = (int)n;
    } else if (strcmp(key, "hall_light") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->hall_light = (int)n;
    } else if (strcmp(key, "sky_fill") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->sky_fill = (int)n;
    } else if (strcmp(key, "night_sky_hue") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->night_sky_hue = (int)n;
    } else if (strcmp(key, "forest_dark") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->forest_dark = (int)n;
    } else if (strcmp(key, "car_colours") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->car_colours = (int)n;
    } else if (strcmp(key, "render_bands") == 0) {
        /* the render threads: "auto" for as many as the logical cores, or a count. Anything else keeps the default
         * (auto), so a damaged file cannot turn the threads off by accident. */
        if (strcmp(value, "auto") == 0)
            s->render_bands = -1;
        else if (parse_int(value, &n) && n >= 0 && n <= 8)
            s->render_bands = (int)n;
    } else if (strcmp(key, "profile") == 0) {
        const int p = port_profile_named(value);
        if (p >= 0) s->profile = p;
    } else if (strcmp(key, "body_shine") == 0) {
        if (parse_int(value, &n) && n >= 0 && n <= 4)
            s->body_shine = (int)n;
    }
}

/* which of the three that moved in from EXTRAS port.cfg actually names; the rest fall back to extras.cfg. The
 * extras.cfg key is the one EXTRAS used to write, so a file from before the move still carries it. */
typedef struct Moved {
    const char *key;   /* the key in port.cfg */
    size_t offset;         /* the field in PortSettings */
    size_t extras_offset;  /* the same setting's field in PortExtras */
} Moved;

static const Moved MOVED[3] = { { "perspective", offsetof(PortSettings, perspective), offsetof(PortExtras, modern_3d) },
                                 { "texture_filter", offsetof(PortSettings, texture_filter),
                                   offsetof(PortExtras, texture_filter) },
                                 { "fog", offsetof(PortSettings, fog), offsetof(PortExtras, fog) } };

PortSettings port_settings_parse(const char *text, size_t size)
{
    PortSettings s = port_settings_defaults();
    size_t pos = 0;
    while (pos < size) {
        char line[PSX_DIR_MAX + 32];   /* port: wide enough for a psx_dir line, the other keys are short */
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
    /* the render threads: "auto" for the sentinel, the count for a number */
    char bands_text[8];
    const char *const bands = s->render_bands < 0 ? "auto" : bands_text;
    if (s->render_bands >= 0) snprintf(bands_text, sizeof bands_text, "%d", s->render_bands);
    const int n = snprintf(out, size,
                           "format=%s\nrefresh_hz=%d\nscale=%s\nfullscreen=%d\nfov_adjust=%d\nview_ultra=%d\n"
                           "perspective=%d\ntexture_filter=%d\nfog=%d\nf3_ultra=%d\nrender_scale=%d\nlaps=%d\npsx_dir=%s\npsx_light=%d\npsx_shadow=%d\npsx_time=%s\nweather=%s\n"
                           "render_bands=%s\nbody_shine=%d\nfree_records=%d\ncar_colours=%d\nfog_sample=%d\nsky_fill=%d\nnight_sky_hue=%d\npsx_headlights=%d\npsx_flares=%d\nhall_light=%d\nforest_dark=%d\nprofile=%s\nport_profile_ui=%d\n",
                           FORMAT_NAMES[s->format], s->refresh_hz, s->scale_integer ? "integer" : "fill",
                           s->fullscreen, s->fov_adjust, s->view_ultra, s->perspective, s->texture_filter, s->fog, s->f3_ultra,
                           s->render_scale, s->laps, s->psx_dir, s->psx_light, s->psx_shadow, PSX_TIME_NAMES[s->psx_time], weather_name(s->weather), bands, s->body_shine,
                           s->free_records, s->car_colours, s->fog_sample, s->sky_fill, s->night_sky_hue, s->psx_headlights, s->psx_flares, s->hall_light, s->forest_dark, port_profile_key((Profile)s->profile), s->profile_ui);
    return n > 0 && (size_t)n < size ? (size_t)n : 0;
}

/* whether `text` has a line starting with `key=`; port.cfg wins over extras.cfg, so a save from the Graphics screen
 * is never undone by an extras.cfg left from before the settings moved. */
static bool names(const char *text, size_t size, const char *key)
{
    const size_t len = strlen(key);
    for (size_t pos = 0; pos < size; pos++) {
        const size_t start = pos;
        while (pos < size && text[pos] != '\n')
            pos++;
        const size_t line = pos - start;
        if (line > len && strncmp(text + start, key, len) == 0 && text[start + len] == '=')
            return true;
    }
    return false;
}

/* The three that moved in from EXTRAS: the ones `text` (port.cfg) does not name come from extras.cfg, read-only. With no
 * port.cfg at all (`text` NULL) none of them is named, so all three come from there. */
static void take_from_extras(PortSettings *s, const char *text, size_t size)
{
    const PortExtras extras = port_extras_load();
    for (int i = 0; i < 3; i++)
        if (!text || !names(text, size, MOVED[i].key))
            *(int *)((char *)s + MOVED[i].offset) =
                *(const int *)((const char *)&extras + MOVED[i].extras_offset);
}

static bool (*display_probe)(int *width, int *height);

void port_settings_set_display_probe(bool (*probe)(int *width, int *height)) { display_probe = probe; }

ScreenFormat port_monitor_format(ScreenFormat fallback)
{
    int w = 0, h = 0;
    return display_probe != NULL && display_probe(&w, &h) && w > 0 && h > 0 ? port_profile_monitor_format(w, h) : fallback;
}

PortSettings port_settings_load(void)
{
    size_t size = 0;
    uint8_t *data = platform_read_user_file(PORT_CFG_NAME, &size);
    if (!data) {
        PortSettings s = port_settings_defaults();
        take_from_extras(&s, NULL, 0);
        return s;
    }
    PortSettings s = port_settings_parse((const char *)data, size);
    take_from_extras(&s, (const char *)data, size);
    platform_free_file(data);
    if (s.profile == PROFILE_CUSTOM) return s;
    return port_profile_settings((Profile)s.profile, s, port_monitor_format(s.format));   /* a profile's settings are its table's, whatever the file says */
}

int port_settings_profile(void)
{
    size_t size = 0;
    uint8_t *data = platform_read_user_file(PORT_CFG_NAME, &size);
    if (!data) return PROFILE_CUSTOM;
    const int p = port_settings_parse((const char *)data, size).profile;
    platform_free_file(data);
    return p;
}

bool port_settings_save(const PortSettings *s)
{
    PortSettings c = *s;
    if (c.profile != PROFILE_CUSTOM && !port_profile_settings_match((Profile)c.profile, &c, port_monitor_format(c.format)))
        c.profile = PROFILE_CUSTOM;   /* changed by hand: the values stay, the profile is no more */
    char text[1024];   /* port: a psx_dir line of up to PSX_DIR_MAX bytes, as well as the rest (about 270 more) */
    const size_t n = port_settings_format(&c, text, sizeof text);
    return n > 0 && platform_write_user_file(PORT_CFG_NAME, (const uint8_t *)text, n);
}

bool port_settings_profile_custom(void)
{
    PortSettings s = port_settings_load();
    if (s.profile == PROFILE_CUSTOM) return true;
    s.profile = PROFILE_CUSTOM;
    return port_settings_save(&s);
}

bool port_profile_select(int p)
{
    if (p < 0 || p >= PROFILE_COUNT) return false;
    port_profile_undo_note();   /* the Options screen's Cancel puts the files back */
    PortSettings s = port_settings_load();
    s.profile = p;
    s = port_profile_settings((Profile)p, s, port_monitor_format(s.format));
    if (!port_settings_save(&s)) return false;   /* port.cfg first: port_extras_save reads the profile from it */
    const PortExtras e = port_profile_extras((Profile)p, port_extras_load());
    return port_extras_save(&e);
}

PlatformConfig port_platform_config(const char *title)
{
    const PortSettings s = port_settings_load();
    return (PlatformConfig){ screen_width(s.format), SCREEN_HEIGHT, title, s.scale_integer, s.fullscreen, s.refresh_hz };
}

int port_weather(void)
{
    const char *v = getenv("TNFS_WEATHER");
    const int w = v != NULL ? weather_named(v) : -1;
    return w >= 0 ? w : port_settings_load().weather;
}

int port_render_scale(void)
{
    const char *v = getenv("TNFS_RENDER_SCALE");
    if (v != NULL && v[0] >= '1' && v[0] <= '3' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().render_scale;
}

int port_psx_time(void)
{
    const char *v = getenv("TNFS_PSX_TIME");
    const int t = v != NULL ? psx_time_named(v) : -1;
    return t >= 0 ? t : port_settings_load().psx_time;
}

int port_fog_sample(void)
{
    const char *v = getenv("TNFS_FOG_SAMPLE");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().fog_sample;
}

int port_fog_lift(void)
{
    const char *v = getenv("TNFS_FOG_LIFT");
    char *end;
    const long n = v != NULL ? strtol(v, &end, 10) : -1;
    return v != NULL && end != v && *end == '\0' && n >= 0 && n <= 100 ? (int)n : FOG_SAMPLE_LIFT_DEFAULT;
}

int port_two_sided_scenery(int automatic)
{
    const char *v = getenv("TNFS_TWO_SIDED");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return automatic != 0;
}

int port_close_boxes(int automatic)
{
    const char *v = getenv("TNFS_CLOSE_BOXES");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return automatic != 0;
}

int port_psx_flares(void)
{
    if (!port_psx_light()) return 0;   /* the halo is the PS1 look's */
    const char *v = getenv("TNFS_PSX_FLARES");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().psx_flares;
}

int port_profile_ui(void)
{
    const char *v = getenv("TNFS_PROFILE_UI");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().profile_ui;
}

int port_hall_light(void)
{
    if (!port_psx_light()) return 0;   /* the PS1 look's */
    const char *v = getenv("TNFS_HALL_LIGHT");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().hall_light;
}

int port_sky_fill(void)
{
    const char *v = getenv("TNFS_SKY_FILL");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().sky_fill;
}

int port_night_sky_hue(void)
{
    const char *v = getenv("TNFS_NIGHT_SKY_HUE");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().night_sky_hue;
}

int port_forest_dark(void)
{
    const char *v = getenv("TNFS_FOREST_DARK");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().forest_dark;
}

int port_sky_fill_mode(void)
{
    const char *v = getenv("TNFS_SKY_FILL_MODE");
    if (v != NULL && (strcmp(v, "a") == 0 || strcmp(v, "1") == 0)) return 1;
    if (v != NULL && (strcmp(v, "b") == 0 || strcmp(v, "2") == 0)) return 2;
    return SKY_FILL_MODE_DEFAULT;
}

int port_free_records(void)
{
    const char *v = getenv("TNFS_FREE_RECORDS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().free_records;
}

int port_car_colours(void)
{
    const char *v = getenv("TNFS_CAR_COLOURS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().car_colours;
}

int port_psx_headlights(void)
{
    const char *v = getenv("TNFS_PSX_HEADLIGHTS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().psx_headlights;
}

int port_psx_shadow(void)
{
    const char *v = getenv("TNFS_PSX_SHADOW");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().psx_shadow;
}

int port_emissive_lamps(void)
{
    const char *v = getenv("TNFS_EMISSIVE_LAMPS");   /* a dev override; the game follows the Graphics Light Effects switch */
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_psx_light();
}

int port_body_shine(void)
{
    const char *v = getenv("TNFS_BODY_SHINE");
    if (v != NULL && v[0] >= '0' && v[0] <= '4' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().body_shine;
}

int port_psx_light(void)
{
    const char *v = getenv("TNFS_PSX_LIGHT");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().psx_light;
}
