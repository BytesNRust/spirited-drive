/* Internal: one decoder per EA compression codec, dispatched from qfs.c.
 * Each decodes src[data_start..src_len) into exactly dst_len bytes. */
#ifndef TNFS_QFS_CODECS_H
#define TNFS_QFS_CODECS_H

#include "formats/qfs.h"

QfsResult qfs_refpack_decode(const uint8_t *src, size_t src_len, size_t data_start, uint8_t *dst, size_t dst_len);
QfsResult qfs_refpack_decode_at(const uint8_t *src, size_t src_len, size_t data_start, uint8_t *dst, size_t dst_len, size_t *at);
QfsResult qfs_huffman_decode(const uint8_t *src, size_t src_len, size_t data_start, uint8_t *dst, size_t dst_len);
QfsResult qfs_btree_decode(const uint8_t *src, size_t src_len, size_t data_start, uint8_t *dst, size_t dst_len);

#endif
