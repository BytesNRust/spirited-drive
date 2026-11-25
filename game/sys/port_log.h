/* port: what the game writes to the console. A release build says nothing but its fatal errors (the ones that end the
 * process or leave the game unusable: no data, no window, no fonts); everything else -- the race's results, a missing
 * shape, a file that could not be read or saved -- is written only with TNFS_VERBOSE=1 in the environment or --verbose
 * on the command line. The explicit modes' own results (--view, --track, --movie, TNFS_TRACE_PLAYER) are their output
 * and stay as they are. The tools that read the logs (golden_frames.sh, link_loopback.sh, sweep_asan.sh) set
 * TNFS_VERBOSE=1. */
#ifndef TNFS_PORT_LOG_H
#define TNFS_PORT_LOG_H

#if defined(__GNUC__) || defined(__clang__)
#define PORT_LOG_FORMAT(a, b) __attribute__((format(printf, a, b)))
#else
#define PORT_LOG_FORMAT(a, b)
#endif

/* A log file for the player to attach to a bug report: `path` (the user directory's spirited-drive.log) is opened afresh each run, the last run's kept beside it as `path`.1, and every
 * line port_log, port_out and port_fatal say goes into it too, verbose or not (the console stays as it was). It stops at PORT_LOG_FILE_MAX bytes with a line saying so.
 * 1 when it is open, 0 when it could not be (the game goes on without). */
enum { PORT_LOG_FILE_MAX = 4 * 1024 * 1024 };
int port_log_open_file(const char *path);
void port_log_close_file(void);
/* port_log_open_file keeps FIVE runs, this one and the four before it: `path` and, when it ends in ".log" (as the game's does), `path` with "-1.log" to "-4.log" in place of ".log" (the
 * newest first; else `path` with ".1" to ".4" added), files a bug report can attach. A run ends with the line "exit: clean" (port_log_exit, which an atexit hook calls once a file is
 * open; "exit: after a fatal error" when port_fatal was said), even after the size limit. A run whose log has no "exit:" line did not end (a crash, a kill, a power cut): the next run
 * opens with the line "previous run: did not end cleanly (kept as ...-unclean.log)" and keeps that log in "-unclean.log" too, which no later rotation drops (the next unclean one
 * replaces it). */
void port_log_exit(void);

/* port (test harness): the scripted input (TNFS_INPUT_SCRIPT's WAIT, platform_sdl3.c) waits for a line the game logs. port_log_watch arms a watch for `text` (at most 63 bytes);
 * port_log_watch_hit is 1 once a line containing it has gone through port_log, port_out or port_fatal since, whether or not the console shows it. Lines logged before the watch was
 * armed do not count. */
void port_log_watch(const char *text);
int port_log_watch_hit(void);

/* 1 when TNFS_VERBOSE is set to anything but "" or "0" (read once), or port_log_set_verbose(1) was called */
int port_log_verbose(void);
void port_log_set_verbose(int on);

/* stderr, with TNFS_VERBOSE only */
void port_log(const char *format, ...) PORT_LOG_FORMAT(1, 2);
/* a developer's detail (each mask file, each car's paint and lamp counts): the file and stderr with TNFS_VERBOSE only. The log a player
 * sends keeps what serves them or a bug report (port_log): the version, the system, the race, what a mod or a dev override changed, the errors. */
void port_log_detail(const char *format, ...) PORT_LOG_FORMAT(1, 2);
/* stdout, with TNFS_VERBOSE only (the race's results) */
void port_out(const char *format, ...) PORT_LOG_FORMAT(1, 2);
/* stderr, always: the errors that end the process or make the game unusable */
void port_fatal(const char *format, ...) PORT_LOG_FORMAT(1, 2);

#endif
