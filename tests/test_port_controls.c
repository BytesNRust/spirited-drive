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
    check_source(d.bind[ACT_STEER][0], SRC_NONE, 0);                  /* no whole axis: the stick is Steer left's and Steer right's halves */
    check_source(d.bind[ACT_STEER_LEFT][0], SRC_AXIS_NEG, 0);         /* the left stick's x, its left half */
    check_source(d.bind[ACT_STEER_RIGHT][0], SRC_AXIS_POS, 0);
    check_source(d.bind[ACT_STEER_LEFT][1], SRC_BUTTON, 13);          /* the d-pad */
    check_source(d.bind[ACT_STEER_RIGHT][1], SRC_BUTTON, 14);
    check_source(d.bind[ACT_GAS][0], SRC_AXIS_POS, 5);                /* the right trigger */
    check_source(d.bind[ACT_BRAKE][0], SRC_AXIS_POS, 4);              /* the left trigger */
    check_source(d.bind[ACT_GEAR_UP][0], SRC_BUTTON, 0);              /* A */
    check_source(d.bind[ACT_GEAR_DOWN][0], SRC_BUTTON, 1);            /* B */
    check_source(d.bind[ACT_CAMERA][0], SRC_BUTTON, 2);               /* X */
    check_source(d.bind[ACT_HANDBRAKE][0], SRC_BUTTON, 3);            /* Y */
    check_source(d.bind[ACT_HORN][0], SRC_NONE, 0);
    check_source(d.bind[ACT_PAUSE][0], SRC_NONE, 0);
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
    c.bind[ACT_STEER_RIGHT][0] = (ControlSource){ SRC_AXIS_POS, 3, 0, 1 };            /* inverted */
    c.bind[ACT_STEER_LEFT][0] = (ControlSource){ SRC_HAT, 0, 8, 0 };
    c.bind[ACT_STEER_LEFT][1] = (ControlSource){ SRC_BUTTON, 13, 0, 0 };              /* two sources */
    c.bind[ACT_GAS][0] = (ControlSource){ SRC_AXIS_FULL, 5, -32768, 0 };              /* a wheel's pedal */
    c.bind[ACT_BRAKE][0] = (ControlSource){ SRC_AXIS_NEG, 2, 32767, 1 };
    c.bind[ACT_HORN][0] = (ControlSource){ SRC_BUTTON, 31, 0, 0 };
    c.bind[ACT_PAUSE][0] = (ControlSource){ SRC_BUTTON, 6, 0, 0 };
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
    check_source(c.bind[ACT_STEER_RIGHT][1], SRC_BUTTON, 14);       /* not in the text: the default d-pad */
}

/* a controls.cfg of before has the steering as one whole axis: it is now the halves of Steer left and Steer right */
static void test_a_whole_steering_axis_loads_as_two_halves(void)
{
    const char *text = "steer=~axis3@100\n";
    PortControls c = port_controls_parse(text, strlen(text));
    check_source(c.bind[ACT_STEER][0], SRC_NONE, 0);                  /* the row is gone: nothing is left on it */
    check_source(c.bind[ACT_STEER_LEFT][0], SRC_AXIS_NEG, 3);         /* the file's stick over the default's */
    check_source(c.bind[ACT_STEER_RIGHT][0], SRC_AXIS_POS, 3);
    CHECK_EQ_INT(c.bind[ACT_STEER_LEFT][0].invert, 1);                /* its rest and its inversion kept */
    CHECK_EQ_INT(c.bind[ACT_STEER_LEFT][0].rest, 100);
    CHECK_EQ_INT(c.bind[ACT_STEER_RIGHT][0].invert, 1);
    check_source(c.bind[ACT_STEER_LEFT][1], SRC_BUTTON, 13);          /* the d-pad stays */
    check_source(c.bind[ACT_STEER_RIGHT][1], SRC_BUTTON, 14);
    /* a side the file names keeps what it names, the stick takes its free slot */
    text = "steer=axis2\nsteer_left=hat0.8\n";
    c = port_controls_parse(text, strlen(text));
    check_source(c.bind[ACT_STEER_LEFT][0], SRC_HAT, 0);
    check_source(c.bind[ACT_STEER_LEFT][1], SRC_AXIS_NEG, 2);
    check_source(c.bind[ACT_STEER_RIGHT][0], SRC_AXIS_POS, 2);
    check_source(c.bind[ACT_STEER][0], SRC_NONE, 0);
    /* two sticks (the old row held two) */
    text = "steer=axis2,axis0\n";
    c = port_controls_parse(text, strlen(text));
    check_source(c.bind[ACT_STEER_LEFT][0], SRC_AXIS_NEG, 2);
    check_source(c.bind[ACT_STEER_LEFT][1], SRC_BUTTON, 13);          /* no room for the second on this side: it stays on the old row */
    check_source(c.bind[ACT_STEER][0], SRC_AXIS_FULL, 0);
    /* `steer=` alone: no stick at all, the d-pad stays */
    text = "steer=\n";
    c = port_controls_parse(text, strlen(text));
    check_source(c.bind[ACT_STEER_LEFT][0], SRC_BUTTON, 13);
    check_source(c.bind[ACT_STEER_RIGHT][0], SRC_BUTTON, 14);
    /* what is written now reads back as it is: the folded file again gives the same */
    text = "steer=axis3\n";
    c = port_controls_parse(text, strlen(text));
    char out[2048];
    const size_t n = port_controls_format(&c, out, sizeof out);
    const PortControls back = port_controls_parse(out, n);
    check_same(&back, &c);
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

/* The CD's files are not read by this test (the tables start as the built-in defaults). */
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path; (void)size; return NULL; }

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

/* A controls.cfg of before has `view=`: the pad has no View action now, so the line is ignored and every other binding
 * stays where it was (the file names the actions, so Nitro and Look back keep their buttons). */
static void test_an_old_view_line_is_ignored(void)
{
    const char *text = "custom=1\nhorn=button4\npause=button6\nview=button7,axis2+\nnitro=button9\nlook_back=button8\nsensitivity=120\n";
    const PortControls c = port_controls_parse(text, strlen(text));
    check_source(c.bind[ACT_HORN][0], SRC_BUTTON, 4);
    check_source(c.bind[ACT_PAUSE][0], SRC_BUTTON, 6);
    check_source(c.bind[ACT_NITRO][0], SRC_BUTTON, 9);
    check_source(c.bind[ACT_NITRO][1], SRC_NONE, 0);
    check_source(c.bind[ACT_LOOK_BACK][0], SRC_BUTTON, 8);
    CHECK_EQ_INT(c.sensitivity, 120);
    char out[2048];
    CHECK(port_controls_format(&c, out, sizeof out) > 0);
    CHECK(strstr(out, "view") == NULL);
    CHECK(strstr(out, "nitro=button9\n") != NULL && strstr(out, "look_back=button8\n") != NULL);
}

static void test_keyboard_keys(void)
{
    PortControls c = port_controls_defaults();
    CHECK_EQ_INT(c.keys_custom, 0);
    char text[2048];
    size_t n = port_controls_format(&c, text, sizeof text);
    CHECK(n > 0 && strstr(text, "key_") == NULL);   /* config.dat's keys: the file says nothing of them */
    /* config.dat's words 3..12 as scancodes: Left, Right, Up, Down, A, Z, Space, C, H */
    const int16_t config[13] = { 0, 0, 0, 0x4b00, 0x4d00, 0x4800, 0x5000, 'A', 'Z', ' ', 'C', 'H', 'V' };
    int defaults[PKEY_COUNT];
    port_controls_key_defaults(config, 0, defaults);
    CHECK_EQ_INT(defaults[PKEY_LEFT], 0x4b);
    CHECK_EQ_INT(defaults[PKEY_GAS], 0x48);
    CHECK_EQ_INT(defaults[PKEY_GEAR_UP], 0x1e);
    CHECK_EQ_INT(defaults[PKEY_HANDBRAKE], 0x39);
    CHECK_EQ_INT(defaults[PKEY_HORN], 0x23);
    CHECK_EQ_INT(defaults[PKEY_CAMERA], 0x2e);
    CHECK_EQ_INT(port_controls_key(&c, defaults, PKEY_HORN), 0x23);
    /* assigning: the first time takes config.dat's, another row's key is taken from it */
    CHECK_EQ_INT(port_controls_set_key(&c, defaults, PKEY_LEFT, 0x10), -1);
    CHECK_EQ_INT(c.keys_custom, 1);
    CHECK_EQ_INT(port_controls_key(&c, defaults, PKEY_RIGHT), 0x4d);
    CHECK_EQ_INT(port_controls_set_key(&c, defaults, PKEY_BRAKE, 0x4d), PKEY_RIGHT);
    CHECK_EQ_INT(c.key[PKEY_RIGHT], 0x50);   /* swapped: Right gets the key Brake had */
    CHECK_EQ_INT(c.key[PKEY_BRAKE], 0x4d);
    CHECK_EQ_INT(port_controls_set_key(&c, defaults, PKEY_RIGHT, 0), -1);   /* none, on purpose: no one else is touched */
    CHECK_EQ_INT(c.key[PKEY_RIGHT], 0);
    CHECK_EQ_INT(c.key[PKEY_BRAKE], 0x4d);
    /* the file: all nine once any is the player's, none as "" */
    n = port_controls_format(&c, text, sizeof text);
    CHECK(n > 0 && strstr(text, "key_left=16\n") && strstr(text, "key_right=\n") && strstr(text, "key_camera=46\n"));
    const PortControls back = port_controls_parse(text, n);
    CHECK_EQ_INT(back.keys_custom, 1);
    for (int k = 0; k < PKEY_COUNT; k++) CHECK_EQ_INT(back.key[k], c.key[k]);
    /* a bad value leaves the key as it was; hex is read */
    const char *const hand = "key_horn=0x23\nkey_left=banana\nkey_gas=9999\n";
    const PortControls hand_c = port_controls_parse(hand, strlen(hand));
    CHECK_EQ_INT(hand_c.key[PKEY_HORN], 0x23);
    CHECK_EQ_INT(hand_c.key[PKEY_LEFT], 0);
    CHECK_EQ_INT(hand_c.key[PKEY_GAS], 0);
    /* what the race gets */
    int32_t codes[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    port_controls_apply_keys(&back, codes);
    CHECK_EQ_INT(codes[PKEY_LEFT], 0x10);
    CHECK_EQ_INT(codes[PKEY_CAMERA], 0x2e);
    CHECK_EQ_INT(codes[9], 10);   /* the parts key is not offered */
    PortControls plain = port_controls_defaults();
    int32_t kept[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    port_controls_apply_keys(&plain, kept);
    CHECK_EQ_INT(kept[PKEY_LEFT], 1);   /* not the player's: config.dat's stay */
    port_controls_reset_keys(&c);
    CHECK_EQ_INT(c.keys_custom, 0);
    CHECK_EQ_INT(c.key[PKEY_LEFT], 0);
    CHECK_EQ_INT(c.key[PKEY_LOOK_BACK], 0x30);   /* B comes back with the rest */
}

/* the look back key: B until it is the player's (config.dat has no word for it), kept out of the race's key codes, in the file with the others,
 * and B again for a file from before it existed */
static void test_look_back_key(void)
{
    PortControls c = port_controls_defaults();
    const int16_t config[13] = { 0, 0, 0, 0x4b00, 0x4d00, 0x4800, 0x5000, 'A', 'Z', ' ', 'C', 'H', 'V' };
    int defaults[PKEY_COUNT];
    port_controls_key_defaults(config, 0, defaults);
    CHECK_EQ_INT(defaults[PKEY_LOOK_BACK], 0x30);
    CHECK_EQ_INT(port_controls_look_back_key(&c), 0x30);
    CHECK_EQ_INT(port_controls_key_of_word(-1), -1);   /* no config.dat word is it */
    CHECK(strcmp(port_controls_key_name(PKEY_LOOK_BACK), "Look back") == 0);
    CHECK_EQ_INT(port_controls_set_key(&c, defaults, PKEY_LOOK_BACK, 0x22), -1);
    CHECK_EQ_INT(port_controls_look_back_key(&c), 0x22);
    char text[2048];
    size_t n = port_controls_format(&c, text, sizeof text);
    CHECK(n > 0 && strstr(text, "key_look_back=34\n") && strstr(text, "look_back=button9\n"));
    CHECK_EQ_INT(port_controls_parse(text, n).key[PKEY_LOOK_BACK], 0x22);
    CHECK_EQ_INT(port_controls_set_key(&c, defaults, PKEY_HORN, 0x22), PKEY_LOOK_BACK);   /* a key a row has goes to the row that asks: swapped */
    CHECK_EQ_INT(port_controls_look_back_key(&c), 0x23);
    const char *const old = "key_left=16\nkey_right=17\n";   /* a file from before the key: it is not none */
    const PortControls from_old = port_controls_parse(old, strlen(old));
    CHECK_EQ_INT(from_old.keys_custom, 1);
    CHECK_EQ_INT(port_controls_look_back_key(&from_old), 0x30);
    CHECK_EQ_INT(port_controls_set_key(&c, defaults, PKEY_LOOK_BACK, 0), -1);
    CHECK_EQ_INT(port_controls_look_back_key(&c), 0);   /* none on purpose */
    CHECK(port_controls_key_reserved(0x5b));   /* the pad's virtual key is not offered to the keyboard */
}

static void test_nitro_key(void)
{
    PortControls c = port_controls_defaults();
    const int16_t config[13] = { 0, 0, 0, 0x4b00, 0x4d00, 0x4800, 0x5000, 'A', 'Z', ' ', 'C', 'H', 'V' };
    int defaults[PKEY_COUNT];
    port_controls_key_defaults(config, 0, defaults);
    CHECK_EQ_INT(defaults[PKEY_NITRO], 0x31);
    CHECK_EQ_INT(port_controls_nitro_key(&c), 0x31);
    CHECK(strcmp(port_controls_key_name(PKEY_NITRO), "Nitro") == 0);
    CHECK_EQ_INT(port_controls_set_key(&c, defaults, PKEY_NITRO, 0x22), -1);
    CHECK_EQ_INT(port_controls_nitro_key(&c), 0x22);
    char text[2048];
    const size_t n = port_controls_format(&c, text, sizeof text);
    CHECK(n > 0 && strstr(text, "key_nitro=34\n"));
    CHECK_EQ_INT(port_controls_parse(text, n).key[PKEY_NITRO], 0x22);
    const char *const old = "key_left=16\nkey_right=17\n";   /* a file from before the key: it is not none */
    const PortControls from_old = port_controls_parse(old, strlen(old));
    CHECK_EQ_INT(port_controls_nitro_key(&from_old), 0x31);
    port_controls_reset_keys(&c);
    CHECK_EQ_INT(port_controls_nitro_key(&c), 0x31);
}

int main(void)
{
    test_defaults_are_todays_pad();
    test_round_trip();
    test_parse_text();
    test_a_whole_steering_axis_loads_as_two_halves();
    test_bad_lines_keep_the_default();
    test_empty_value_clears();
    test_load_save();
    test_a_file_can_say_the_pad_is_not_custom();
    test_nitro_is_a_binding_in_the_file();
    test_an_old_view_line_is_ignored();
    test_keyboard_keys();
    test_look_back_key();
    test_nitro_key();
    return TEST_REPORT();
}
