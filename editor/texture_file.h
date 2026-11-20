/* «Show texture file» (owner, dep85 follow-up): the bank PNG a texture of the open car is painted in, so the Mask panel can show it in the
 * Finder. A car kit's bank folders hold it (formats/car_kit_paths.h: model/near/bank, model/far/bank),
 * named by the bank's entries.tsv. Pure C, no window. */
#ifndef TNFS_TRACK_EDITOR_TEXTURE_FILE_H
#define TNFS_TRACK_EDITOR_TEXTURE_FILE_H

#include <stddef.h>

/* The PNG of texture `name` (its 4 bytes as the car file spells them) of bank `level` (0 near, 1 far) in the kit at `kit_root`: 1 with `out`
 * the file's path, which is there; 0 with `out` "" when the folder is not a kit, the bank has no such bitmap, its file is missing or the
 * path does not fit `cap`. */
int texture_file_of(const char *kit_root, int level, const char *name, char *out, size_t cap);

#endif
