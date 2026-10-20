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

static void test_sources_the_device_lacks_are_ignored(void)
{
    PortControls c = port_controls_defaults();
    c.custom = 1;
    c.bind[ACT_GEAR_UP][0] = (ControlSource){ SRC_BUTTON, 14, 0, 0 };
    c.bind[ACT_GAS][0] = (ControlSource){ SRC_AXIS_POS, 7, 0, 0 };
    c.bind[ACT_STEER][0] = (ControlSource){ SRC_AXIS_FULL, 6, 0, 0 };
    c.bind[ACT_PAUSE][0] = (ControlSource){ SRC_HAT, 1, 1, 0 };
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
    c.bind[ACT_VIEW][0] = (ControlSource){ SRC_AXIS_POS, 3, 0, 0 };
    RawPad r = pad();
    CHECK_EQ_INT(control_map_apply(&c, &r).keys, 0);
    r.button[6] = 1;
    CHECK_EQ_INT(control_map_apply(&c, &r).keys, 1u << ACT_PAUSE);
    r.hat[0] = 1, r.hats = 1;
    CHECK_EQ_INT(control_map_apply(&c, &r).keys, 1u << ACT_PAUSE | 1u << ACT_HORN);
    r.axis[3] = 30000;                       /* an axis past half its travel is a press */
    CHECK_EQ_INT(control_map_apply(&c, &r).keys, 1u << ACT_PAUSE | 1u << ACT_HORN | 1u << ACT_VIEW);
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

int main(void)
{
    test_invert_two_sources_and_thresholds();
    test_a_wheel_with_a_file_keeps_the_second_axis();
    test_defaults_are_the_pad_as_it_was();
    test_defaults_buttons();
    test_a_wheel_pedal_resting_at_an_end();
    test_sources_the_device_lacks_are_ignored();
    test_dead_zone_and_sensitivity_stay_in_range();
    test_virtual_keys();
    test_a_wheel_without_a_file_passes_through();
    test_a_wheel_with_a_file_keeps_its_other_buttons();
    return TEST_REPORT();
}
