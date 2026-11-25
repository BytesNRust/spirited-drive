/* port: what a bug report needs to know about the machine that the game does not otherwise say. */
#ifndef TNFS_OS_INFO_H
#define TNFS_OS_INFO_H

#include <stddef.h>

/* The operating system's version in `out` (NUL-terminated, cut to `cap`): "15.1" on macOS (the product version, not Darwin's), "10.0 (build 22631)" on Windows (a build of 22000 or more is Windows 11),
 * "Linux 6.8.0" on the others; "unknown" when it cannot be read. */
void os_info_version(char *out, size_t cap);

/* The architecture this program was built for: "arm64", "x86_64" or "unknown". */
const char *os_info_build_arch(void);

#endif
