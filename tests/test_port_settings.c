/* The port's own settings file (port.cfg): defaults, text round trip, and what a damaged file leaves alone. */
#include "game/sys/port_settings.h"
#include "test.h"

#include "game/platform/platform.h"

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
}

static void test_round_trip(void)
{
    const PortSettings s = { .format = SCREEN_21_9, .refresh_hz = 144, .scale_integer = 1, .fullscreen = 1,
                             .fov_adjust = -7, .view_ultra = 1, .perspective = 1, .texture_filter = 1, .fog = 1, .psx_light = 1 };
    char text[256];
    const size_t n = port_settings_format(&s, text, sizeof text);
    CHECK(n > 0 && n < sizeof text);
    check_same(port_settings_parse(text, n), s);
}

static void test_parse_keys(void)
{
    const char *text = "format=21:9\nrefresh_hz=144\n";
    const PortSettings s = port_settings_parse(text, strlen(text));
    CHECK_EQ_INT(s.format, SCREEN_21_9);
    CHECK_EQ_INT(s.refresh_hz, 144);
    CHECK_EQ_INT(s.fov_adjust, 0);   /* untouched keys keep the default */
    CHECK_EQ_INT(s.psx_light, 0);
    const char *lit = "psx_light=1\n";
    CHECK_EQ_INT(port_settings_parse(lit, strlen(lit)).psx_light, 1);
    const char *bad = "psx_light=2\n";
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).psx_light, 0);
}

static void test_bad_input_keeps_defaults(void)
{
    const char *text = "refresh_hz=75\nfov_adjust=99\nformat=5:4\nno equals sign\nunknown=3\n";
    check_same(port_settings_parse(text, strlen(text)), port_settings_defaults());
    check_same(port_settings_parse("", 0), port_settings_defaults());
}

static void test_load_save(void)
{
    check_same(port_settings_load(), port_settings_defaults());   /* no file yet */
    const PortSettings s = { .format = SCREEN_16_9, .refresh_hz = 120, .scale_integer = 1, .fullscreen = 1,
                             .fov_adjust = 15, .view_ultra = 1, .perspective = 1, .texture_filter = 1, .fog = 1 };
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
    test_load_save();
    test_moved_settings_fall_back_to_extras();
    return TEST_REPORT();
}
