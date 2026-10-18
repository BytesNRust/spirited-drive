/* The joystick as the menus and the replay's panel read it (0043d4c0 and
 * below): its buttons remapped, its axes moved to the words they stand for,
 * and the menu keys they make. The device itself is the platform's. */
#ifndef TNFS_JOYSTICK_H
#define TNFS_JOYSTICK_H

#include <stdint.h>

enum { JOY_AXES = 4, JOY_BUTTONS = 4 };

/* 004fce10..004fcee4, in this order (probes copy it as one block) */
typedef struct {
    uint8_t unknown_ce10[4];
    int32_t raw;                    /* 004fce14: set, the buttons are taken as read (no 0043da50) */
    int32_t upright;                /* 004fce18: set, no axis on is turned over (0043db50 not needed) */
    int16_t unknown_ce1c;
    int16_t button_on[JOY_BUTTONS]; /* 004fce1e: each button's bit kept (sign-extended into the mask) */
    int16_t axis_on[JOY_AXES];      /* 004fce26 */
    int16_t forces_on;              /* 004fce2e: the force-feedback stick wanted (0043cb50 tried) */
    uint8_t button_bit[JOY_BUTTONS]; /* 004fce30: the bit each button lands on (& 31) */
    uint8_t axis_slot[JOY_AXES];    /* 004fce34: 1, 2, 4 or 8, the axis word an axis goes to */
    int16_t invert[JOY_BUTTONS];    /* 004fce38: xor'ed into the buttons */
    int16_t axis_invert[JOY_AXES];  /* 004fce40: each axis turned over within its calibration (0043db50) */
    int32_t raw_axes;               /* 004fce48: set, the axes stay where they were read */
    int32_t dir_key;                /* 004fce4c: 0x4800 / 0x5000 / 0x4b00 / 0x4d00 (up, down, left, right) or 0 */
    int32_t button_key;             /* 004fce50: 0x20 / 0xd or 0 */
    int32_t pad_x, pad_y;           /* 004fce54, 004fce58: the pad read (0043d598), -0x20..0x1f and -0x1e..0x1e */
    uint8_t unknown_ce5c[0xc];
    uint32_t pad_buttons;           /* 004fce68: the buttons as read with it */
    int32_t axis[JOY_AXES];         /* 004fce6c: x, y, z, r */
    uint8_t unknown_ce7c[8];
    uint32_t buttons;               /* 004fce84 */
    /* 004fce88..004fcee4: joystick.cfg's 0x5c bytes, the calibration */
    int32_t x_min, x_mid, x_max;    /* 004fce88 */
    int32_t x_scale_low, x_scale_high;  /* 004fce94, 004fce98: 16.16 below and above the mid */
    int32_t x_low, x_high;          /* 004fce9c, 004fcea0 */
    int32_t y_min, y_mid, y_max;    /* 004fcea4 */
    int32_t y_scale_low, y_scale_high;  /* 004fceb0, 004fceb4 */
    int32_t y_low, y_high;          /* 004fceb8, 004fcebc */
    int32_t z_min, z_mid, z_max;    /* 004fcec0 */
    int32_t z_scale_low, z_scale_high;  /* 004fcecc, 004fced0 */
    int32_t z_pedal;                /* 004fced4: with an r axis, z a pedal of its own (16.16) */
    int32_t r_min, r_max;           /* 004fced8, 004fcedc (0043db50's) */
    int32_t r_pedal;                /* 004fcee0 */
} JoyState;

typedef struct {
    void *ctx;
    /* 0048d1d0 (cdecl device, &buttons, &x, &y, &z, &r, 0, 0, 0): 0 when
     * there is nothing to read */
    int32_t (*read)(void *ctx, int32_t device, uint32_t *buttons, int32_t axis[JOY_AXES]);
} JoyOps;

/* 0043da50 (eax the buttons): each button kept by its 004fce1e flag (the
 * four sign-extended shorts or'ed as a mask, << 0..3), xor'ed with its
 * 004fce38 short (the first short against the whole masked word), and bit
 * k of that put on bit 004fce30[k] & 31. */
void joy_remap(const JoyState *j, uint32_t *buttons);

/* 0043d8d0 (004bb908 the device): read (0048d1d0) into 004fce84 and
 * 004fce6c..78; nothing read gives 0. 004fce14 0: 0043da50 on the buttons.
 * 004fce48 0: each axis on (004fce26[k]) goes to the word its slot names
 * (004fce34[k]: 1 x, 2 y, 4 z, 8 r; any other nowhere), the later axis
 * winning; a word no axis goes to: the original stores the caller's ebx /
 * esi / ebp / edi there, the port 0. Then 1. */
int32_t joy_poll(JoyState *j, int32_t device, const JoyOps *ops);

/* 0043d4c0 (004fcdcc present): with a joystick polled, 004fce50 = 0x20 for
 * button bit 0, 0xd for bit 1, else 0; 004fce4c = 0x4800 with y below
 * 004fceb8, 0x5000 above 004fcebc, else 0x4b00 with x below 004fce9c,
 * 0x4d00 above 004fcea0, else 0; 1 when either is set. Else both 0, 0. */
int32_t joy_keys(JoyState *j, int32_t present, int32_t device, const JoyOps *ops);

#endif
