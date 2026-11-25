/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: see win_console.h. */
#include "game/platform/sdl3/win_console.h"

#if defined(_WIN32)
#include <windows.h>

#include <stdio.h>
#include <string.h>

static int has_output(void)
{
    const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    return out != NULL && out != INVALID_HANDLE_VALUE;   /* a redirect (a file, a pipe) or a console already */
}

void win_console_attach(int argc, char **argv)
{
    int wanted = 0;
    for (int i = 1; i < argc; i++) wanted |= strcmp(argv[i], "--console") == 0;
    if (has_output() && !wanted) return;
    if (!AttachConsole(ATTACH_PARENT_PROCESS) && (!wanted || !AllocConsole())) return;   /* no shell to show it in: the log file is what is left */
    if (!has_output() || GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) == FILE_TYPE_UNKNOWN) {
        (void)freopen("CONOUT$", "w", stdout);
        (void)freopen("CONOUT$", "w", stderr);
    }
}
#else
void win_console_attach(int argc, char **argv)
{
    (void)argc, (void)argv;
}
#endif
