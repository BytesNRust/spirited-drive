/* QFS: EA's compressed containers (*.QFS). The game's dispatcher (NFS_WIN.EXE
 * 004a2520) picks the codec from the first header byte (bit 0 cleared):
 *   0x10        RefPack LZ77   004a823c
 *   0x30/32/34  Huffman        004a83b0
 *   0x46        BTree pairs    004a96f4
 *   0x4A        RLE            004a9910 (unused on the TNFS SE CD)
 * Bit 0 set adds a 3-byte field before the 24-bit big-endian size. */
#ifndef TNFS_QFS_H
#define TNFS_QFS_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    QFS_OK = 0,
    QFS_ERR_HEADER,     /* not a QFS stream */
    QFS_ERR_TRUNCATED,  /* input ended before the stop code */
    QFS_ERR_OFFSET,     /* back-reference before the start of the output */
    QFS_ERR_OVERFLOW,   /* output does not fit the destination buffer */
    QFS_ERR_SIZE,       /* stream produced a size different from the header */
    QFS_ERR_CORRUPT,    /* structurally invalid stream (e.g. cyclic BTree node) */
    QFS_ERR_UNSUPPORTED /* valid header, codec not implemented yet */
} QfsResult;

int qfs_is_compressed(const uint8_t *src, size_t src_len);
QfsResult qfs_decompressed_size(const uint8_t *src, size_t src_len, size_t *out_size);

/* dst_len must be at least the size announced in the header. */
QfsResult qfs_decompress(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_len);

const char *qfs_result_str(QfsResult result);

#endif
