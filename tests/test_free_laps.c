/* Free laps (free_laps.h): the exe's lap counts, the setting from the environment, the running race's count. No game data. */
#include "game/sys/free_laps.h"
#include "test.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/* port.cfg is not there: the platform's user files are empty (the setting's own parse is test_port_settings's) */
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name; (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size) { (void)name, (void)data, (void)size; return false; }
void platform_free_file(uint8_t *data) { free(data); }

static void test_exe_counts(void)
{
    const int exe[] = { 2, 4, 6, 8, 12, 16 };
    for (size_t i = 0; i < sizeof exe / sizeof *exe; i++) CHECK(free_laps_is_exe(exe[i]));
    CHECK(!free_laps_is_exe(0));
    CHECK(!free_laps_is_exe(1));
    CHECK(!free_laps_is_exe(3));
    CHECK(!free_laps_is_exe(17));
    CHECK(!free_laps_is_exe(99));
}

static void test_environment(void)
{
    setenv("TNFS_LAPS", "17", 1);
    CHECK_EQ_INT(port_laps(), 17);
    setenv("TNFS_LAPS", "99", 1);
    CHECK_EQ_INT(port_laps(), 99);
    setenv("TNFS_LAPS", "0", 1);
    CHECK_EQ_INT(port_laps(), 0);
    const char *refused[] = { "100", "-3", "x", "5x", "" };
    for (size_t i = 0; i < sizeof refused / sizeof *refused; i++) {
        setenv("TNFS_LAPS", refused[i], 1);
        CHECK(port_laps() >= 0 && port_laps() <= FREE_LAPS_MAX);   /* not the env's: port.cfg's (none here) */
        CHECK(port_laps() != 100);
    }
    unsetenv("TNFS_LAPS");
}

static void test_running_race(void)
{
    CHECK_EQ_INT(free_laps_count(), 0);
    CHECK_EQ_INT(free_laps_or(8), 8);
    free_laps_set(25);
    CHECK_EQ_INT(free_laps_count(), 25);
    CHECK_EQ_INT(free_laps_or(8), 25);
    free_laps_set(100);   /* out of range: none */
    CHECK_EQ_INT(free_laps_count(), 0);
    free_laps_set(0);
    CHECK_EQ_INT(free_laps_or(4), 4);
}

int main(void)
{
    test_exe_counts();
    test_environment();
    test_running_race();
    return TEST_REPORT();
}
