/* joy_pad_read with split pedals (z the gas, r the brake): the exe sums them (y = r less z), so a brake held with the gas
 * only slows the throttle and never reaches the brake; port (brake_over_gas) lets a brake past its dead zone be the pedal.
 *   test_joy_pad */
#include "game/sys/joy_calibrate.h"
#include "game/sys/joy_pad.h"

#include <stdio.h>
#include <string.h>

enum { MID = 0x4000, TOP = 0x0000, BOTTOM = 0x7fff, PEDAL = 0x1e };

static int failures;
#define CHECK_EQ(got, want)                                                                                       \
    do {                                                                                                          \
        const long g_ = (long)(got), w_ = (long)(want);                                                           \
        if (g_ != w_) {                                                                                           \
            fprintf(stderr, "%s:%d: %s = %ld, want %ld\n", __FILE__, __LINE__, #got, g_, w_);                     \
            failures++;                                                                                           \
        }                                                                                                         \
    } while (0)

static int32_t the_axis[JOY_AXES];

static int32_t read_pad(void *ctx, int32_t device, uint32_t *buttons, int32_t axis[JOY_AXES])
{
    (void)ctx, (void)device;
    *buttons = 0;
    memcpy(axis, the_axis, sizeof the_axis);
    return 1;
}

/* y with the gas z and the brake r as the platform gives them (control_map: AXIS_TOP less twice the pull; 0 is a pedal all the way down) */
static int32_t pad_y(int gas, int brake, int brake_wins)
{
    JoyState j;
    memset(&j, 0, sizeof j);
    j.x_min = j.y_min = j.z_min = j.r_min = 0;
    j.x_max = j.y_max = j.z_max = j.r_max = BOTTOM;
    j.x_mid = j.y_mid = j.z_mid = MID;
    j.upright = 1;
    for (int k = 0; k < JOY_AXES; k++) j.axis_on[k] = 1, j.axis_slot[k] = (uint8_t)(1 << k);
    joy_calibrate(&j, 1);
    JoyStart s;
    memset(&s, 0, sizeof s);
    s.present = 1;
    s.has_axis[2] = s.has_axis[3] = 1;
    s.brake_wins = brake_wins;
    the_axis[0] = the_axis[1] = MID;
    the_axis[2] = BOTTOM - gas * (BOTTOM / 100);   /* gas, brake: 0..100 % pulled */
    the_axis[3] = BOTTOM - brake * (BOTTOM / 100);
    const JoyOps ops = { NULL, read_pad };
    joy_pad_read(&j, &s, &ops);
    return j.pad_y;
}

static void test_exe_sums_the_pedals(void)
{
    CHECK_EQ(pad_y(100, 0, 0) < 0, 1);        /* the gas: the pedal negative */
    CHECK_EQ(pad_y(0, 100, 0) > 0, 1);        /* the brake: positive */
    CHECK_EQ(pad_y(100, 50, 0) < 0, 1);       /* the exe: the half brake only lifts the gas, the pedal is still gas */
}

static void test_brake_wins_over_the_gas(void)
{
    CHECK_EQ(pad_y(100, 50, 1) > 0, 1);       /* the brake is the pedal while the gas is down */
    CHECK_EQ(pad_y(100, 50, 1) * 0x889 / 256 > 10, 1);   /* car_control.c: brake = pedal * PEDAL_STEP / 256, and car_lights.c lights above 10 */
    CHECK_EQ(pad_y(100, 50, 0) * 0x889 / 256 > 10, 0);   /* the exe: no brake, no light */
    CHECK_EQ(pad_y(100, 100, 1), PEDAL);      /* all of it */
    CHECK_EQ(pad_y(100, 0, 1) < 0, 1);        /* no brake: the gas as it was */
    CHECK_EQ(pad_y(0, 0, 1), 0);
    CHECK_EQ(pad_y(0, 100, 1), pad_y(0, 100, 0));   /* alone, the brake reads the same either way */
    CHECK_EQ(pad_y(70, 0, 1), pad_y(70, 0, 0));
}

int main(void)
{
    test_exe_sums_the_pedals();
    test_brake_wins_over_the_gas();
    if (failures != 0) {
        fprintf(stderr, "test_joy_pad: %d failures\n", failures);
        return 1;
    }
    puts("test_joy_pad: ok");
    return 0;
}
