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

/* A log file for the player to attach to a bug report: `path` (the user directory's tnfs.log) is opened afresh each run, the last run's kept beside it as `path`.1, and every
 * line port_log, port_out and port_fatal say goes into it too, verbose or not (the console stays as it was). It stops at PORT_LOG_FILE_MAX bytes with a line saying so.
 * 1 when it is open, 0 when it could not be (the game goes on without). */
enum { PORT_LOG_FILE_MAX = 4 * 1024 * 1024 };
int port_log_open_file(const char *path);
void port_log_close_file(void);

/* 1 when TNFS_VERBOSE is set to anything but "" or "0" (read once), or port_log_set_verbose(1) was called */
int port_log_verbose(void);
void port_log_set_verbose(int on);

/* stderr, with TNFS_VERBOSE only */
void port_log(const char *format, ...) PORT_LOG_FORMAT(1, 2);
/* stdout, with TNFS_VERBOSE only (the race's results) */
void port_out(const char *format, ...) PORT_LOG_FORMAT(1, 2);
/* stderr, always: the errors that end the process or make the game unusable */
void port_fatal(const char *format, ...) PORT_LOG_FORMAT(1, 2);

#endif
