/* RefPack LZ77 (codec 0x10). Mirrors NFS_WIN.EXE 004a823c, which copies in
 * dwords where it can; byte-wise copies here produce identical output. */
#include "formats/qfs_codecs.h"

enum { STOP_CODE = 0xFC };  /* opcodes 0xFC..0xFF end the stream */

typedef struct {
    const uint8_t *src;
    size_t src_len;
    size_t in;
    uint8_t *dst;
    size_t dst_len;
    size_t out;
} Decoder;

static QfsResult need(const Decoder *d, size_t bytes)
{
    return bytes <= d->src_len - d->in ? QFS_OK : QFS_ERR_TRUNCATED;
}

static QfsResult copy_literals(Decoder *d, size_t count)
{
    if (count > d->src_len - d->in) return QFS_ERR_TRUNCATED;
    if (count > d->dst_len - d->out) return QFS_ERR_OVERFLOW;
    for (size_t i = 0; i < count; i++) d->dst[d->out++] = d->src[d->in++];
    return QFS_OK;
}

/* Byte by byte on purpose: overlapping copies (offset < length) repeat data. */
static QfsResult copy_back(Decoder *d, size_t offset, size_t length)
{
    if (offset > d->out) return QFS_ERR_OFFSET;
    if (length > d->dst_len - d->out) return QFS_ERR_OVERFLOW;
    for (size_t i = 0; i < length; i++, d->out++) d->dst[d->out] = d->dst[d->out - offset];
    return QFS_OK;
}

/* Decodes one opcode. Sets *done on the stop code. */
static QfsResult step(Decoder *d, int *done)
{
    QfsResult r = need(d, 1);
    if (r != QFS_OK) return r;
    const uint8_t code = d->src[d->in];
    size_t literals, length, offset;

    if (!(code & 0x80)) {
        if ((r = need(d, 2)) != QFS_OK) return r;
        const uint8_t b1 = d->src[d->in + 1];
        d->in += 2;
        literals = code & 3;
        length = ((code >> 2) & 7) + 3;
        offset = ((size_t)(code >> 5) << 8) + b1 + 1;
    } else if (!(code & 0x40)) {
        if ((r = need(d, 3)) != QFS_OK) return r;
        const uint8_t b1 = d->src[d->in + 1], b2 = d->src[d->in + 2];
        d->in += 3;
        literals = b1 >> 6;
        length = (code & 0x3F) + 4;
        offset = ((size_t)(b1 & 0x3F) << 8) + b2 + 1;
    } else if (!(code & 0x20)) {
        if ((r = need(d, 4)) != QFS_OK) return r;
        const uint8_t b1 = d->src[d->in + 1], b2 = d->src[d->in + 2], b3 = d->src[d->in + 3];
        d->in += 4;
        literals = code & 3;
        length = ((size_t)((code >> 2) & 3) << 8) + b3 + 5;
        offset = ((size_t)(code & 0x10) << 12) + ((size_t)b1 << 8) + b2 + 1;
    } else {
        d->in += 1;
        if (code >= STOP_CODE) {
            *done = 1;
            return copy_literals(d, code & 3);
        }
        return copy_literals(d, ((size_t)(code & 0x1F) << 2) + 4);
    }

    if ((r = copy_literals(d, literals)) != QFS_OK) return r;
    return copy_back(d, offset, length);
}

QfsResult qfs_refpack_decode_at(const uint8_t *src, size_t src_len, size_t data_start, uint8_t *dst, size_t dst_len, size_t *at)
{
    Decoder d = { src, src_len, data_start, dst, dst_len, 0 };
    int done = 0;
    while (!done) {
        QfsResult r = step(&d, &done);
        if (r != QFS_OK) {
            if (at) *at = d.in;
            return r;
        }
    }
    if (at) *at = d.in;
    return d.out == dst_len ? QFS_OK : QFS_ERR_SIZE;
}

QfsResult qfs_refpack_decode(const uint8_t *src, size_t src_len, size_t data_start, uint8_t *dst, size_t dst_len)
{
    return qfs_refpack_decode_at(src, src_len, data_start, dst, dst_len, NULL);
}
