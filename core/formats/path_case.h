/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port (Linux): a file or folder found without regard to case. The game asks for its files in lower, upper or mixed case; the CD is ISO 9660 (upper case, lower case when
 * mounted on Linux) and a player's copy or a mod may be either. macOS and Windows ignore case; a Linux file system does not. */
#ifndef TNFS_PATH_CASE_H
#define TNFS_PATH_CASE_H

#include <stddef.h>

/* `dir`/`rel` ('/' between parts) with each part of `rel` found without regard to case (`dir` is taken as written); a part spelt as asked wins over another spelling.
 * 1 with the path as the file system spells it in `out`; 0 when it is not there or does not fit. */
int path_find_case(const char *dir, const char *rel, char *out, size_t cap);

/* The same for a whole path, when it is not known which folder is spelt right: its parts up to the first that is not there as written are taken as they are, the rest are
 * found without regard to case. 1 with the path as the file system spells it in `out` (`path` itself when it is there); 0 when it is not there or does not fit. */
int path_spelled(const char *path, char *out, size_t cap);

#endif
