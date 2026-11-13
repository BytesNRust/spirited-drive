/* The anonymous counters: three one-shot events behind the one switch (update_check), flags in stats.cfg written before
 * sending, nothing from a dev build or with the switch Off. The sender is a seam: no test touches the network. */
#include "game/sys/port_stats.h"
#include "game/sys/port_extras.h"
#include "core/update/update_check.h"
#include "test.h"

#include "game/platform/platform.h"

#include <stdlib.h>
#include <string.h>

/* in-memory user files */
static struct { char name[32]; uint8_t data[1024]; size_t size; int present; } files[4];

static int slot_of(const char *name, int create)
{
    for (int i = 0; i < 4; i++)
        if (files[i].present && strcmp(files[i].name, name) == 0) return i;
    if (!create) return -1;
    for (int i = 0; i < 4; i++)
        if (!files[i].present) return files[i].present = 1, snprintf(files[i].name, sizeof files[i].name, "%s", name), i;
    return -1;
}

uint8_t *platform_read_user_file(const char *name, size_t *size)
{
    const int i = slot_of(name, 0);
    if (i < 0) return NULL;
    uint8_t *copy = malloc(files[i].size + 1);
    memcpy(copy, files[i].data, files[i].size);
    *size = files[i].size;
    return copy;
}

bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    const int i = slot_of(name, 1);
    if (i < 0 || size > sizeof files[i].data) return false;
    memcpy(files[i].data, data, size);
    files[i].size = size;
    return true;
}

void platform_free_file(uint8_t *data) { free(data); }

static int sent;
static char last_event[32], last_version[40];
static void fake_sender(const char *event, const char *version)
{
    sent++;
    snprintf(last_event, sizeof last_event, "%s", event);
    snprintf(last_version, sizeof last_version, "%s", version);
}

static void fresh(const char *version, int on)
{
    memset(files, 0, sizeof files);
    sent = 0;
    last_event[0] = last_version[0] = 0;
    PortExtras e = port_extras_load();
    e.update_check = on;
    port_extras_save(&e);
    port_stats_set_sender(fake_sender);
    port_stats_set_version(version);
}

static void stats_off_sends_nothing(void)
{
    fresh("0.9.0", 0);
    port_stats_event(STATS_FIRST_START);
    port_stats_event(STATS_DATA_READY);
    port_stats_event(STATS_FIRST_RACE);
    CHECK_EQ_INT(sent, 0);
    CHECK_EQ_INT(slot_of("stats.cfg", 0), -1);   /* not even a flag file */
}

static void stats_once_survives_restart(void)
{
    fresh("0.9.0", 1);
    port_stats_event(STATS_FIRST_START);
    port_stats_event(STATS_FIRST_START);
    CHECK_EQ_INT(sent, 1);
    CHECK(strcmp(last_event, "first_start") == 0);
    CHECK(strcmp(last_version, "0.9.0") == 0);
    port_stats_set_sender(fake_sender);   /* a new run: only the file carries over */
    port_stats_set_version("0.9.0");
    port_stats_event(STATS_FIRST_START);
    CHECK_EQ_INT(sent, 1);
    const int i = slot_of("stats.cfg", 0);
    CHECK(i >= 0);
    if (i >= 0) CHECK(strstr((const char *)files[i].data, "first_start=1") != NULL);
    port_stats_event(STATS_DATA_READY);   /* the others are their own flags */
    port_stats_event(STATS_FIRST_RACE);
    CHECK_EQ_INT(sent, 3);
    CHECK(strcmp(last_event, "first_race") == 0);
}

static void stats_dev_build_sends_nothing(void)
{
    fresh("0.9.0-dev+abc", 1);
    port_stats_event(STATS_FIRST_START);
    CHECK_EQ_INT(sent, 0);
    CHECK_EQ_INT(slot_of("stats.cfg", 0), -1);   /* so the flag is still free for the release */
}

static void stats_sends_the_version_without_its_build(void)
{
    fresh("0.9.0-beta.1+abc", 1);
    port_stats_event(STATS_FIRST_START);
    CHECK_EQ_INT(sent, 1);
    CHECK(strcmp(last_version, "0.9.0-beta.1") == 0);
}

static void stats_flag_is_written_before_sending(void)
{
    /* a store that cannot write: the event is lost, never sent twice */
    fresh("0.9.0", 1);
    for (int i = 0; i < 4; i++) files[i].present = 1, snprintf(files[i].name, sizeof files[i].name, "x%d", i);
    port_stats_event(STATS_FIRST_START);
    CHECK_EQ_INT(sent, 0);
}

static void stats_command_is_bounded(void)
{
    const char *cmd = stats_command_for_tests(STATS_FIRST_RACE, "0.9.0");
    CHECK(cmd != NULL);
    if (cmd == NULL) return;
    CHECK(strstr(cmd, "--max-time 5") != NULL);
    CHECK(strstr(cmd, "--proto =https") != NULL);
    CHECK(strstr(cmd, "-X POST") != NULL);
    CHECK(strstr(cmd, "e=first_race") != NULL);
    CHECK(strstr(cmd, "v=0.9.0") != NULL);
    const char *v = strstr(cmd, "v=") + 2;
    for (; *v != '&' && *v != '"' && *v != 0; v++) CHECK(strchr("0123456789.-", *v) != NULL || (*v >= 'A' && *v <= 'Z') || (*v >= 'a' && *v <= 'z'));
    cmd = stats_command_for_tests(STATS_FIRST_START, "0.9.0-beta.1+abc");
    CHECK(cmd != NULL && strstr(cmd, "v=0.9.0-beta.1&p=" UPDATE_API_PLATFORM "\"") != NULL && strstr(cmd, "+abc") == NULL);   /* the system last */
    CHECK(stats_command_for_tests(STATS_FIRST_START, "0.9.0;ls") == NULL);
}

static void event_names(void)
{
    CHECK(strcmp(stats_event_name(STATS_FIRST_START), "first_start") == 0);
    CHECK(strcmp(stats_event_name(STATS_DATA_READY), "data_ready") == 0);
    CHECK(strcmp(stats_event_name(STATS_FIRST_RACE), "first_race") == 0);
}

int main(void)
{
    stats_off_sends_nothing();
    stats_once_survives_restart();
    stats_dev_build_sends_nothing();
    stats_sends_the_version_without_its_build();
    stats_flag_is_written_before_sending();
    stats_command_is_bounded();
    event_names();
    return TEST_REPORT();
}
