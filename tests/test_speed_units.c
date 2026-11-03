/* The speed's units (speed_units_kmh): the player's own choice from extras.cfg, mph until it is made. */
#include "game/sys/port_extras.h"
#include "test.h"

#include "game/platform/platform.h"

#include <stddef.h>
#include <stdint.h>

/* port_extras.c links the platform's user file; this test never reads one */
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}
void platform_free_file(uint8_t *data) { (void)data; }

static void test_the_choice_gives_the_units(void)
{
    CHECK_EQ_INT(speed_units_kmh(UNITS_MPH), 0);
    CHECK_EQ_INT(speed_units_kmh(UNITS_KMH), 1);
}

static void test_the_default_is_mph(void)
{
    CHECK_EQ_INT(port_extras_defaults().units, UNITS_MPH);
}

/* The CD's files are not read by this test (the tables start as the built-in defaults). */
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path; (void)size; return NULL; }

int main(void)
{
    test_the_choice_gives_the_units();
    test_the_default_is_mph();
    return TEST_REPORT();
}
