/* Huffman (codecs 0x30, 0x32, 0x34). Mirrors NFS_WIN.EXE 004a83b0.
 *
 * Bit stream (MSB first) after the size: clue byte; for code lengths 1, 2, ...
 * the symbol count of that length (getnum) until the code space is full; the
 * symbols, each as a skip over byte values not used yet; then canonical codes.
 * A decoded clue starts an escape: getnum n > 0 repeats the last byte n times,
 * n == 0 then bit 0 + 8 bits is a literal, n == 0 then bit 1 ends the stream.
 * 0x32 and 0x34 then undo one or two levels of delta coding over the output.
 * The game uses an 8-bit lookup table plus range search; decoding bit by bit
 * against the same canonical code gives identical output. */
#include "formats/qfs_codecs.h"

enum {
    MAX_CODE_LENGTH = 16,  /* the game compares codes as 16-bit left-justified values */
    CODEC_DELTA = 0x32,
    CODEC_DELTA2 = 0x34
};

typedef struct {
    const uint8_t *src;
    size_t bit_end;
    size_t bit;
} Bits;

static int read_bit(Bits *b, unsigned *out)
{
    if (b->bit >= b->bit_end) return 0;
    *out = (b->src[b->bit >> 3] >> (7 - (b->bit & 7))) & 1;
    b->bit++;
    return 1;
}

static int read_bits(Bits *b, unsigned count, uint32_t *out)
{
    uint32_t v = 0;
    for (unsigned i = 0; i < count; i++) {
        unsigned bit;
        if (!read_bit(b, &bit)) return 0;
        v = (v << 1) | bit;
    }
    *out = v;
    return 1;
}

/* EA's variable-length number: 1xx -> xx (0..3); otherwise k zeros, a one,
 * then n = k + 2 bits v -> v + 2^n - 4. */
static int read_num(Bits *b, uint32_t *out)
{
    unsigned bit;
    if (!read_bit(b, &bit)) return 0;
    if (bit) {
        uint32_t v;
        if (!read_bits(b, 2, &v)) return 0;
        *out = v;
        return 1;
    }
    unsigned n = 3;
    for (;;) {
        if (!read_bit(b, &bit)) return 0;
        if (bit) break;
        if (++n > 30) return 0;
    }
    uint32_t v;
    if (!read_bits(b, n, &v)) return 0;
    *out = v + (1u << n) - 4;
    return 1;
}

typedef struct {
    uint32_t count[MAX_CODE_LENGTH + 1];
    uint32_t first[MAX_CODE_LENGTH + 1];  /* first canonical code of each length */
    uint32_t index[MAX_CODE_LENGTH + 1];  /* symbol index of that first code */
    unsigned max_length;
    uint8_t symbols[256];
    uint32_t symbol_count;
} Code;

static QfsResult read_code(Bits *b, Code *c)
{
    uint32_t next = 0, total = 0;
    for (unsigned len = 1;; len++) {
        if (len > MAX_CODE_LENGTH) return QFS_ERR_CORRUPT;
        uint32_t count;
        if (!read_num(b, &count)) return QFS_ERR_TRUNCATED;
        c->first[len] = next * 2;
        c->index[len] = total;
        c->count[len] = count;
        next = next * 2 + count;
        total += count;
        if (next > (1u << len) || total > 256) return QFS_ERR_CORRUPT;
        c->max_length = len;
        if (count != 0 && next == (1u << len)) break;  /* code space full */
    }
    c->symbol_count = total;

    uint8_t used[256] = { 0 };
    uint8_t symbol = 0xFF;
    for (uint32_t i = 0; i < total; i++) {
        uint32_t skip;
        if (!read_num(b, &skip)) return QFS_ERR_TRUNCATED;
        for (skip += 1; skip > 0;) {
            symbol++;
            if (!used[symbol]) skip--;
        }
        used[symbol] = 1;
        c->symbols[i] = symbol;
    }
    return QFS_OK;
}

static QfsResult read_symbol(Bits *b, const Code *c, uint8_t *out)
{
    uint32_t code = 0;
    for (unsigned len = 1; len <= c->max_length; len++) {
        unsigned bit;
        if (!read_bit(b, &bit)) return QFS_ERR_TRUNCATED;
        code = (code << 1) | bit;
        if (code - c->first[len] < c->count[len]) {
            *out = c->symbols[c->index[len] + (code - c->first[len])];
            return QFS_OK;
        }
    }
    return QFS_ERR_CORRUPT;
}

/* Handles the data after a decoded clue. Sets *done on the end marker. */
static QfsResult escape(Bits *b, uint8_t *dst, size_t dst_len, size_t *out, int *done)
{
    uint32_t run;
    if (!read_num(b, &run)) return QFS_ERR_TRUNCATED;
    if (run != 0) {
        if (*out == 0) return QFS_ERR_CORRUPT;
        if (run > dst_len - *out) return QFS_ERR_OVERFLOW;
        const uint8_t last = dst[*out - 1];
        for (uint32_t i = 0; i < run; i++) dst[(*out)++] = last;
        return QFS_OK;
    }
    unsigned end;
    if (!read_bit(b, &end)) return QFS_ERR_TRUNCATED;
    if (end) {
        *done = 1;
        return QFS_OK;
    }
    uint32_t literal;
    if (!read_bits(b, 8, &literal)) return QFS_ERR_TRUNCATED;
    if (*out >= dst_len) return QFS_ERR_OVERFLOW;
    dst[(*out)++] = (uint8_t)literal;
    return QFS_OK;
}

static void undo_delta(uint8_t codec, uint8_t *dst, size_t len)
{
    uint8_t sum = 0, sum2 = 0;
    for (size_t i = 0; i < len; i++) {
        if (codec == CODEC_DELTA) {
            sum = (uint8_t)(sum + dst[i]);
            dst[i] = sum;
        } else if (codec == CODEC_DELTA2) {
            sum = (uint8_t)(sum + dst[i]);
            sum2 = (uint8_t)(sum2 + sum);
            dst[i] = sum2;
        }
    }
}

QfsResult qfs_huffman_decode(const uint8_t *src, size_t src_len, size_t data_start, uint8_t *dst, size_t dst_len)
{
    Bits b = { src, src_len * 8, data_start * 8 };
    uint32_t clue;
    if (!read_bits(&b, 8, &clue)) return QFS_ERR_TRUNCATED;

    static const Code empty;
    Code code = empty;
    QfsResult r = read_code(&b, &code);
    if (r != QFS_OK) return r;

    size_t out = 0;
    for (int done = 0; !done;) {
        uint8_t symbol;
        if ((r = read_symbol(&b, &code, &symbol)) != QFS_OK) return r;
        if (symbol == clue) {
            if ((r = escape(&b, dst, dst_len, &out, &done)) != QFS_OK) return r;
        } else {
            if (out >= dst_len) return QFS_ERR_OVERFLOW;
            dst[out++] = symbol;
        }
    }
    if (out != dst_len) return QFS_ERR_SIZE;

    /* The game filters on the full pack code (first byte without bit 0, then 0xFB). */
    if (src[1] == 0xFB) undo_delta(src[0] & 0xFE, dst, dst_len);
    return QFS_OK;
}
