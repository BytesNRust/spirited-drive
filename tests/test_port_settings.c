/* The port's own settings file (port.cfg): defaults, text round trip, and what a damaged file leaves alone. */
#include "game/sys/port_settings.h"
#include "test.h"

#include "game/platform/platform.h"
#include "game/race/weather.h"

#include <stdlib.h>
#include <string.h>

/* Two in-memory user files stand in for the platform's storage: port.cfg is the one port_settings writes, and
 * extras.cfg is only read, as the fallback for the three settings that moved in from there. */
typedef struct {
    char name[32];
    uint8_t bytes[256];
    size_t size;
    int present;
} Stored;

static Stored stored[2];

/* the slot holding `name`, or the first free one; a second file takes stored[1] rather than overwriting the first */
static Stored *slot_for(const char *name)
{
    for (int i = 0; i < 2; i++)
        if (stored[i].name[0] != '\0' && strcmp(stored[i].name, name) == 0) return &stored[i];
    for (int i = 0; i < 2; i++)
        if (stored[i].name[0] == '\0') {
            strcpy(stored[i].name, name);
            return &stored[i];
        }
    strcpy(stored[0].name, name);
    return &stored[0];
}

static void put_file(const char *name, const char *text)
{
    Stored *s = slot_for(name);
    snprintf((char *)s->bytes, sizeof s->bytes, "%s", text);
    s->size = strlen(text);
    s->present = 1;
}

static void clear_files(void)
{
    for (int i = 0; i < 2; i++) {
        stored[i].name[0] = '\0';
        stored[i].size = 0;
        stored[i].present = 0;
    }
}

uint8_t *platform_read_user_file(const char *name, size_t *size)
{
    for (int i = 0; i < 2; i++)
        if (stored[i].present && strcmp(name, stored[i].name) == 0) {
            uint8_t *copy = malloc(stored[i].size);
            memcpy(copy, stored[i].bytes, stored[i].size);
            *size = stored[i].size;
            return copy;
        }
    return NULL;
}

bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    Stored *s = slot_for(name);
    if (size > sizeof s->bytes)
        return false;
    memcpy(s->bytes, data, size);
    s->size = size;
    s->present = 1;
    return true;
}

void platform_free_file(uint8_t *data) { free(data); }

static void check_same(PortSettings a, PortSettings b)
{
    CHECK_EQ_INT(a.format, b.format);
    CHECK_EQ_INT(a.refresh_hz, b.refresh_hz);
    CHECK_EQ_INT(a.scale_integer, b.scale_integer);
    CHECK_EQ_INT(a.fullscreen, b.fullscreen);
    CHECK_EQ_INT(a.fov_adjust, b.fov_adjust);
    CHECK_EQ_INT(a.view_ultra, b.view_ultra);
    CHECK_EQ_INT(a.perspective, b.perspective);
    CHECK_EQ_INT(a.texture_filter, b.texture_filter);
    CHECK_EQ_INT(a.fog, b.fog);
    CHECK_EQ_INT(a.psx_light, b.psx_light);
    CHECK_EQ_INT(a.psx_shadow, b.psx_shadow);
    CHECK_EQ_INT(a.psx_time, b.psx_time);
    CHECK_EQ_INT(a.render_scale, b.render_scale);
    CHECK_EQ_INT(a.laps, b.laps);
    CHECK_EQ_INT(a.weather, b.weather);
    CHECK_EQ_INT(a.render_bands, b.render_bands);
}

static void test_defaults(void)
{
    const PortSettings d = port_settings_defaults();
    CHECK_EQ_INT(d.format, SCREEN_4_3);
    CHECK_EQ_INT(d.refresh_hz, 60);
    CHECK_EQ_INT(d.scale_integer, 0);
    CHECK_EQ_INT(d.fullscreen, 0);
    CHECK_EQ_INT(d.fov_adjust, 0);
    CHECK_EQ_INT(d.view_ultra, 0);
    CHECK_EQ_INT(d.perspective, 0);
    CHECK_EQ_INT(d.texture_filter, 0);
    CHECK_EQ_INT(d.fog, 0);
    CHECK_EQ_INT(d.render_bands, -1);   /* the render threads are Auto by default: as many as the logical cores */
}

static void test_round_trip(void)
{
    const PortSettings s = { .format = SCREEN_21_9, .refresh_hz = 144, .scale_integer = 1, .fullscreen = 1,
                             .fov_adjust = -7, .view_ultra = 1, .perspective = 1, .texture_filter = 1, .fog = 1, .render_scale = 3, .laps = 25, .psx_light = 1, .psx_shadow = 1, .psx_time = PSX_TIME_DUSK, .weather = WEATHER_RAIN, .render_bands = 4 };
    char text[256];
    const size_t n = port_settings_format(&s, text, sizeof text);
    CHECK(n > 0 && n < sizeof text);
    check_same(port_settings_parse(text, n), s);
}

/* the render threads: the number of threads the bands use, 0 for off (the plain path) and -1 for "auto" (as many as
 * the logical cores, at most eight). The file names them "render_bands=0|auto|2|4". */
static void test_render_bands_values(void)
{
    char text[256];
    const char *const text_in = "render_bands=auto\n";
    PortSettings s = port_settings_parse(text_in, strlen(text_in));
    CHECK_EQ_INT(s.render_bands, -1);   /* "auto" is the sentinel, not a count */

    const char *const two = "render_bands=2\n";
    s = port_settings_parse(two, strlen(two));
    CHECK_EQ_INT(s.render_bands, 2);

    const char *const four = "render_bands=4\n";
    s = port_settings_parse(four, strlen(four));
    CHECK_EQ_INT(s.render_bands, 4);

    const char *const off = "render_bands=0\n";
    s = port_settings_parse(off, strlen(off));
    CHECK_EQ_INT(s.render_bands, 0);

    /* the written form: "auto" for the sentinel, the count for a number */
    PortSettings a = port_settings_defaults();
    a.render_bands = -1;
    CHECK(port_settings_format(&a, text, sizeof text) > 0);
    CHECK(strstr(text, "render_bands=auto\n") != NULL);
    a.render_bands = 4;
    CHECK(port_settings_format(&a, text, sizeof text) > 0);
    CHECK(strstr(text, "render_bands=4\n") != NULL);
    a.render_bands = 0;
    CHECK(port_settings_format(&a, text, sizeof text) > 0);
    CHECK(strstr(text, "render_bands=0\n") != NULL);
}

/* a damaged value keeps the default, like every other key: the render threads stay on Auto */
static void test_bad_render_bands_keeps_the_default(void)
{
    static const char *const bad[] = { "render_bands=9\n", "render_bands=-2\n", "render_bands=two\n",
                                       "render_bands=Auto\n", "render_bands=\n" };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        const PortSettings s = port_settings_parse(bad[i], strlen(bad[i]));
        CHECK_EQ_INT(s.render_bands, -1);
    }
}

/* the render scale: the many times the world is drawn over the frame's size, 1, 2 or 3. The file names them
 * "render_scale=1|2|3". */
static void test_render_scale_values(void)
{
    static const char *const good[] = { "render_scale=1\n", "render_scale=2\n", "render_scale=3\n" };
    for (int i = 0; i < 3; i++) {
        const PortSettings s = port_settings_parse(good[i], strlen(good[i]));
        CHECK_EQ_INT(s.render_scale, i + 1);
    }
    /* the written form: the number on its own */
    char text[256];
    PortSettings a = port_settings_defaults();
    a.render_scale = 3;
    CHECK(port_settings_format(&a, text, sizeof text) > 0);
    CHECK(strstr(text, "render_scale=3\n") != NULL);
}

/* a damaged value keeps the default, like every other key: the render scale stays at 1x */
static void test_bad_render_scale_keeps_the_default(void)
{
    static const char *const bad[] = { "render_scale=4\n", "render_scale=0\n", "render_scale=-1\n",
                                       "render_scale=two\n", "render_scale=\n" };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        const PortSettings s = port_settings_parse(bad[i], strlen(bad[i]));
        CHECK_EQ_INT(s.render_scale, 1);
    }
}

/* the environment wins over the file: TNFS_RENDER_SCALE (1..3) when set, else port.cfg's render_scale. It is what
 * the checks run with, so a file that says 1 must not stop the environment from saying 3. */
static void test_the_environment_wins_over_the_setting(void)
{
    unsetenv("TNFS_RENDER_SCALE");
    CHECK_EQ_INT(port_render_scale(), 1);   /* the default, with no file and no environment */
    setenv("TNFS_RENDER_SCALE", "3", 1);
    CHECK_EQ_INT(port_render_scale(), 3);
    setenv("TNFS_RENDER_SCALE", "", 1);
    CHECK_EQ_INT(port_render_scale(), 1);   /* an empty value is not a setting */
    setenv("TNFS_RENDER_SCALE", "9", 1);
    CHECK_EQ_INT(port_render_scale(), 1);   /* nor is one outside the range */
    unsetenv("TNFS_RENDER_SCALE");
    CHECK_EQ_INT(port_render_scale(), 1);
}

static void test_parse_keys(void)
{
    const char *text = "format=21:9\nrefresh_hz=144\n";
    const PortSettings s = port_settings_parse(text, strlen(text));
    CHECK_EQ_INT(s.format, SCREEN_21_9);
    CHECK_EQ_INT(s.refresh_hz, 144);
    CHECK_EQ_INT(s.fov_adjust, 0);   /* untouched keys keep the default */
    CHECK_EQ_INT(s.render_scale, 1);
    CHECK_EQ_INT(s.laps, 0);
    const char *many = "laps=99\n", *too_many = "laps=100\n", *negative = "laps=-1\n";
    CHECK_EQ_INT(port_settings_parse(many, strlen(many)).laps, 99);
    CHECK_EQ_INT(port_settings_parse(too_many, strlen(too_many)).laps, 0);
    CHECK_EQ_INT(port_settings_parse(negative, strlen(negative)).laps, 0);
    CHECK_EQ_INT(s.weather, WEATHER_CLEAR);
    const char *foggy = "weather=Fog\n", *stormy = "weather=storm\n";
    CHECK_EQ_INT(port_settings_parse(foggy, strlen(foggy)).weather, WEATHER_FOG);
    CHECK_EQ_INT(port_settings_parse(stormy, strlen(stormy)).weather, WEATHER_CLEAR);
    const char *scaled = "render_scale=2\n";
    CHECK_EQ_INT(port_settings_parse(scaled, strlen(scaled)).render_scale, 2);
    CHECK_EQ_INT(s.psx_light, 0);
    CHECK_EQ_INT(s.psx_shadow, 0);
    const char *shadow = "psx_shadow=1\n";
    CHECK_EQ_INT(port_settings_parse(shadow, strlen(shadow)).psx_shadow, 1);
    const char *bad_shadow = "psx_shadow=7\n";
    CHECK_EQ_INT(port_settings_parse(bad_shadow, strlen(bad_shadow)).psx_shadow, 0);
    const char *lit = "psx_light=1\n";
    CHECK_EQ_INT(port_settings_parse(lit, strlen(lit)).psx_light, 1);
    const char *bad = "psx_light=2\n";
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).psx_light, 0);
    CHECK_EQ_INT(s.psx_time, PSX_TIME_DAY);
    const char *at = "psx_time=A\n", *ct = "psx_time=c\n", *bt = "psx_time=day\n", *no = "psx_time=noon\n";
    const char *dawn = "psx_time=Dawn\n", *dusk = "psx_time=DUSK\n", *b = "psx_time=B\n";
    CHECK_EQ_INT(port_settings_parse(at, strlen(at)).psx_time, PSX_TIME_DAWN);
    CHECK_EQ_INT(port_settings_parse(ct, strlen(ct)).psx_time, PSX_TIME_DUSK);
    CHECK_EQ_INT(port_settings_parse(bt, strlen(bt)).psx_time, PSX_TIME_DAY);
    CHECK_EQ_INT(port_settings_parse(b, strlen(b)).psx_time, PSX_TIME_DAY);
    CHECK_EQ_INT(port_settings_parse(dawn, strlen(dawn)).psx_time, PSX_TIME_DAWN);
    CHECK_EQ_INT(port_settings_parse(dusk, strlen(dusk)).psx_time, PSX_TIME_DUSK);
    /* written as the words */
    char out[512];
    PortSettings w = port_settings_defaults();
    w.psx_time = PSX_TIME_DAWN;
    CHECK(port_settings_format(&w, out, sizeof out) > 0 && strstr(out, "psx_time=dawn\n") != NULL);
    w.psx_time = PSX_TIME_DUSK;
    CHECK(port_settings_format(&w, out, sizeof out) > 0 && strstr(out, "psx_time=dusk\n") != NULL);
    w.psx_time = PSX_TIME_DAY;
    CHECK(port_settings_format(&w, out, sizeof out) > 0 && strstr(out, "psx_time=day\n") != NULL);
    CHECK_EQ_INT(port_settings_parse(no, strlen(no)).psx_time, PSX_TIME_DAY);
}

static void test_bad_input_keeps_defaults(void)
{
    const char *text = "refresh_hz=75\nrender_scale=4\nfov_adjust=99\nformat=5:4\nno equals sign\nunknown=3\n";
    check_same(port_settings_parse(text, strlen(text)), port_settings_defaults());
    check_same(port_settings_parse("", 0), port_settings_defaults());
}

static void test_load_save(void)
{
    check_same(port_settings_load(), port_settings_defaults());   /* no file yet */
    const PortSettings s = { .format = SCREEN_16_9, .refresh_hz = 120, .scale_integer = 1, .fullscreen = 1,
                             .fov_adjust = 15, .view_ultra = 1, .perspective = 1, .texture_filter = 1, .fog = 1,
                             .render_scale = 2, .render_bands = 2 };
    CHECK(port_settings_save(&s));
    check_same(port_settings_load(), s);
}

/* The three settings that moved in from EXTRAS (perspective correction, texture filter, fog) live in port.cfg now,
 * but an extras.cfg written before the move keeps its value: only when port.cfg has no key for one does its value
 * come from extras.cfg. A key in port.cfg always wins, so a save from Graphics is not undone by a stale extras.cfg. */
static void test_moved_settings_fall_back_to_extras(void)
{
    clear_files();

    /* neither file: the defaults */
    check_same(port_settings_load(), port_settings_defaults());

    /* extras.cfg alone: its three values are taken */
    put_file("extras.cfg", "modern_3d=1\ntexture_filter=1\nfog=0\n");
    PortSettings s = port_settings_load();
    CHECK_EQ_INT(s.perspective, 1);
    CHECK_EQ_INT(s.texture_filter, 1);
    CHECK_EQ_INT(s.fog, 0);

    /* port.cfg names them: it wins over extras.cfg */
    put_file("port.cfg", "perspective=0\ntexture_filter=0\nfog=1\n");
    s = port_settings_load();
    CHECK_EQ_INT(s.perspective, 0);
    CHECK_EQ_INT(s.texture_filter, 0);
    CHECK_EQ_INT(s.fog, 1);

    /* port.cfg names one and not the others: the named one wins, the rest still come from extras.cfg */
    put_file("port.cfg", "fog=1\n");
    s = port_settings_load();
    CHECK_EQ_INT(s.perspective, 1);   /* from extras.cfg */
    CHECK_EQ_INT(s.texture_filter, 1);   /* from extras.cfg */
    CHECK_EQ_INT(s.fog, 1);              /* from port.cfg */

    /* saving from Graphics writes all three to port.cfg, and they survive the fallback afterwards */
    s.perspective = 0;
    s.texture_filter = 0;
    s.fog = 0;
    CHECK(port_settings_save(&s));
    /* the saved text carries all three, so a later load never has to consult extras.cfg for them */
    char text[256] = "";
    CHECK(port_settings_format(&s, text, sizeof text) > 0);
    CHECK(strstr(text, "perspective=") != NULL);
    CHECK(strstr(text, "texture_filter=") != NULL);
    CHECK(strstr(text, "fog=") != NULL);
    s = port_settings_load();
    CHECK_EQ_INT(s.perspective, 0);
    CHECK_EQ_INT(s.texture_filter, 0);
    CHECK_EQ_INT(s.fog, 0);

    clear_files();
}

/* The CD's files are not read by this test (the tables start as the built-in defaults). */
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path; (void)size; return NULL; }

int main(void)
{
    clear_files();
    test_defaults();
    test_round_trip();
    test_parse_keys();
    test_bad_input_keeps_defaults();
    test_render_bands_values();
    test_bad_render_bands_keeps_the_default();
    test_render_scale_values();
    test_bad_render_scale_keeps_the_default();
    test_the_environment_wins_over_the_setting();
    test_load_save();
    test_moved_settings_fall_back_to_extras();
    return TEST_REPORT();
}
