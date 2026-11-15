/* The zip a mod is installed from (dep59 n4). The central directory is read once and every limit checked on it, before anything is
 * unpacked: the archive's size, the count of entries, each file's size, their total and how much a deflated entry may grow (a bomb).
 * Only stored and deflated entries, one disk, no zip64, no encryption. An entry's bytes are unpacked into a buffer of exactly the size the
 * directory gives (stb_image's inflate), and their CRC checked. Every entry's local header and bytes must lie before the central
 * directory and apart from every other entry's (no overlapping bomb). Names are kept as written (with '/'), and refused when they are
 * empty or too long, hold anything but printable ASCII, or what Windows cannot hold (\ : * ? " < > |, a component ending with '.' or
 * ' ', a device's name such as CON or COM1); whether a path is safe and wanted is the mod check's (mod_check.h). */
#ifndef TNFS_ZIP_H
#define TNFS_ZIP_H

#include "formats/exchange.h"

#include <stddef.h>
#include <stdint.h>

enum {
    ZIP_MAX_SIZE = 256u << 20,    /* the archive */
    ZIP_MAX_ENTRIES = 4096,
    ZIP_MAX_FILE = 64u << 20,     /* one entry, unpacked */
    ZIP_MAX_TOTAL = 512u << 20,   /* every entry, unpacked */
    ZIP_MAX_RATIO = 200,          /* a deflated entry may grow up to this many times */
    ZIP_NAME_MAX = 255
};

typedef enum {
    ZIP_OK = 0,
    ZIP_ERR_TOO_BIG,        /* the archive is over ZIP_MAX_SIZE */
    ZIP_ERR_NOT_ZIP,        /* no end of central directory */
    ZIP_ERR_TRUNCATED,      /* a record or an entry's bytes run past the archive, or their sizes disagree */
    ZIP_ERR_ZIP64,
    ZIP_ERR_MULTIDISK,
    ZIP_ERR_TOO_MANY,       /* over ZIP_MAX_ENTRIES */
    ZIP_ERR_ENCRYPTED,
    ZIP_ERR_METHOD,         /* neither stored nor deflated */
    ZIP_ERR_NAME,
    ZIP_ERR_FILE_TOO_BIG,   /* an entry over ZIP_MAX_FILE */
    ZIP_ERR_TOTAL_TOO_BIG,  /* the entries together over ZIP_MAX_TOTAL */
    ZIP_ERR_RATIO,          /* a deflated entry that would grow more than ZIP_MAX_RATIO times */
    ZIP_ERR_INFLATE,        /* the deflated bytes are damaged or do not give the size the directory says */
    ZIP_ERR_CRC,
    ZIP_ERR_OVERLAP,        /* two entries' bytes overlap, or one runs into the central directory */
    ZIP_ERR_MEMORY
} ZipResult;

typedef struct {
    char name[ZIP_NAME_MAX + 1];
    int is_dir;          /* the name ends with '/' */
    int is_special;      /* its mode says neither a file nor a folder: a link, a device, a pipe */
    uint16_t method;     /* 0 stored, 8 deflated */
    uint32_t crc, csize, usize, local_offset;
} ZipEntry;

typedef struct {
    const uint8_t *data;   /* the caller's archive, not copied: it must outlive the ZipArchive */
    size_t size;
    ZipEntry *entries;
    size_t count;
} ZipArchive;

/* The directory of the archive in `data`. ZIP_OK fills `out` (zip_close frees it); otherwise `out` holds nothing to free. */
ZipResult zip_open(const uint8_t *data, size_t size, ZipArchive *out);
/* Entry `i` unpacked into a new buffer (the caller frees it; never NULL on ZIP_OK, even for an empty file); on any other result *out is NULL. */
ZipResult zip_extract(const ZipArchive *z, size_t i, uint8_t **out, size_t *out_size);
void zip_close(ZipArchive *z);
/* The files of `set` as a zip, every entry stored (no compression), in the set's order, names as they are; malloc'd, its size in
 * `size`. NULL when a name does not fit ZIP_NAME_MAX, the set is empty or holds more than the reader's entry limit, the archive
 * would pass the reader's size limit, or memory ran out: what it writes, zip_open reads. No clock goes in (every entry's time is
 * 1980-01-01 00:00), so the same set gives the same bytes. */
uint8_t *zip_write_stored(const FileSet *set, size_t *size);
/* A short English phrase for the result, for the player and the log */
const char *zip_result_text(ZipResult r);

#endif
