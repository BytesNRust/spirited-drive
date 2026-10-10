/* Whole game files <-> editable file trees: the layer the CLI drives. A file is unpacked (QFS removed), its
 * kind found from the content and the name (SHPI, "wwww" tree, TRI, PBS, PDN, BNK, anything else is raw),
 * and handed to that kind's exchange. The export holds resource.tsv (kind, whether it was packed) and the
 * kind's files; trees put each child under c<i>/ and list every node in tree.tsv.
 *
 * An import takes the ORIGINAL file again and the edited tree. When the edited payload equals the original's
 * the original bytes are returned untouched (a packed file keeps its original compressed stream); otherwise
 * a file that was packed is packed again with RefPack (qfs.h), which the game's loader unpacks in place or
 * the import refuses. */
#ifndef TNFS_RESOURCE_EXCHANGE_H
#define TNFS_RESOURCE_EXCHANGE_H

#include "formats/exchange.h"

/* `name` is the file name (its extension helps tell PBS, PDN and BNK); `palette` (768 RGB bytes or NULL) is
 * used for bitmaps whose container holds no palette. */
int resource_export(const uint8_t *file, size_t size, const char *name, const uint8_t *palette, FileSet *out, ExchangeError *err);
int resource_import(const uint8_t *original, size_t size, const char *name, const uint8_t *palette, const FileSet *in,
                    uint8_t **out, size_t *out_len, ImportStats *stats, ExchangeError *err);

#endif
