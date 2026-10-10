/* SHPI (FSH) <-> PNG + TSV. An export gives, per bitmap entry, an indexed PNG named NNNN_LABEL.png (the
 * palette is the SHPI's first palette entry, else the external one, else a grey ramp); per other entry
 * (palettes, ...) a NNNN_LABEL.bin with the bytes after the 16-byte entry header; and entries.tsv with
 * everything else: name, type, block size, width, height, misc words, and the bytes after the pixels.
 * An import rebuilds the SHPI from the original's directory, entries.tsv and those files. Replace-only:
 * the entry count, order and names must match the original. An unedited export imports back to the
 * original bytes. */
#ifndef TNFS_SHPI_EXCHANGE_H
#define TNFS_SHPI_EXCHANGE_H

#include "formats/exchange.h"

/* external_palette: 768 RGB bytes or NULL. 1 on success; 0 with err filled. */
int shpi_export(const uint8_t *data, size_t size, const uint8_t *external_palette, FileSet *out, ExchangeError *err);

/* Writes the new SHPI in *out (malloc'd, free with free()). Pixels from RGBA or from an indexed PNG whose
 * palette differs from the resource's are moved to the nearest palette colour and counted in stats. */
int shpi_import(const uint8_t *original, size_t size, const uint8_t *external_palette, const FileSet *in,
                uint8_t **out, size_t *out_len, ImportStats *stats, ExchangeError *err);

#endif
