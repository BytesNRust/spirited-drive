/* The speed's units (speed_units_kmh): the language's km/h for German, or the player's own choice from extras.cfg, which
 * leaves the language as it is. */
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

static void test_language_gives_the_units(void)
{
    CHECK_EQ_INT(speed_units_kmh(UNITS_LANGUAGE, 0), 0);   /* English: mph */
    CHECK_EQ_INT(speed_units_kmh(UNITS_LANGUAGE, 1), 1);   /* German: km/h */
}

static void test_the_choice_overrides_the_language(void)
{
    CHECK_EQ_INT(speed_units_kmh(UNITS_KMH, 0), 1);
    CHECK_EQ_INT(speed_units_kmh(UNITS_KMH, 1), 1);
    CHECK_EQ_INT(speed_units_kmh(UNITS_MPH, 0), 0);
    CHECK_EQ_INT(speed_units_kmh(UNITS_MPH, 1), 0);   /* German words with mph: the choice wins */
}

static void test_the_default_is_the_exe(void)
{
    CHECK_EQ_INT(port_extras_defaults().units, UNITS_LANGUAGE);
}

int main(void)
{
    test_language_gives_the_units();
    test_the_choice_overrides_the_language();
    test_the_default_is_the_exe();
    return TEST_REPORT();
}
