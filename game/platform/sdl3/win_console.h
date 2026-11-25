/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: Windows release builds are GUI programs (no console window beside the game), so a terminal sees nothing of what they print unless they ask for it. */
#ifndef TNFS_WIN_CONSOLE_H
#define TNFS_WIN_CONSOLE_H

/* Windows: when standard output is not already a file or a pipe (a redirect keeps working), the program takes the console of the shell that started it (cmd, PowerShell) for stdout and stderr; and with
 * `--console` among the arguments it opens a console of its own when it has none (started from Explorer), to read the log's lines live. Nothing elsewhere, and nothing in a console-subsystem build. */
void win_console_attach(int argc, char **argv);

#endif
