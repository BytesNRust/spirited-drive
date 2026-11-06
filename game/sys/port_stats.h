/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: three anonymous one-shot counters, sent to our API (core/update/update_check.h) and only when the one switch
 * (the mode's file's update_check, "Updates and anonymous stats") is On: the first start, the data ready after the first-run
 * copy, the first finished race. Nothing but the event's name and the version goes out. Each event is sent once per
 * install: its flag is written to <user dir>/stats.cfg (the player's folder, --user DIR; a mods folder, --mods, is never written) BEFORE it is sent, so a crash can only lose an event, never
 * repeat one. A development build ("-dev") sends nothing. */
#ifndef TNFS_PORT_STATS_H
#define TNFS_PORT_STATS_H

typedef enum { STATS_FIRST_START, STATS_DATA_READY, STATS_FIRST_RACE, STATS_EVENT_COUNT } StatsEvent;

/* "first_start", "data_ready", "first_race" */
const char *stats_event_name(StatsEvent e);

/* The send is a seam: the game sets the platform's (platform_stats_post), the tests a fake; none set sends nothing.
 * `version` is the API's form (no "+build"). */
typedef void (*StatsSender)(const char *event, const char *version);
void port_stats_set_sender(StatsSender sender);

/* The version the events carry; default TNFS_VERSION. NULL back to the default. */
void port_stats_set_version(const char *version);

/* Sends the event once per install, unless the switch is Off, this is a development build, the flag is already
 * written, or it cannot be written. */
void port_stats_event(StatsEvent e);

/* The curl command a send would run (for the tests), NULL when the version is not one the API takes */
const char *stats_command_for_tests(StatsEvent e, const char *version);

#endif
