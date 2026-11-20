/* The pack of tnfs-studio (`tnfs-studio pack DIR OUT --from ORIGINAL`), a function the editor's Build shares with the command line (the tnfs_tools library). */
#ifndef TNFS_STUDIO_PACK_TREE_H
#define TNFS_STUDIO_PACK_TREE_H

#include "formats/exchange.h"

#include <stdint.h>

/* The tree `tree` (an unpacked file's folder) packed over the file `from_path` it was unpacked from into `out_path`; `palette` (RGB, 768 bytes) or NULL as unpack's
 * --palette. Nothing is overwritten without `force`. Says the size written on stdout. 0 with `err` filled on failure. */
int studio_pack(const char *tree, const char *out_path, const char *from_path, const uint8_t *palette, int force, ExchangeError *err);

/* the last part of a path, and "path: message" in `err` unless the message already starts with the file's name */
const char *studio_base_name(const char *path);
void studio_name_the_file(ExchangeError *err, const char *path);

#endif
