/* control_map: the virtual joystick from the bindings. With the defaults a gamepad gives what platform_joy_sdl3.c's
 * pad_read gave; a wheel's pedal that rests at an end is released at rest; a source the device lacks is ignored. */
#include "game/sys/control_map.h"
#include "test.h"

#include "game/platform/platform.h"

#include <stdlib.h>
#include <string.h>

/* port_controls.c, which the defaults come from, reaches the platform's files; the test has none */
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}
void platform_free_file(uint8_t *data) { free(data); }

/* SDL_Gamepad's numbers */
enum { SOUTH = 0, EAST = 1, WEST = 2, NORTH = 3, BACK = 4, START = 6, LEFT_STICK = 7, RIGHT_STICK = 8, LEFT_SHOULDER = 9,
       RIGHT_SHOULDER = 10, DPAD_UP = 11, DPAD_DOWN = 12, DPAD_LEFT = 13, DPAD_RIGHT = 14, PAD_BUTTON_COUNT = 26 };
enum { LEFTX = 0, LEFTY = 1, RIGHTX = 2, RIGHTY = 3, LEFT_TRIGGER = 4, RIGHT_TRIGGER = 5, PAD_AXIS_COUNT = 6 };
enum { TOP = 0xffff, MIDDLE = 0x8000 };

static RawPad pad(void)
{
    RawPad r;
    memset(&r, 0, sizeof r);
    r.gamepad = 1, r.buttons = PAD_BUTTON_COUNT, r.axes = PAD_AXIS_COUNT;
    return r;
}

static RawPad wheel(int buttons, int axes)
{
    RawPad r;
    memset(&r, 0, sizeof r);
    r.buttons = buttons, r.axes = axes;
    return r;
}

static void test_defaults_are_the_pad_as_it_was(void)
{
    const PortControls c = port_controls_defaults();
    RawPad r = pad();
    VirtualPad v = control_map_apply(&c, &r);
    CHECK_EQ_INT(v.buttons, 0);
    CHECK_EQ_INT(v.axis[0], MIDDLE);
    CHECK_EQ_INT(v.axis[1], MIDDLE);
    CHECK_EQ_INT(v.axis[2], TOP);            /* the pedals released */
    CHECK_EQ_INT(v.axis[3], TOP);
    CHECK_EQ_INT(v.keys, 0);

    r.axis[LEFTX] = -32768;                  /* the stick all the way left */
    v = control_map_apply(&c, &r);
    CHECK_EQ_INT(v.axis[0], 0);
    r.axis[LEFTX] = 32767;
    v = control_map_apply(&c, &r);
    CHECK_EQ_INT(v.axis[0], TOP);
    r.axis[LEFTX] = 1000;                    /* no dead zone: the raw value on the middle */
    v = control_map_apply(&c, &r);
    CHECK_EQ_INT(v.axis[0], MIDDLE + 1000);

    r = pad();
    r.button[DPAD_LEFT] = 1;                 /* the d-pad pushes the stick to its end, the low end first */
    v = control_map_apply(&c, &r);
    CHECK_EQ_INT(v.axis[0], 0);
    r.button[DPAD_RIGHT] = 1;
    v = control_map_apply(&c, &r);
    CHECK_EQ_INT(v.axis[0], 0);
    r.button[DPAD_LEFT] = 0;
    v = control_map_apply(&c, &r);
    CHECK_EQ_INT(v.axis[0], TOP);

    r = pad();
    r.axis[LEFTY] = -1234;
    v = control_map_apply(&c, &r);
    CHECK_EQ_INT(v.axis[1], MIDDLE - 1234);  /* the menus' vertical: the stick's y */
    r.button[DPAD_UP] = 1;
    v = control_map_apply(&c, &r);
    CHECK_EQ_INT(v.axis[1], 0);
    r.button[DPAD_UP] = 0, r.button[DPAD_DOWN] = 1;
    v = control_map_apply(&c, &r);
    CHECK_EQ_INT(v.axis[1], TOP);

    r = pad();
    r.axis[RIGHT_TRIGGER] = 0x4000;          /* the triggers half way: TOP - 2 x the trigger */
    r.axis[LEFT_TRIGGER] = 0x7fff;
    v = control_map_apply(&c, &r);
    CHECK_EQ_INT(v.axis[2], TOP - 2 * 0x4000);
    CHECK_EQ_INT(v.axis[3], TOP - 2 * 0x7fff);
}

static void test_defaults_buttons(void)
{
    const PortControls c = port_controls_defaults();
    RawPad r = pad();
    r.button[SOUTH] = 1;
    CHECK_EQ_INT(control_map_apply(&c, &r).buttons, 1u << 0);
    r = pad(), r.button[EAST] = 1;
    CHECK_EQ_INT(control_map_apply(&c, &r).buttons, 1u << 1);
    r = pad(), r.button[WEST] = 1;
    CHECK_EQ_INT(control_map_apply(&c, &r).buttons, 1u << 2);
    r = pad(), r.button[NORTH] = 1;
    CHECK_EQ_INT(control_map_apply(&c, &r).buttons, 1u << 3);
    /* the others in XInput's WinMM order after the four: shoulders, back, start, the sticks' buttons */
    r = pad(), r.button[LEFT_SHOULDER] = 1;
    CHECK_EQ_INT(control_map_apply(&c, &r).buttons, 1u << 4);
    r = pad(), r.button[RIGHT_SHOULDER] = 1;
    CHECK_EQ_INT(control_map_apply(&c, &r).buttons, 1u << 5);
    r = pad(), r.button[BACK] = 1;
    CHECK_EQ_INT(control_map_apply(&c, &r).buttons, 1u << 6);
    r = pad(), r.button[START] = 1;
    CHECK_EQ_INT(control_map_apply(&c, &r).buttons, 1u << 7);
    r = pad(), r.button[LEFT_STICK] = 1;
    CHECK_EQ_INT(control_map_apply(&c, &r).buttons, 1u << 8);
    r = pad(), r.button[RIGHT_STICK] = 1;
    CHECK_EQ_INT(control_map_apply(&c, &r).buttons, 1u << 9);
}

static void test_a_wheel_pedal_resting_at_an_end(void)
{
    PortControls c = port_controls_defaults();
    c.custom = 1;
    c.bind[ACT_GAS][0] = (ControlSource){ SRC_AXIS_FULL, 1, -32768, 0 };
    c.bind[ACT_BRAKE][0] = (ControlSource){ SRC_AXIS_FULL, 2, 32767, 0 };
    RawPad r = wheel(12, 4);
    r.axis[1] = -32768, r.axis[2] = 32767;
    VirtualPad v = control_map_apply(&c, &r);
    CHECK_EQ_INT(v.axis[2], TOP);            /* released at rest, whichever end that is */
    CHECK_EQ_INT(v.axis[3], TOP);
    r.axis[1] = 32767, r.axis[2] = -32768;   /* both to the floor */
    v = control_map_apply(&c, &r);
    CHECK(v.axis[2] <= 2);
    CHECK(v.axis[3] <= 2);
    r.axis[1] = 0;                           /* half way */
    v = control_map_apply(&c, &r);
    CHECK(v.axis[2] > 0x7000 && v.axis[2] < 0x9000);
}

/* Steer left and Steer right on the two halves of one axis steer in proportion, like the pedals: no dead spot or jump at the centre, the ends reached */
static void test_half_axes_steer_in_proportion(void)
{
    PortControls c = port_controls_defaults();
    c.custom = 1;
    c.bind[ACT_STEER][0] = c.bind[ACT_STEER][1] = (ControlSource){ SRC_NONE, 0, 0, 0 };
    c.bind[ACT_STEER_LEFT][0] = (ControlSource){ SRC_AXIS_NEG, RIGHTX, 0, 0 };
    c.bind[ACT_STEER_RIGHT][0] = (ControlSource){ SRC_AXIS_POS, RIGHTX, 0, 0 };
    c.bind[ACT_STEER_LEFT][1] = c.bind[ACT_STEER_RIGHT][1] = (ControlSource){ SRC_NONE, 0, 0, 0 };
    RawPad r = pad();
    const int at[] = { 0, 1, -1, 12000, -12000, 20000, -20000, 32767, -32768 };
    const int want[] = { MIDDLE, MIDDLE + 1, MIDDLE - 1, MIDDLE + 12000, MIDDLE - 12000, MIDDLE + 20000, MIDDLE - 20000, TOP, 0 };
    for (int i = 0; i < 9; i++) {
        r.axis[RIGHTX] = (int16_t)at[i];
        CHECK_EQ_INT(control_map_apply(&c, &r).axis[0], want[i]);
    }
    int last = -1;
    for (int x = -32768; x <= 32767; x += 257) {   /* no jump and never backwards across the whole travel */
        r.axis[RIGHTX] = (int16_t)x;
        const int v = control_map_apply(&c, &r).axis[0];
        CHECK(v >= last);
        CHECK(last < 0 || v - last <= 258);
        last = v;
    }
    c.deadzone = 10;   /* a dead zone about the centre, the ends still reached */
    r.axis[RIGHTX] = 1000;
    CHECK_EQ_INT(control_map_apply(&c, &r).axis[0], MIDDLE);
    r.axis[RIGHTX] = 32767;
    CHECK_EQ_INT(control_map_apply(&c, &r).axis[0], TOP);
    r.axis[RIGHTX] = -32768;
    CHECK_EQ_INT(control_map_apply(&c, &r).axis[0], 0);
    /* a button on one side is the full lock whatever the sensitivity, an axis half on the other still in proportion */
    c.deadzone = 0, c.sensitivity = 50;
    c.bind[ACT_STEER_LEFT][1] = (ControlSource){ SRC_BUTTON, DPAD_LEFT, 0, 0 };
    r = pad();
    r.button[DPAD_LEFT] = 1;
    CHECK_EQ_INT(control_map_apply(&c, &r).axis[0], 0);
    r = pad();
    r.axis[RIGHTX] = 20000;
    CHECK_EQ_INT(control_map_apply(&c, &r).axis[0], MIDDLE + 10000);   /* half the gain */
}

/* a pad with no controls.cfg steers with the stick's two halves and the d-pad, as it did with its one axis */
/* one push of a stick binds both ways: the other half of the same axis goes to the other side */
static void test_a_pushed_half_binds_the_other_side(void)
{
    PortControls c = port_controls_defaults();
    const ControlSource left_of_right_stick = { SRC_AXIS_NEG, RIGHTX, 0, 1 };   /* inverted: kept on the mirror */
    PortControls d = control_map_mirror_half(&c, ACT_STEER_LEFT, left_of_right_stick);
    CHECK_EQ_INT(d.bind[ACT_STEER_RIGHT][0].kind, SRC_AXIS_POS);       /* over the default stick's half */
    CHECK_EQ_INT(d.bind[ACT_STEER_RIGHT][0].index, RIGHTX);
    CHECK_EQ_INT(d.bind[ACT_STEER_RIGHT][0].invert, 1);
    CHECK_EQ_INT(d.bind[ACT_STEER_RIGHT][1].index, DPAD_RIGHT);        /* the d-pad stays */
    CHECK_EQ_INT(d.bind[ACT_STEER_LEFT][0].index, LEFTX);              /* the pushed side is the caller's to set */
    /* the other direction, and a side with a free slot but no half */
    PortControls e = c;
    e.bind[ACT_STEER_LEFT][0] = (ControlSource){ SRC_BUTTON, DPAD_LEFT, 0, 0 };
    e.bind[ACT_STEER_LEFT][1] = (ControlSource){ SRC_NONE, 0, 0, 0 };
    e = control_map_mirror_half(&e, ACT_STEER_RIGHT, (ControlSource){ SRC_AXIS_POS, RIGHTX, 0, 0 });
    CHECK_EQ_INT(e.bind[ACT_STEER_LEFT][0].kind, SRC_BUTTON);
    CHECK_EQ_INT(e.bind[ACT_STEER_LEFT][1].kind, SRC_AXIS_NEG);
    CHECK_EQ_INT(e.bind[ACT_STEER_LEFT][1].index, RIGHTX);
    /* nothing to do for a button, a whole axis, or another action; nothing to give a side with no room */
    d = control_map_mirror_half(&c, ACT_STEER_LEFT, (ControlSource){ SRC_BUTTON, 5, 0, 0 });
    CHECK_EQ_INT(d.bind[ACT_STEER_RIGHT][0].index, LEFTX);
    d = control_map_mirror_half(&c, ACT_STEER_LEFT, (ControlSource){ SRC_AXIS_FULL, RIGHTX, 0, 0 });
    CHECK_EQ_INT(d.bind[ACT_STEER_RIGHT][0].index, LEFTX);
    d = control_map_mirror_half(&c, ACT_GAS, (ControlSource){ SRC_AXIS_NEG, RIGHTX, 0, 0 });
    CHECK_EQ_INT(d.bind[ACT_STEER_RIGHT][0].index, LEFTX);
    PortControls full = c;
    full.bind[ACT_STEER_RIGHT][0] = (ControlSource){ SRC_BUTTON, 5, 0, 0 };
    full.bind[ACT_STEER_RIGHT][1] = (ControlSource){ SRC_BUTTON, 6, 0, 0 };
    d = control_map_mirror_half(&full, ACT_STEER_LEFT, left_of_right_stick);
    CHECK_EQ_INT(d.bind[ACT_STEER_RIGHT][0].index, 5);
    CHECK_EQ_INT(d.bind[ACT_STEER_RIGHT][1].index, 6);
}

static void test_the_default_stick_is_two_halves(void)
{
    const PortControls c = port_controls_defaults();
    CHECK_EQ_INT(c.bind[ACT_STEER][0].kind, SRC_NONE);
    CHECK_EQ_INT(c.bind[ACT_STEER_LEFT][0].kind, SRC_AXIS_NEG);
    CHECK_EQ_INT(c.bind[ACT_STEER_LEFT][0].index, LEFTX);
    CHECK_EQ_INT(c.bind[ACT_STEER_RIGHT][0].kind, SRC_AXIS_POS);
    CHECK_EQ_INT(c.bind[ACT_STEER_LEFT][1].index, DPAD_LEFT);
    CHECK_EQ_INT(c.bind[ACT_STEER_RIGHT][1].index, DPAD_RIGHT);
}

static void test_sources_the_device_lacks_are_ignored(void)
{
    PortControls c = port_controls_defaults();
    c.custom = 1;
    c.bind[ACT_GEAR_UP][0] = (ControlSource){ SRC_BUTTON, 14, 0, 0 };
    c.bind[ACT_GAS][0] = (ControlSource){ SRC_AXIS_POS, 7, 0, 0 };
    c.bind[ACT_STEER][0] = (ControlSource){ SRC_AXIS_FULL, 6, 0, 0 };
    c.bind[ACT_STEER_LEFT][0] = (ControlSource){ SRC_AXIS_NEG, 6, 0, 0 };
    c.bind[ACT_STEER_RIGHT][0] = (ControlSource){ SRC_AXIS_POS, 7, 0, 0 };
    c.bind[ACT_STEER_LEFT][1] = c.bind[ACT_STEER_RIGHT][1] = (ControlSource){ SRC_NONE, 0, 0, 0 };
    c.bind[ACT_PAUSE][0] = (ControlSource){ SRC_HAT, 1, 1, 0 };
    c.bind[ACT_NITRO][0] = (ControlSource){ SRC_BUTTON, 14, 0, 0 };
    c.bind[ACT_LOOK_BACK][0] = (ControlSource){ SRC_BUTTON, 20, 0, 0 };
    RawPad r = wheel(11, 4);                 /* a smaller device than the file names */
    memset(r.button, 1, sizeof r.button);    /* the arrays past its counts hold anything */
    memset(r.axis, 0x7f, sizeof r.axis);
    memset(r.hat, 0xff, sizeof r.hat);
    const VirtualPad v = control_map_apply(&c, &r);
    CHECK_EQ_INT(v.buttons & 1, 0);          /* button 14 does not exist */
    CHECK_EQ_INT(v.axis[0], MIDDLE);
    CHECK_EQ_INT(v.axis[2], TOP);
    CHECK_EQ_INT(v.keys, 0);
}

static void test_dead_zone_and_sensitivity_stay_in_range(void)
{
    PortControls c = port_controls_defaults();
    c.deadzone = 0, c.sensitivity = 200;
    RawPad r = pad();
    r.axis[LEFTX] = 32767;
    CHECK_EQ_INT(control_map_apply(&c, &r).axis[0], TOP);
    r.axis[LEFTX] = -32768;
    CHECK_EQ_INT(control_map_apply(&c, &r).axis[0], 0);
    r.axis[LEFTX] = 20000;                   /* twice the push, past the end: clipped */
    CHECK_EQ_INT(control_map_apply(&c, &r).axis[0], TOP);
    c.deadzone = 50, c.sensitivity = 50;
    r.axis[LEFTX] = 10000;                   /* inside the dead zone */
    CHECK_EQ_INT(control_map_apply(&c, &r).axis[0], MIDDLE);
    r.axis[LEFTX] = 32767;                   /* the end of the travel, half the gain */
    const int v = control_map_apply(&c, &r).axis[0];
    CHECK(v > MIDDLE && v <= TOP);
    c.deadzone = 20, c.sensitivity = 100;    /* the drift of a worn stick is not a steer */
    r.axis[LEFTX] = 3000;
    CHECK_EQ_INT(control_map_apply(&c, &r).axis[0], MIDDLE);
    r.axis[LEFTX] = 32767;                   /* and the full push is still the full steer */
    CHECK_EQ_INT(control_map_apply(&c, &r).axis[0], TOP);
}

static void test_virtual_keys(void)
{
    PortControls c = port_controls_defaults();
    c.bind[ACT_PAUSE][0] = (ControlSource){ SRC_BUTTON, 6, 0, 0 };
    c.bind[ACT_HORN][0] = (ControlSource){ SRC_HAT, 0, 1, 0 };
    c.bind[ACT_NITRO][0] = (ControlSource){ SRC_AXIS_POS, 3, 0, 0 };
    RawPad r = pad();
    CHECK_EQ_INT(control_map_apply(&c, &r).keys, 0);
    r.button[6] = 1;
    CHECK_EQ_INT(control_map_apply(&c, &r).keys, 1u << ACT_PAUSE);
    r.hat[0] = 1, r.hats = 1;
    CHECK_EQ_INT(control_map_apply(&c, &r).keys, 1u << ACT_PAUSE | 1u << ACT_HORN);
    r.axis[3] = 30000;                       /* an axis past half its travel is a press */
    CHECK_EQ_INT(control_map_apply(&c, &r).keys, 1u << ACT_PAUSE | 1u << ACT_HORN | 1u << ACT_NITRO);
}

/* Nitro is a virtual key of the pad's right shoulder by default: the four face buttons are taken. */
static void test_nitro_is_a_virtual_key(void)
{
    const PortControls c = port_controls_defaults();
    RawPad r = pad();
    CHECK_EQ_INT(control_map_apply(&c, &r).keys & (1u << ACT_NITRO), 0);
    r.button[RIGHT_SHOULDER] = 1;
    CHECK_EQ_INT(control_map_apply(&c, &r).keys, 1u << ACT_NITRO);
    CHECK(strcmp(control_map_action_name(ACT_NITRO), "Nitro") == 0);
}

/* Look back is a virtual key of the pad's left shoulder by default (the 360 camera's, race_follow.h), away from the others' */
static void test_look_back_is_a_virtual_key(void)
{
    const PortControls c = port_controls_defaults();
    RawPad r = pad();
    CHECK_EQ_INT(control_map_apply(&c, &r).keys & (1u << ACT_LOOK_BACK), 0);
    r.button[LEFT_SHOULDER] = 1;
    CHECK_EQ_INT(control_map_apply(&c, &r).keys, 1u << ACT_LOOK_BACK);
    CHECK(strcmp(control_map_action_name(ACT_LOOK_BACK), "Look back") == 0);
    const PortControls moved = control_map_assign(&c, ACT_LOOK_BACK, 0, (ControlSource){ SRC_BUTTON, NORTH, 0, 0 });
    CHECK_EQ_INT(moved.bind[ACT_HANDBRAKE][0].kind, SRC_NONE);   /* a button belongs to one action: the handbrake gave it up */
    CHECK_EQ_INT(moved.bind[ACT_LOOK_BACK][0].index, NORTH);
}

static void test_a_wheel_without_a_file_passes_through(void)
{
    const PortControls c = port_controls_defaults();   /* custom 0 */
    RawPad r = wheel(12, 4);
    r.button[0] = 1, r.button[5] = 1, r.button[11] = 1;
    r.axis[0] = -500, r.axis[1] = 7, r.axis[2] = 32767, r.axis[3] = -32768;
    const VirtualPad v = control_map_apply(&c, &r);
    CHECK_EQ_INT(v.buttons, 1u << 0 | 1u << 5 | 1u << 11);
    CHECK_EQ_INT(v.axis[0], MIDDLE - 500);
    CHECK_EQ_INT(v.axis[1], MIDDLE + 7);
    CHECK_EQ_INT(v.axis[2], TOP);
    CHECK_EQ_INT(v.axis[3], 0);
    r = wheel(3, 2);                         /* fewer axes than four: the middle */
    const VirtualPad w = control_map_apply(&c, &r);
    CHECK_EQ_INT(w.axis[2], MIDDLE);
    CHECK_EQ_INT(w.axis[3], MIDDLE);
}

static void test_a_wheel_with_a_file_keeps_its_other_buttons(void)
{
    PortControls c = port_controls_defaults();
    c.custom = 1;
    c.bind[ACT_GEAR_UP][0] = (ControlSource){ SRC_BUTTON, 8, 0, 0 };
    RawPad r = wheel(12, 4);
    r.button[8] = 1, r.button[9] = 1;
    const VirtualPad v = control_map_apply(&c, &r);
    CHECK_EQ_INT(v.buttons & 1, 1);          /* button 8 is gear up now */
    CHECK_EQ_INT(v.buttons & (1u << 9), 1u << 9);   /* the others as they were, for the controls screen */
}

static void test_invert_two_sources_and_thresholds(void)
{
    PortControls c = port_controls_defaults();
    c.custom = 1;
    c.bind[ACT_STEER][0] = (ControlSource){ SRC_AXIS_FULL, 0, 0, 1 };      /* inverted */
    c.bind[ACT_STEER][1] = (ControlSource){ SRC_AXIS_FULL, 2, 0, 0 };      /* a second stick */
    c.bind[ACT_GAS][0] = (ControlSource){ SRC_AXIS_POS, 5, 0, 0 };
    c.bind[ACT_GAS][1] = (ControlSource){ SRC_BUTTON, 0, 0, 0 };
    c.bind[ACT_PAUSE][0] = (ControlSource){ SRC_AXIS_POS, 3, 0, 0 };
    RawPad r = wheel(12, 6);
    r.axis[0] = 5000;
    CHECK_EQ_INT(control_map_apply(&c, &r).axis[0], MIDDLE - 5000);        /* the other way */
    r.axis[2] = -9000;                                                     /* the larger push wins */
    CHECK_EQ_INT(control_map_apply(&c, &r).axis[0], MIDDLE - 9000);
    r.axis[5] = 0x2000, r.button[0] = 1;                                   /* the button's all, over the trigger's */
    CHECK_EQ_INT(control_map_apply(&c, &r).axis[2], TOP - 2 * 0x7fff);
    r.axis[3] = 100;                                                       /* a hair of an axis is not a press */
    CHECK_EQ_INT(control_map_apply(&c, &r).keys, 0);
    c.bind[ACT_BRAKE][0] = (ControlSource){ SRC_AXIS_POS, 4, 0, 1 };       /* a pedal that reads backwards */
    r.axis[4] = -20000;
    CHECK_EQ_INT(control_map_apply(&c, &r).axis[3], TOP - 2 * 20000);
}

static void test_a_wheel_with_a_file_keeps_the_second_axis(void)
{
    PortControls c = port_controls_defaults();
    c.custom = 1;
    RawPad r = wheel(12, 4);
    r.axis[1] = 7;
    CHECK_EQ_INT(control_map_apply(&c, &r).axis[1], MIDDLE + 7);
}

static void test_detect_a_wheel_pedal_resting_at_an_end(void)
{
    RawPad rest = wheel(12, 4), now = wheel(12, 4);
    rest.axis[2] = -32768, now.axis[2] = 32767;                       /* pressed to the floor */
    ControlSource s = control_map_detect(&rest, &now, false);
    CHECK_EQ_INT(s.kind, SRC_AXIS_FULL);
    CHECK_EQ_INT(s.index, 2);
    CHECK_EQ_INT(s.rest, -32768);
    rest.axis[2] = 32767, now.axis[2] = -32768;                       /* one that rests at the top */
    s = control_map_detect(&rest, &now, false);
    CHECK_EQ_INT(s.kind, SRC_AXIS_FULL);
    CHECK_EQ_INT(s.rest, 32767);
    rest.axis[2] = -32768, now.axis[2] = -32768 + 20000;              /* not half way yet */
    CHECK_EQ_INT(control_map_detect(&rest, &now, false).kind, SRC_NONE);
}

static void test_detect_a_gamepad_trigger_and_steering(void)
{
    RawPad rest = pad(), now = pad();
    now.axis[RIGHT_TRIGGER] = 30000;
    ControlSource s = control_map_detect(&rest, &now, false);
    CHECK_EQ_INT(s.kind, SRC_AXIS_POS);
    CHECK_EQ_INT(s.index, RIGHT_TRIGGER);
    now = pad(), now.axis[LEFTX] = -30000;                            /* a stick that rests in the middle */
    s = control_map_detect(&rest, &now, false);
    CHECK_EQ_INT(s.kind, SRC_AXIS_NEG);
    CHECK_EQ_INT(s.index, LEFTX);
    s = control_map_detect(&rest, &now, true);                        /* asked for an axis to steer with */
    CHECK_EQ_INT(s.kind, SRC_AXIS_FULL);
    CHECK_EQ_INT(s.index, LEFTX);
}

static void test_detect_ignores_drift_and_needs_a_press(void)
{
    RawPad rest = pad(), now = pad();
    now.axis[LEFTX] = 3000, now.axis[RIGHTY] = -2500;                 /* a worn stick's drift */
    CHECK_EQ_INT(control_map_detect(&rest, &now, false).kind, SRC_NONE);
    now = pad(), now.button[NORTH] = 1;
    ControlSource s = control_map_detect(&rest, &now, false);
    CHECK_EQ_INT(s.kind, SRC_BUTTON);
    CHECK_EQ_INT(s.index, NORTH);
    CHECK_EQ_INT(control_map_detect(&rest, &now, true).kind, SRC_NONE);   /* a button cannot steer */
    rest.button[NORTH] = 1;                                           /* held when the capture began */
    CHECK_EQ_INT(control_map_detect(&rest, &now, false).kind, SRC_NONE);
}

static void test_detect_a_hat_and_the_most_moved_axis(void)
{
    RawPad rest = wheel(12, 4), now = wheel(12, 4);
    rest.hats = now.hats = 1;
    now.hat[0] = 8;
    ControlSource s = control_map_detect(&rest, &now, false);
    CHECK_EQ_INT(s.kind, SRC_HAT);
    CHECK_EQ_INT(s.index, 0);
    CHECK_EQ_INT(s.rest, 8);
    CHECK_EQ_INT(control_map_detect(&rest, &now, true).kind, SRC_NONE);
    rest.hat[0] = 8;                                                  /* held when the capture began */
    CHECK_EQ_INT(control_map_detect(&rest, &now, false).kind, SRC_NONE);
    now.hat[0] = 8 | 1;                                               /* up pressed too: that one */
    s = control_map_detect(&rest, &now, false);
    CHECK_EQ_INT(s.rest, 1);
    now = wheel(12, 4);
    now.axis[0] = 20000, now.axis[3] = 32000;                         /* two moved: the larger travel */
    s = control_map_detect(&rest, &now, false);
    CHECK_EQ_INT(s.index, 3);
}

static void test_swap_gives_the_taker_what_the_action_gave_up(void)
{
    PortControls c = port_controls_defaults();
    c.bind[ACT_STEER][0] = (ControlSource){ SRC_AXIS_FULL, LEFTX, 0, 0 };   /* a controls.cfg of before: its whole axis */
    const ControlSource south = c.bind[ACT_GEAR_UP][0], east = c.bind[ACT_GEAR_DOWN][0];
    /* gear down takes A (gear up's) and gives up B: gear up gets B, so neither is left bare */
    PortControls base = c;
    base.bind[ACT_GEAR_DOWN][0] = (ControlSource){ SRC_NONE, 0, 0, 0 };
    PortControls d = control_map_swap(&base, ACT_GEAR_DOWN, 0, south, east);
    CHECK_EQ_INT(d.bind[ACT_GEAR_DOWN][0].index, SOUTH);
    CHECK_EQ_INT(d.bind[ACT_GEAR_UP][0].kind, SRC_BUTTON);
    CHECK_EQ_INT(d.bind[ACT_GEAR_UP][0].index, EAST);
    /* nothing given up: the one that had it loses it, as assign does */
    d = control_map_swap(&base, ACT_GEAR_DOWN, 0, south, (ControlSource){ SRC_NONE, 0, 0, 0 });
    CHECK_EQ_INT(d.bind[ACT_GEAR_UP][0].kind, SRC_NONE);
    /* a button is no use to the steering axis: Steering takes gear up's button and gear up is left bare */
    PortControls s = c;
    s.bind[ACT_STEER][0] = (ControlSource){ SRC_NONE, 0, 0, 0 };
    d = control_map_swap(&s, ACT_STEER, 0, south, c.bind[ACT_STEER][0]);   /* gives up its axis: gear up may take it */
    CHECK_EQ_INT(d.bind[ACT_GEAR_UP][0].kind, SRC_AXIS_FULL);
    d = control_map_swap(&c, ACT_GEAR_DOWN, 0, c.bind[ACT_STEER][0], south);   /* the axis taken from Steering */
    CHECK_EQ_INT(d.bind[ACT_STEER][0].kind, SRC_NONE);                         /* a button does not steer */
}

static void test_assign_takes_the_source_from_the_other_action(void)
{
    PortControls c = port_controls_defaults();
    const ControlSource a = { SRC_BUTTON, SOUTH, 0, 0 };              /* gear up's today */
    PortControls d = control_map_assign(&c, ACT_HORN, 0, a);
    CHECK_EQ_INT(d.bind[ACT_HORN][0].kind, SRC_BUTTON);
    CHECK_EQ_INT(d.bind[ACT_HORN][0].index, SOUTH);
    CHECK_EQ_INT(d.bind[ACT_GEAR_UP][0].kind, SRC_NONE);              /* gone from where it was */
    CHECK_EQ_INT(d.bind[ACT_GEAR_DOWN][0].index, EAST);               /* the others untouched */
    CHECK_EQ_INT(c.bind[ACT_GEAR_UP][0].kind, SRC_BUTTON);            /* and the input is not changed */
}

static void test_assign_slots_compact_and_keep_two(void)
{
    PortControls c = port_controls_defaults();
    c.bind[ACT_STEER_LEFT][1] = (ControlSource){ SRC_HAT, 0, 8, 0 };  /* the d-pad's button and a hat */
    PortControls d = control_map_assign(&c, ACT_PAUSE, 0, c.bind[ACT_STEER_LEFT][0]);   /* the button leaves */
    CHECK_EQ_INT(d.bind[ACT_STEER_LEFT][0].kind, SRC_HAT);            /* the hat moved up to slot 0 */
    CHECK_EQ_INT(d.bind[ACT_STEER_LEFT][1].kind, SRC_NONE);
    d = control_map_assign(&d, ACT_STEER_LEFT, 1, (ControlSource){ SRC_BUTTON, 5, 0, 0 });
    CHECK_EQ_INT(d.bind[ACT_STEER_LEFT][0].kind, SRC_HAT);
    CHECK_EQ_INT(d.bind[ACT_STEER_LEFT][1].index, 5);
    d = control_map_assign(&d, ACT_STEER_LEFT, 0, (ControlSource){ SRC_BUTTON, 5, 0, 0 });   /* the same one again */
    CHECK_EQ_INT(d.bind[ACT_STEER_LEFT][0].index, 5);                 /* in slot 0; the copy in slot 1 is gone */
    CHECK_EQ_INT(d.bind[ACT_STEER_LEFT][1].kind, SRC_NONE);
    d = control_map_assign(&d, ACT_GAS, 0, (ControlSource){ SRC_AXIS_POS, 5, 0, 0 });   /* the one it already has */
    CHECK_EQ_INT(d.bind[ACT_GAS][0].index, 5);
    CHECK_EQ_INT(d.bind[ACT_GAS][1].kind, SRC_NONE);
    d = control_map_assign(&d, ACT_PAUSE, 0, (ControlSource){ SRC_NONE, 0, 0, 0 });   /* clearing */
    CHECK_EQ_INT(d.bind[ACT_PAUSE][0].kind, SRC_NONE);
}

static void test_assign_hats_differ_by_direction(void)
{
    PortControls c = port_controls_defaults();
    c.bind[ACT_STEER_LEFT][0] = (ControlSource){ SRC_HAT, 0, 8, 0 };
    c.bind[ACT_STEER_RIGHT][0] = (ControlSource){ SRC_HAT, 0, 2, 0 };
    const PortControls d = control_map_assign(&c, ACT_HORN, 0, (ControlSource){ SRC_HAT, 0, 1, 0 });   /* up */
    CHECK_EQ_INT(d.bind[ACT_STEER_LEFT][0].kind, SRC_HAT);            /* left and right stay */
    CHECK_EQ_INT(d.bind[ACT_STEER_RIGHT][0].kind, SRC_HAT);
}

static void test_source_names(void)
{
    char t[40];
    control_map_source_name(&(ControlSource){ SRC_BUTTON, 0, 0, 0 }, 1, t, sizeof t);
    CHECK(strcmp(t, "A") == 0);
    control_map_source_name(&(ControlSource){ SRC_BUTTON, 5, 0, 0 }, 0, t, sizeof t);
    CHECK(strcmp(t, "Button 5") == 0);                                  /* a wheel's own number */
    control_map_source_name(&(ControlSource){ SRC_AXIS_POS, 5, 0, 0 }, 1, t, sizeof t);
    CHECK(strcmp(t, "RT") == 0);
    control_map_source_name(&(ControlSource){ SRC_AXIS_FULL, 0, 0, 0 }, 1, t, sizeof t);
    CHECK(strcmp(t, "Left X") == 0);
    control_map_source_name(&(ControlSource){ SRC_AXIS_NEG, 2, 0, 0 }, 0, t, sizeof t);
    CHECK(strcmp(t, "Axis 2-") == 0);
    control_map_source_name(&(ControlSource){ SRC_AXIS_FULL, 1, 0, 1 }, 0, t, sizeof t);
    CHECK(strcmp(t, "Axis 1 inv") == 0);
    control_map_source_name(&(ControlSource){ SRC_HAT, 0, 8, 0 }, 0, t, sizeof t);
    CHECK(strcmp(t, "Hat 0 Left") == 0);
    control_map_source_name(&(ControlSource){ SRC_NONE, 0, 0, 0 }, 0, t, sizeof t);
    CHECK(strcmp(t, "-") == 0);
    control_map_source_name(&(ControlSource){ SRC_BUTTON, 40, 0, 0 }, 1, t, sizeof t);
    CHECK(strcmp(t, "Button 40") == 0);                                 /* past the gamepad's names */
    control_map_source_name(&(ControlSource){ SRC_BUTTON, 5, 0, 0 }, 0, t, 4);
    CHECK(strcmp(t, "But") == 0);                                       /* cut, not overrun */
}

static void test_binding_text_joins_the_sources(void)
{
    char t[64];
    PortControls c = port_controls_defaults();
    control_map_binding_text(&c, ACT_STEER_LEFT, 1, t, sizeof t);
    CHECK(strcmp(t, "Left X-, Left") == 0);   /* the stick's left half and the d-pad */
    c.bind[ACT_GAS][1] = (ControlSource){ SRC_BUTTON, 7, 0, 0 };
    control_map_binding_text(&c, ACT_GAS, 1, t, sizeof t);
    CHECK(strcmp(t, "RT, L3") == 0);
    control_map_binding_text(&c, ACT_HORN, 1, t, sizeof t);
    CHECK(strcmp(t, "-") == 0);                                         /* nothing bound */
    CHECK(strcmp(control_map_action_name(ACT_HANDBRAKE), "Handbrake") == 0);
    CHECK(strcmp(control_map_action_name(ACT_STEER), "Analog steering") == 0);
}

static void test_force_strength_scales_the_games_level(void)
{
    CHECK_EQ_INT(control_map_force(80, 100), 80);     /* 100 % leaves the level as it is */
    CHECK_EQ_INT(control_map_force(100, 100), 100);
    CHECK_EQ_INT(control_map_force(80, 0), 0);        /* 0 % is nothing */
    CHECK_EQ_INT(control_map_force(80, 50), 40);
    CHECK_EQ_INT(control_map_force(-80, 50), -40);    /* a force's direction stays */
    CHECK_EQ_INT(control_map_force(150, 100), 100);   /* never past 100 */
    CHECK_EQ_INT(control_map_force(-150, 100), -100);
    CHECK_EQ_INT(control_map_force(90, 300), 90);     /* a strength past 100 is 100 */
    CHECK_EQ_INT(control_map_force(90, -5), 0);       /* and one below 0 is 0 */
    CHECK_EQ_INT(control_map_force(1, 50), 0);        /* cut toward zero */
}

/* The CD's files are not read by this test (the tables start as the built-in defaults). */
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path; (void)size; return NULL; }

int main(void)
{
    test_force_strength_scales_the_games_level();
    test_source_names();
    test_binding_text_joins_the_sources();
    test_detect_a_wheel_pedal_resting_at_an_end();
    test_detect_a_gamepad_trigger_and_steering();
    test_detect_ignores_drift_and_needs_a_press();
    test_detect_a_hat_and_the_most_moved_axis();
    test_assign_takes_the_source_from_the_other_action();
    test_assign_slots_compact_and_keep_two();
    test_assign_hats_differ_by_direction();
    test_swap_gives_the_taker_what_the_action_gave_up();
    test_invert_two_sources_and_thresholds();
    test_a_wheel_with_a_file_keeps_the_second_axis();
    test_defaults_are_the_pad_as_it_was();
    test_defaults_buttons();
    test_a_wheel_pedal_resting_at_an_end();
    test_half_axes_steer_in_proportion();
    test_the_default_stick_is_two_halves();
    test_a_pushed_half_binds_the_other_side();
    test_sources_the_device_lacks_are_ignored();
    test_dead_zone_and_sensitivity_stay_in_range();
    test_virtual_keys();
    test_nitro_is_a_virtual_key();
    test_look_back_is_a_virtual_key();
    test_a_wheel_without_a_file_passes_through();
    test_a_wheel_with_a_file_keeps_its_other_buttons();
    return TEST_REPORT();
}
