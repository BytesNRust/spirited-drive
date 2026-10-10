/* Whether 004a2520 can unpack a RefPack stream in place. It moves the packed
 * bytes to offset unpacked + 0x400 - packed of a buffer of unpacked + 0x400
 * bytes and unpacks from the front, so the writer must never pass the reader.
 * The walk counts the stream's bytes without the header (which is stricter than
 * the game's own accounting) and the output after each opcode's literals. */
#include "formats/qfs.h"

enum { SLACK = 0x400, HEADER_LEN = 5, STOP_CODE = 0xFC };

int qfs_inplace_ok(const uint8_t *packed, size_t packed_len, size_t unpacked)
{
    size_t size = 0;
    if (!qfs_is_compressed(packed, packed_len) || (packed[0] & ~1) != 0x10) return 0;
    if (packed[0] & 1) return 0;  /* the 3-byte extra field variant is not written by us */
    if (qfs_decompressed_size(packed, packed_len, &size) != QFS_OK || size != unpacked) return 0;
    if (packed_len > unpacked + SLACK) return 0;                  /* rule 1 */

    const size_t shift = unpacked + SLACK - packed_len;
    size_t in = HEADER_LEN, out = 0;
    for (;;) {
        if (in >= packed_len) return 0;
        const uint8_t code = packed[in];
        size_t literals, copy = 0, bytes;
        if (!(code & 0x80)) { bytes = 2; literals = code & 3; copy = ((code >> 2) & 7) + 3; }
        else if (!(code & 0x40)) { bytes = 3; if (in + 1 >= packed_len) return 0; literals = packed[in + 1] >> 6; copy = (code & 0x3F) + 4; }
        else if (!(code & 0x20)) { bytes = 4; literals = code & 3; copy = ((size_t)((code >> 2) & 3) << 8) + (in + 3 < packed_len ? packed[in + 3] : 0) + 5; }
        else if (code < STOP_CODE) { bytes = 1; literals = ((size_t)(code & 0x1F) << 2) + 4; }
        else { bytes = 1; literals = code & 3; }
        if (in + bytes + literals > packed_len) return 0;
        in += bytes + literals;
        out += literals;
        if (out > (in - HEADER_LEN) + shift) return 0;            /* rule 2 */
        if (code >= STOP_CODE) break;
        out += copy;
        if (out > (in - HEADER_LEN) + shift) return 0;
    }
    return out == unpacked;
}
