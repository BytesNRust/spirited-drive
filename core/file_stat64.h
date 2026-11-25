/* The file calls whose size can pass 2 GiB, with a 64-bit size on every system. MinGW's `struct stat` has a 32-bit st_size and its stat() fails with EOVERFLOW on a
 * file of 2 GiB or more (core/win_posix.h maps lstat to that stat); `struct _stati64` and _stati64() are the 64-bit pair. One name for the type and one for each call,
 * here, so the struct and the function can never be of different kinds. Include <sys/stat.h> first. */
#ifndef TNFS_FILE_STAT64_H
#define TNFS_FILE_STAT64_H

#include <sys/stat.h>

#ifdef _WIN32
#define TNFS_STAT struct _stati64
#define tnfs_stat(path, st) _stati64(path, st)
#define tnfs_lstat(path, st) _stati64(path, st)   /* no links to tell from their targets */
#else
#define TNFS_STAT struct stat
#define tnfs_stat(path, st) stat(path, st)
#define tnfs_lstat(path, st) lstat(path, st)
#endif

/* a compile-time check: a 32-bit st_size is an array of size -1 */
typedef char tnfs_stat_size_is_64_bits[sizeof(((TNFS_STAT *)0)->st_size) >= 8 ? 1 : -1];

#endif
