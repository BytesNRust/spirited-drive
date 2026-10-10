#include "formats/qfs.h"

#include "formats/qfs_codecs.h"

enum {
    QFS_FLAG_EXTRA = 0x01,  /* first byte bit 0: 3-byte field before the size */
    CODEC_REFPACK = 0x10,
    CODEC_HUFF = 0x30,
    CODEC_HUFF_DELTA = 0x32,
    CODEC_HUFF_DELTA2 = 0x34,
    CODEC_BTREE = 0x46,
    CODEC_RLE = 0x4A
};

static int codec_of(const uint8_t *src)
{
    return src[0] & ~QFS_FLAG_EXTRA;
}

static size_t header_length(const uint8_t *src)
{
    return (src[0] & QFS_FLAG_EXTRA) ? 8 : 5;
}

static int known_codec(int codec)
{
    switch (codec) {
    case CODEC_REFPACK:
    case CODEC_HUFF:
    case CODEC_HUFF_DELTA:
    case CODEC_HUFF_DELTA2:
    case CODEC_BTREE:
    case CODEC_RLE:
        return 1;
    }
    return 0;
}

int qfs_is_compressed(const uint8_t *src, size_t src_len)
{
    /* 004a2520 accepts 0xFB or '2' as second byte, then switches on the first. */
    return src_len >= 5 && (src[1] == 0xFB || src[1] == 0x32) && known_codec(codec_of(src))
        && src_len >= header_length(src);
}

QfsResult qfs_decompressed_size(const uint8_t *src, size_t src_len, size_t *out_size)
{
    if (!qfs_is_compressed(src, src_len)) return QFS_ERR_HEADER;
    const uint8_t *p = src + header_length(src) - 3;
    *out_size = ((size_t)p[0] << 16) | ((size_t)p[1] << 8) | p[2];
    return QFS_OK;
}

QfsResult qfs_decompress(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_len)
{
    size_t expected;
    QfsResult r = qfs_decompressed_size(src, src_len, &expected);
    if (r != QFS_OK) return r;
    if (expected > dst_len) return QFS_ERR_OVERFLOW;

    const size_t start = header_length(src);
    switch (codec_of(src)) {
    case CODEC_REFPACK:
        return qfs_refpack_decode(src, src_len, start, dst, expected);
    case CODEC_HUFF:
    case CODEC_HUFF_DELTA:
    case CODEC_HUFF_DELTA2:
        return qfs_huffman_decode(src, src_len, start, dst, expected);
    case CODEC_BTREE:
        return qfs_btree_decode(src, src_len, start, dst, expected);
    default:
        return QFS_ERR_UNSUPPORTED;
    }
}

const char *qfs_result_str(QfsResult result)
{
    switch (result) {
    case QFS_OK: return "ok";
    case QFS_ERR_HEADER: return "not a QFS stream";
    case QFS_ERR_TRUNCATED: return "truncated stream";
    case QFS_ERR_OFFSET: return "back-reference before start of output";
    case QFS_ERR_OVERFLOW: return "output larger than destination";
    case QFS_ERR_SIZE: return "decoded size differs from header";
    case QFS_ERR_CORRUPT: return "corrupt stream";
    case QFS_ERR_UNSUPPORTED: return "codec not implemented";
    case QFS_ERR_MEMORY: return "out of memory";
    }
    return "unknown error";
}
