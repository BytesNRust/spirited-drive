/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: see process_pipe.h. */
#include "game/platform/sdl3/process_pipe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>

enum { LINE_MAX_BYTES = 2048, WAIT_MS = 15000 };

struct ProcessPipe {
    HANDLE read;
    HANDLE process;
};

ProcessPipe *process_pipe_open(const char *command_line)
{
    char line[LINE_MAX_BYTES];   /* CreateProcess may write into its command line */
    if (command_line == NULL || strlen(command_line) >= sizeof line) return NULL;
    memcpy(line, command_line, strlen(command_line) + 1);
    SECURITY_ATTRIBUTES inherit = { sizeof inherit, NULL, TRUE };
    HANDLE read = NULL, write = NULL;
    if (!CreatePipe(&read, &write, &inherit, 0)) return NULL;
    SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0);   /* only the child's end is inherited */
    HANDLE nul = CreateFileA("NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, 0, NULL);
    STARTUPINFOA info;
    memset(&info, 0, sizeof info);
    info.cb = sizeof info;
    info.dwFlags = STARTF_USESTDHANDLES;
    info.hStdInput = nul;
    info.hStdOutput = write;
    info.hStdError = nul;
    PROCESS_INFORMATION made;
    const BOOL started = nul != INVALID_HANDLE_VALUE && CreateProcessA(NULL, line, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &info, &made);
    CloseHandle(write);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    ProcessPipe *p = started ? malloc(sizeof *p) : NULL;
    if (p == NULL) {
        if (started) TerminateProcess(made.hProcess, 1), CloseHandle(made.hProcess);
        if (started) CloseHandle(made.hThread);
        CloseHandle(read);
        return NULL;
    }
    CloseHandle(made.hThread);
    p->read = read;
    p->process = made.hProcess;
    return p;
}

size_t process_pipe_read(ProcessPipe *pipe, void *buf, size_t cap)
{
    DWORD got = 0;
    if (pipe == NULL || cap == 0 || !ReadFile(pipe->read, buf, (DWORD)(cap > 65536 ? 65536 : cap), &got, NULL)) return 0;   /* a broken pipe is the program's end */
    return got;
}

int process_pipe_close(ProcessPipe *pipe)
{
    if (pipe == NULL) return -1;
    CloseHandle(pipe->read);
    DWORD code = (DWORD)-1;
    if (WaitForSingleObject(pipe->process, WAIT_MS) == WAIT_TIMEOUT) TerminateProcess(pipe->process, 1);
    else if (!GetExitCodeProcess(pipe->process, &code)) code = (DWORD)-1;
    CloseHandle(pipe->process);
    free(pipe);
    return code == 0 ? 0 : -1;
}

#elif defined(__EMSCRIPTEN__)

ProcessPipe *process_pipe_open(const char *command_line) { (void)command_line; return NULL; }   /* the web build runs no programs */
size_t process_pipe_read(ProcessPipe *pipe, void *buf, size_t cap) { (void)pipe, (void)buf, (void)cap; return 0; }
int process_pipe_close(ProcessPipe *pipe) { (void)pipe; return -1; }

#else

struct ProcessPipe {
    FILE *file;
};

ProcessPipe *process_pipe_open(const char *command_line)
{
    ProcessPipe *p = command_line != NULL ? malloc(sizeof *p) : NULL;
    if (p == NULL) return NULL;
    p->file = popen(command_line, "r");
    if (p->file == NULL) {
        free(p);
        return NULL;
    }
    return p;
}

size_t process_pipe_read(ProcessPipe *pipe, void *buf, size_t cap)
{
    return pipe != NULL ? fread(buf, 1, cap, pipe->file) : 0;
}

int process_pipe_close(ProcessPipe *pipe)
{
    if (pipe == NULL) return -1;
    const int status = pclose(pipe->file);
    free(pipe);
    return status == 0 ? 0 : -1;
}

#endif
