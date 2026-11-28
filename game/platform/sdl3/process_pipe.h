/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: run a program and read what it writes to its standard output (the update check's curl). On Windows the program is started with CreateProcess and no window, its standard error and input
 * pointing at NUL and with no shell (so no console window flashes from a program that has none, and a GUI program has no stdio for _popen to inherit); elsewhere it is popen's, the shell's command line. */
#ifndef TNFS_PROCESS_PIPE_H
#define TNFS_PROCESS_PIPE_H

#include <stddef.h>

typedef struct ProcessPipe ProcessPipe;

/* `command_line` started; NULL when it could not be (not found, too long: 2047 bytes at most on Windows). */
ProcessPipe *process_pipe_open(const char *command_line);

/* up to `cap` bytes of the program's output; 0 at its end */
size_t process_pipe_read(ProcessPipe *pipe, void *buf, size_t cap);

/* the pipe closed and the program waited for (Windows: 15 s, then ended); its exit status (0 when all went well; popen's shell says 127 for a program that is not there), or -1 when it did not end normally. `pipe` is freed. */
int process_pipe_close(ProcessPipe *pipe);

#endif
