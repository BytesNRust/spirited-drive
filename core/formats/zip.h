/* The zip a mod is installed from (dep59 n4). The central directory is read once and every limit checked on it, before anything is
 * unpacked: the archive's size, the count of entries, each file's size, their total and how much a deflated entry may grow (a bomb).
 * Only stored and deflated entries, one disk, no zip64, no encryption. An entry's bytes are unpacked into a buffer of exactly the size the
 * directory gives (stb_image's inflate), and their CRC checked. Names are kept as written (with '/'), refused only when they are empty, too
 * long, or hold a control character, a backslash or a colon; whether a path is safe and wanted is the mod check's (mod_check.h). */
#ifndef TNFS_ZIP_H
#define TNFS_ZIP_H

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
    ZIP_ERR_MEMORY
} ZipResult;

typedef struct {
    char name[ZIP_NAME_MAX + 1];
    int is_dir;          /* the name ends with '/' */
    int is_link;         /* made on Unix with a symbolic link's mode */
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
/* A short English phrase for the result, for the player and the log */
const char *zip_result_text(ZipResult r);

#endif
