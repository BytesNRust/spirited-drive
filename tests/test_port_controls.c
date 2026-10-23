/* The port's control bindings (controls.cfg): the defaults are today's pad mapping, the text round trip, and what a
 * damaged file leaves alone. */
#include "game/sys/port_controls.h"
#include "test.h"

#include "game/platform/platform.h"

#include <stdlib.h>
#include <string.h>

/* One in-memory user file stands in for the platform's storage. */
static char stored_name[32];
static uint8_t stored[2048];
static size_t stored_size;
static int stored_present;

uint8_t *platform_read_user_file(const char *name, size_t *size)
{
    if (!stored_present || strcmp(name, stored_name) != 0)
        return NULL;
    uint8_t *copy = malloc(stored_size);
    memcpy(copy, stored, stored_size);
    *size = stored_size;
    return copy;
}

bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    if (size > sizeof stored)
        return false;
    snprintf(stored_name, sizeof stored_name, "%s", name);
    memcpy(stored, data, size);
    stored_size = size;
    stored_present = 1;
    return true;
}

void platform_free_file(uint8_t *data) { free(data); }

static int same_source(ControlSource a, ControlSource b)
{
    return a.kind == b.kind && a.index == b.index && a.rest == b.rest && a.invert == b.invert;
}

static void check_same(const PortControls *a, const PortControls *b)
{
    for (int act = 0; act < ACT_COUNT; act++)
        for (int slot = 0; slot < 2; slot++) {
            test_checks++;
            if (!same_source(a->bind[act][slot], b->bind[act][slot])) {
                test_failures++;
                fprintf(stderr, "%s:%d: binding %d/%d differs\n", __FILE__, __LINE__, act, slot);
            }
        }
    CHECK_EQ_INT(a->deadzone, b->deadzone);
    CHECK_EQ_INT(a->sensitivity, b->sensitivity);
    CHECK_EQ_INT(a->ffb_strength, b->ffb_strength);
}

static void check_source(ControlSource s, SourceKind kind, int index)
{
    CHECK_EQ_INT(s.kind, kind);
    CHECK_EQ_INT(s.index, index);
}

static void test_defaults_are_todays_pad(void)
{
    const PortControls d = port_controls_defaults();
    check_source(d.bind[ACT_STEER][0], SRC_AXIS_FULL, 0);             /* the left stick's x */
    check_source(d.bind[ACT_STEER_LEFT][0], SRC_BUTTON, 13);          /* the d-pad */
    check_source(d.bind[ACT_STEER_RIGHT][0], SRC_BUTTON, 14);
    check_source(d.bind[ACT_GAS][0], SRC_AXIS_POS, 5);                /* the right trigger */
    check_source(d.bind[ACT_BRAKE][0], SRC_AXIS_POS, 4);              /* the left trigger */
    check_source(d.bind[ACT_GEAR_UP][0], SRC_BUTTON, 0);              /* A */
    check_source(d.bind[ACT_GEAR_DOWN][0], SRC_BUTTON, 1);            /* B */
    check_source(d.bind[ACT_CAMERA][0], SRC_BUTTON, 2);               /* X */
    check_source(d.bind[ACT_HANDBRAKE][0], SRC_BUTTON, 3);            /* Y */
    check_source(d.bind[ACT_HORN][0], SRC_NONE, 0);
    check_source(d.bind[ACT_PAUSE][0], SRC_NONE, 0);
    check_source(d.bind[ACT_VIEW][0], SRC_NONE, 0);
    check_source(d.bind[ACT_NITRO][0], SRC_BUTTON, 10);               /* RB: the face buttons are taken */
    check_source(d.bind[ACT_GAS][1], SRC_NONE, 0);
    CHECK_EQ_INT(d.deadzone, 0);
    CHECK_EQ_INT(d.sensitivity, 100);
    CHECK_EQ_INT(d.ffb_strength, 100);
    CHECK_EQ_INT(d.custom, 0);
}

static void test_round_trip(void)
{
    PortControls c = port_controls_defaults();
    c.bind[ACT_STEER][0] = (ControlSource){ SRC_AXIS_FULL, 3, 0, 1 };                 /* inverted */
    c.bind[ACT_STEER_LEFT][0] = (ControlSource){ SRC_HAT, 0, 8, 0 };
    c.bind[ACT_STEER_LEFT][1] = (ControlSource){ SRC_BUTTON, 13, 0, 0 };              /* two sources */
    c.bind[ACT_GAS][0] = (ControlSource){ SRC_AXIS_FULL, 5, -32768, 0 };              /* a wheel's pedal */
    c.bind[ACT_BRAKE][0] = (ControlSource){ SRC_AXIS_NEG, 2, 32767, 1 };
    c.bind[ACT_HORN][0] = (ControlSource){ SRC_BUTTON, 31, 0, 0 };
    c.bind[ACT_PAUSE][0] = (ControlSource){ SRC_BUTTON, 6, 0, 0 };
    c.bind[ACT_VIEW][0] = (ControlSource){ SRC_NONE, 0, 0, 0 };
    c.bind[ACT_GEAR_UP][0] = (ControlSource){ SRC_NONE, 0, 0, 0 };                    /* cleared */
    c.deadzone = 25, c.sensitivity = 150, c.ffb_strength = 40;
    char text[2048];
    const size_t n = port_controls_format(&c, text, sizeof text);
    CHECK(n > 0 && n < sizeof text);
    const PortControls back = port_controls_parse(text, n);
    check_same(&back, &c);
}

static void test_parse_text(void)
{
    const char *text = "steer=axis0\ngas=axis5+@-32768\ngear_up=button0\nsteer_left=hat0.8,button13\n"
                       "deadzone=12\nbrake=~axis2+\n";
    const PortControls c = port_controls_parse(text, strlen(text));
    check_source(c.bind[ACT_GAS][0], SRC_AXIS_POS, 5);
    CHECK_EQ_INT(c.bind[ACT_GAS][0].rest, -32768);
    check_source(c.bind[ACT_STEER_LEFT][0], SRC_HAT, 0);
    CHECK_EQ_INT(c.bind[ACT_STEER_LEFT][0].rest, 8);
    check_source(c.bind[ACT_STEER_LEFT][1], SRC_BUTTON, 13);
    check_source(c.bind[ACT_BRAKE][0], SRC_AXIS_POS, 2);
    CHECK_EQ_INT(c.bind[ACT_BRAKE][0].invert, 1);
    CHECK_EQ_INT(c.deadzone, 12);
    check_source(c.bind[ACT_STEER_RIGHT][0], SRC_BUTTON, 14);       /* not in the text: the default */
}

static void test_bad_lines_keep_the_default(void)
{
    const char *text = "bogus=button1\ngas=button-3\ndeadzone=90\nbrake=axis99\nsensitivity=10\nno equals sign\n"
                       "gear_up=hat0.3x\nffb_strength=101\nsteer=axis0@40000\nhorn=hat0.0\npause=hat0.16\ngear_down=button7,bogus\n";
    const PortControls bad = port_controls_parse(text, strlen(text)), want = port_controls_defaults();
    check_same(&bad, &want);
    const PortControls empty = port_controls_parse("", 0);
    check_same(&empty, &want);
}

static void test_empty_value_clears(void)
{
    const char *text = "steer_left=\nsteer_right=\n";
    const PortControls c = port_controls_parse(text, strlen(text));
    check_source(c.bind[ACT_STEER_LEFT][0], SRC_NONE, 0);
    check_source(c.bind[ACT_STEER_RIGHT][0], SRC_NONE, 0);
}

static void test_load_save(void)
{
    PortControls c = port_controls_load();
    const PortControls want = port_controls_defaults();
    check_same(&c, &want);   /* no file yet */
    CHECK_EQ_INT(c.custom, 0);
    c.bind[ACT_HORN][0] = (ControlSource){ SRC_BUTTON, 9, 0, 0 };
    c.deadzone = 7;
    c.custom = 1;   /* as the screen leaves an edited one */
    CHECK(port_controls_save(&c));
    CHECK(strcmp(stored_name, "controls.cfg") == 0);
    const PortControls back = port_controls_load();
    check_same(&back, &c);
    CHECK_EQ_INT(back.custom, 1);
}

static const char *stored_text(void)
{
    stored[stored_size < sizeof stored ? stored_size : sizeof stored - 1] = '\0';
    return (const char *)stored;
}

static void test_a_file_can_say_the_pad_is_not_custom(void)
{
    PortControls c = port_controls_defaults();   /* custom 0: Defaults on a wheel */
    c.deadzone = 0;
    CHECK(port_controls_save(&c));
    CHECK(strstr(stored_text(), "custom=0") != NULL);
    const PortControls back = port_controls_load();
    check_same(&back, &c);
    CHECK_EQ_INT(back.custom, 0);                  /* a file read, and still not custom */
    c.custom = 1;
    CHECK(port_controls_save(&c));
    CHECK(strstr(stored_text(), "custom") == NULL);
    CHECK_EQ_INT(port_controls_load().custom, 1);
}

static void test_nitro_is_a_binding_in_the_file(void)
{
    const char *text = "custom=1\nnitro=button9,axis5+\n";
    const PortControls c = port_controls_parse(text, strlen(text));
    check_source(c.bind[ACT_NITRO][0], SRC_BUTTON, 9);
    check_source(c.bind[ACT_NITRO][1], SRC_AXIS_POS, 5);
    char out[1024];
    CHECK(port_controls_format(&c, out, sizeof out) > 0);
    CHECK(strstr(out, "nitro=button9,axis5+\n") != NULL);
}

int main(void)
{
    test_defaults_are_todays_pad();
    test_round_trip();
    test_parse_text();
    test_bad_lines_keep_the_default();
    test_empty_value_clears();
    test_load_save();
    test_a_file_can_say_the_pad_is_not_custom();
    test_nitro_is_a_binding_in_the_file();
    return TEST_REPORT();
}
