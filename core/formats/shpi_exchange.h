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

/* The bank's flat files (dep85): entries.tsv, a row per entry in the directory's order with its physical place in `pos`, and
 * bank.tsv, the head shpi_build would otherwise make up: `tag` (bytes 12..15), `lead_hex` (the bytes between the directory and the
 * first entry) and `size_word` (bytes 4..7: auto when it is the bank's length; a number for a CD bank whose CFM holds bytes of its own
 * after it, which travel in the last entry's tail as they always have), always all three. */
#define SHPI_ENTRIES_FILE "entries.tsv"
#define SHPI_BANK_FILE "bank.tsv"

/* external_palette: 768 RGB bytes or NULL. 1 on success; 0 with err filled. */
int shpi_export(const uint8_t *data, size_t size, const uint8_t *external_palette, FileSet *out, ExchangeError *err);

enum { SHPI_FILE_NAME_SIZE = 24 };   /* 10 digits, _, 4, .png */

/* The file shpi_export writes for the bitmap entry named `name` (the first of that name in the directory): NNNN_LABEL.png, its
 * directory index and its name made file-name safe. 1 when the SHPI has such a bitmap; 0 otherwise (`file` empty). The MTL of a
 * mesh's export names its textures by it (mesh_exchange.h). */
int shpi_entry_file(const uint8_t *data, size_t size, const char name[4], char file[SHPI_FILE_NAME_SIZE]);

/* fix/edited-preview: the bitmap entry named `name` (the first of that name) as RGBA, its colours from the palette shpi_export's PNG
 * has (the SHPI's first palette entry, else a grey ramp), its texels of palette index `key` transparent. 1 with *rgba (malloc'd), its
 * size and the count of key texels in *keyed; 0 when the SHPI has no such bitmap, its pixels do not fit in it, or memory runs out */
int shpi_entry_rgba(const uint8_t *data, size_t size, const char name[4], uint8_t key, uint8_t **rgba, uint32_t *w, uint32_t *h,
                    uint32_t *keyed);

/* Writes the new SHPI in *out (malloc'd, free with free()). Pixels from RGBA or from an indexed PNG whose
 * palette differs from the resource's are moved to the nearest palette colour and counted in stats. */
int shpi_import(const uint8_t *original, size_t size, const uint8_t *external_palette, const FileSet *in,
                uint8_t **out, size_t *out_len, ImportStats *stats, ExchangeError *err);

/* dep85: the SHPI built from its flat files alone, no bank to start from: bank.tsv's head, entries.tsv's rows placed by `pos` (a row
 * without one is in its directory place), each bitmap's PNG (an indexed one on the bank's palette copied, any other moved to the
 * nearest colour of the first palette in directory order and counted in stats), each other entry's .bin. The unedited flat files of
 * a bank give it back byte for byte. 1 with *out (malloc'd); 0 with the file, line or entry in err. `in` is only read. */
int shpi_from_flat(const FileSet *in, const uint8_t *external_palette, uint8_t **out, size_t *out_len, ImportStats *stats, ExchangeError *err);

#endif
