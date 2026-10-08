/* FSH: EA "SHPI" bitmap container (*.FSH, and the payload of most *.QFS).
 *
 *   header  "SHPI", u32 file size, u32 entry count, 4-char directory id
 *   dir     count x (4-char name, u32 offset from file start)
 *   entry   u8 type, u24 block size, u16 width, u16 height, 4 x i16 misc, data
 *
 * misc[2..3] is the screen position for UI bitmaps. All integers little-endian. */
#ifndef TNFS_FSH_H
#define TNFS_FSH_H

#include <stddef.h>
#include <stdint.h>

enum {
    FSH_TYPE_PALETTE_6BIT = 0x22,  /* VGA DAC values 0..63 */
    FSH_TYPE_PALETTE_8BIT = 0x24,
    FSH_TYPE_BITMAP = 0x7B         /* 8-bit indexed pixels */
};

typedef enum {
    FSH_OK = 0,
    FSH_ERR_HEADER,     /* not an SHPI container */
    FSH_ERR_BOUNDS,     /* directory or entry outside the data */
    FSH_ERR_NOT_FOUND,  /* no entry with that name */
    FSH_ERR_TYPE        /* entry is not of the requested kind */
} FshResult;

typedef struct {
    const uint8_t *data;
    size_t size;
    uint32_t count;
} Fsh;

typedef struct {
    char name[4];           /* not NUL-terminated */
    uint8_t type;
    uint32_t block_size;
    uint16_t width;
    uint16_t height;
    int16_t misc[4];
    const uint8_t *payload; /* bytes after the 16-byte entry header */
    size_t payload_size;    /* bytes available up to the end of the data */
} FshEntry;

/* Keeps a pointer to data; it must outlive the Fsh. */
FshResult fsh_open(Fsh *fsh, const uint8_t *data, size_t size);
FshResult fsh_find(const Fsh *fsh, const char name[4], uint32_t *index);
FshResult fsh_entry(const Fsh *fsh, uint32_t index, FshEntry *out);

/* Expands a palette entry to 256 RGB triplets as the game does. */
FshResult fsh_palette_rgb(const FshEntry *entry, uint8_t rgb[768]);

const char *fsh_result_str(FshResult result);

#endif
