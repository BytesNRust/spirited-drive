/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: see port_stats.h. */
#include "game/sys/port_stats.h"

#include "core/update/update_check.h"
#include "game/platform/platform.h"
#include "game/sys/port_extras.h"
#include "tnfs_version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STATS_FILE "stats.cfg"

static StatsSender sender;
static const char *version_override;

const char *stats_event_name(StatsEvent e)
{
    static const char *const NAMES[STATS_EVENT_COUNT] = { "first_start", "data_ready", "first_race" };
    return (unsigned)e < STATS_EVENT_COUNT ? NAMES[e] : "";
}

void port_stats_set_sender(StatsSender s) { sender = s; }
void port_stats_set_version(const char *version) { version_override = version; }

static const char *current_version(void) { return version_override != NULL ? version_override : TNFS_VERSION; }

/* the file's lines "<event>=1" */
static int flag_set(const char *text, const char *name)
{
    char line[48];
    snprintf(line, sizeof line, "%s=1", name);
    for (const char *p = text; (p = strstr(p, line)) != NULL; p += strlen(line))
        if ((p == text || p[-1] == '\n') && (p[strlen(line)] == '\n' || p[strlen(line)] == '\0' || p[strlen(line)] == '\r')) return 1;
    return 0;
}

void port_stats_event(StatsEvent e)
{
    char version[40];
    if ((unsigned)e >= STATS_EVENT_COUNT || sender == NULL) return;
    if (!port_extras_load().update_check) return;
    const char *const raw = current_version();
    if (update_is_dev_build(raw) || !update_api_version(raw, version, sizeof version)) return;

    const char *const name = stats_event_name(e);
    size_t size = 0;
    uint8_t *old = platform_read_user_file(STATS_FILE, &size);
    char text[256];
    size_t have = old != NULL && size < sizeof text - 32 ? size : 0;
    if (have > 0) memcpy(text, old, have);
    text[have] = '\0';
    if (old != NULL) platform_free_file(old);
    if (flag_set(text, name)) return;
    if (have > 0 && text[have - 1] != '\n') text[have++] = '\n';
    const int added = snprintf(text + have, sizeof text - have, "%s=1\n", name);
    if (added <= 0 || (size_t)added >= sizeof text - have) return;
    /* the flag first: a crash after it loses the event, it never sends it twice */
    if (!platform_write_user_file(STATS_FILE, (const uint8_t *)text, have + (size_t)added)) return;
    sender(name, version);
}

const char *stats_command_for_tests(StatsEvent e, const char *version)
{
    static char command[512];
    if ((unsigned)e >= STATS_EVENT_COUNT) return NULL;
    return update_api_command(command, sizeof command, stats_event_name(e), version) ? command : NULL;
}
