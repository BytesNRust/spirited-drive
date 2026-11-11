/* port (the first-run wizard): the CD's files copied into the player's own folder, once. The copy goes into <dest>.part and is renamed to <dest> when whole, so a
 * copy that was interrupted is never taken for a good one (the next run starts it again); the names keep the case they have. */
#ifndef TNFS_DATA_COPY_H
#define TNFS_DATA_COPY_H

#include <stdint.h>

/* called after each file with the bytes copied so far; return 0 to cancel */
typedef int (*DataCopyProgress)(void *user, int64_t bytes);

/* The size in bytes of the files under `src` (a folder), -1 when it cannot be read. */
int64_t data_copy_size(const char *src);

/* Copies `src` to `dest`. A `dest` that exists is replaced as a whole: the copy goes into `dest`.part, the old one is moved to `dest`.old, the new one into place and the old one
 * deleted, so a failure or a cancel leaves one whole copy. Returns 1 when done, 0 on any failure or a cancel (`dest`.part is removed). */
int data_copy_tree(const char *src, const char *dest, DataCopyProgress progress, void *user);

/* port (ISO): the same from an .iso image of the CD. `root` is the folder inside the image that holds the files (DataCheck.root: "" the image's root). The names are the
 * image's own, without ";1" or a trailing dot; a folder met twice (a loop in the image) is not entered again. The size is -1 when the image cannot be read. */
int64_t data_copy_iso_size(const char *iso_path, const char *root);
int data_copy_iso(const char *iso_path, const char *root, const char *dest, DataCopyProgress progress, void *user);

/* Why the last data_copy_tree failed, "could not <what> <path>: <reason>" (also in spirited-drive.log); "" after a success. A folder that is a symbolic link is never entered, so a loop
 * cannot run the copy away; a link to a file is copied as the file. */
const char *data_copy_error(void);

#endif
