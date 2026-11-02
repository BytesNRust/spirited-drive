/* port (the first-run wizard): the CD's files copied into the player's own folder, once. The copy goes into <dest>.part and is renamed to <dest> when whole, so a
 * copy that was interrupted is never taken for a good one (the next run starts it again); the names keep the case they have. */
#ifndef TNFS_DATA_COPY_H
#define TNFS_DATA_COPY_H

#include <stdint.h>

/* called after each file with the bytes copied so far; return 0 to cancel */
typedef int (*DataCopyProgress)(void *user, int64_t bytes);

/* The size in bytes of the files under `src` (a folder), -1 when it cannot be read. */
int64_t data_copy_size(const char *src);

/* Copies `src` to `dest` (which must not exist). Returns 1 when done, 0 on any failure or a cancel (`dest` is then not left behind, and `dest`.part is removed). */
int data_copy_tree(const char *src, const char *dest, DataCopyProgress progress, void *user);

#endif
