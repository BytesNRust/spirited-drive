/* The player's car as Control Central starts (central_select.c central_car_start): the original sends the Warrior
 * (11) back to car 0; the port's "Warrior Player Car" extra keeps it while the Warrior is on offer. */
#include "game/frontend/central_run.h"
#include "test.h"

#include "game/platform/platform.h"

/* central_draw.c reaches variant_rule.c, which reads extras.cfg through the platform; the test has no file */
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}
void platform_free_file(uint8_t *data) { (void)data; }

static int16_t start(int16_t car, int16_t warrior, const int16_t *keep)
{
    ControlCentralState st = { 0 };
    st.player_car = &car, st.warrior = &warrior, st.warrior_keep = keep;
    central_car_start(&st);
    return car;
}

/* The CD's files are not read by this test (the tables start as the built-in defaults). */
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path; (void)size; return NULL; }

int main(void)
{
    const int16_t on = 1, off = 0;
    CHECK_EQ_INT(start(11, 1, NULL), 0);   /* the original's */
    CHECK_EQ_INT(start(11, 1, &off), 0);
    CHECK_EQ_INT(start(7, 1, NULL), 7);    /* the others stay */
    CHECK_EQ_INT(start(11, 1, &on), 11);   /* kept */
    CHECK_EQ_INT(start(11, 0, &on), 0);    /* not on offer: no Warrior to keep */
    CHECK_EQ_INT(start(3, 1, &on), 3);
    return TEST_REPORT();
}
