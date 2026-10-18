#include "game/frontend/tgv_frame.h"

#include "core/formats/qfs.h"
#include "game/frontend/tgv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    AT_TAG = 0, AT_A = 8, AT_B = 0xa, AT_C = 0xc, AT_D = 0xe, AT_FLAGS = 0x10, AT_DATA = 0x14,
    KEY_RAW = 1,      /* the key frame's flags: the picture not packed */
    MOVE_BITS = 10,   /* a move's dx and dy, signed */
    TAG_KEY = 0x5447566b, TAG_KEY_SWAPPED = 0x6b564754, TAG_FRAME = 0x54475666, TAG_FRAME_SWAPPED = 0x66564754
};

static uint32_t u16_at(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }

static uint32_t u32_at(const uint8_t *p) { return u16_at(p) | u16_at(p + 2) << 16; }

/* 0048e888 */
TgvState *tgv_open(void)
{
    return calloc(1, sizeof(TgvState));
}

/* 0048e928 */
void tgv_close(TgvState *s)
{
    if (s == NULL) return;
    free(s->work), free(s->shape), free(s->previous), free(s->rows), free(s->codes);
    free(s);
}

/* The picture's bytes, w x h rounded up to a dword */
static uint32_t picture_bytes(const TgvState *s)
{
    return ((uint32_t)s->width * (uint32_t)s->height + 3) & ~3u;
}

static void shape_header(uint8_t *shape, int32_t w, int32_t h)
{
    memset(shape, 0, TGV_SHAPE_HEADER);
    shape[0] = (uint8_t)tgv_shape_type(8);
    shape[4] = (uint8_t)w, shape[5] = (uint8_t)(w >> 8), shape[6] = (uint8_t)h, shape[7] = (uint8_t)(h >> 8);
}

/* The shapes, the row offsets and the codes again for a new size: the old ones freed, fresh ones zeroed */
static void key_buffers(TgvState *s, uint32_t bytes)
{
    free(s->shape), free(s->previous), free(s->rows), free(s->codes);
    s->shape = calloc(1, (size_t)bytes + TGV_SHAPE_HEADER);
    s->previous = calloc(1, (size_t)bytes + TGV_SHAPE_HEADER);
    s->rows = calloc(2 * (size_t)s->height, sizeof *s->rows);
    s->codes = calloc(bytes / 16, sizeof *s->codes);
}

/* 0048e598 */
int tgv_key_frame(TgvState *s, const uint8_t *frame, size_t size)
{
    const uint32_t colors = size >= AT_DATA ? u16_at(frame + AT_D) : 0;
    if (size < AT_DATA || colors > TGV_PALETTE / 3 || size < AT_DATA + 3 * (size_t)colors) return 0;
    const int32_t old_w = s->width, old_h = s->height;
    s->width = (int32_t)u16_at(frame + AT_A), s->height = (int32_t)u16_at(frame + AT_B);
    s->unknown_8 = (int32_t)u16_at(frame + AT_C), s->colors = (int32_t)colors;
    memcpy(s->palette, frame + AT_DATA, 3 * (size_t)colors);
    const uint32_t bytes = picture_bytes(s);
    if (s->width != old_w || s->height != old_h) key_buffers(s, bytes);
    if (s->shape == NULL || s->previous == NULL || s->rows == NULL || s->codes == NULL) return 0;
    shape_header(s->shape, s->width, s->height);
    shape_header(s->previous, s->width, s->height);
    for (int32_t i = 0; i < 2 * s->height; i++) s->rows[i] = (int32_t)((uint32_t)(i - s->height) * (uint32_t)s->width);
    const uint8_t *data = frame + AT_DATA + 3 * (size_t)colors;
    const size_t left = size - (size_t)(data - frame);
    if (u32_at(frame + AT_FLAGS) & KEY_RAW) {
        memmove(s->shape + TGV_SHAPE_HEADER, data, left < bytes ? left : bytes);
    } else {
        const QfsResult r = qfs_decompress(data, left, s->shape + TGV_SHAPE_HEADER, bytes);
        if (r != QFS_OK) {
            fprintf(stderr, "tgvdecode: the key frame's picture does not unpack (%s)\n", qfs_result_str(r));
            return 0;
        }
    }
    return 1;
}

/* The work buffer for the frame's moves and blocks: bigger when they do not fit, the old one freed */
static int frame_work(TgvState *s)
{
    const uint32_t moves = 8 * (uint32_t)s->moves, blocks = 16 * (uint32_t)(s->copies + s->twos);
    if (s->work_moves + s->work_blocks < moves + blocks) {
        s->work_blocks = blocks, s->work_moves = moves + 8;
        free(s->work);
        s->work = NULL, s->raw = NULL;
    }
    if (s->work == NULL) s->work = malloc((size_t)s->work_moves + s->work_blocks);
    return s->work != NULL;
}

/* Whether every block the codes move stays inside the last picture's buffer, its header included. A row of blocks
 * starts 4 x cols + 3 x pitch bytes after the one before, as 004a2ad0 steps (not 4 lines when the width is not a
 * multiple of 4). */
static int moves_inside(const TgvState *s, int32_t cols, int32_t rows)
{
    const int64_t pitch = s->width, end = (int64_t)picture_bytes(s);
    for (int32_t y = 0; y < rows; y++)
        for (int32_t x = 0; x < cols; x++) {
            const int32_t code = s->codes[y * cols + x];
            if (code < 0 || code >= s->moves) continue;
            const int64_t from = y * (4 * (int64_t)cols + 3 * pitch) + 4 * x + s->work[code];
            if (from < -TGV_SHAPE_HEADER || from + 3 * pitch + 4 > end) return 0;
        }
    return 1;
}

/* 0048e308 */
int tgv_frame(TgvState *s, const uint8_t *frame, size_t size)
{
    if (size < AT_DATA) return 0;
    const int32_t w = s->width, h = s->height;
    uint8_t *const swap = s->shape;
    s->shape = s->previous, s->previous = swap;
    s->moves = (int32_t)u16_at(frame + AT_A), s->copies = (int32_t)u16_at(frame + AT_B);
    s->twos = (int32_t)u16_at(frame + AT_C), s->code_bits = (int32_t)u16_at(frame + AT_D);
    if (!frame_work(s)) return 0;
    const uint32_t move_bytes = (((uint32_t)s->moves * 2 * MOVE_BITS + 31) & ~31u) >> 3;
    const int32_t blocks = (int32_t)((uint32_t)w * (uint32_t)h) >> 4;
    const uint32_t code_bytes = (((uint32_t)s->code_bits * (uint32_t)blocks + 31) & ~31u) >> 3;
    const size_t need = AT_DATA + (s->moves != 0 ? move_bytes : 0) + 16 * (size_t)s->copies + 8 * (size_t)s->twos + code_bytes;
    if (size < need || s->shape == NULL || s->codes == NULL) return 0;
    s->raw = (uint8_t *)s->work + 8 * s->moves;
    const uint8_t *p = frame + AT_DATA;
    if (s->moves != 0) {
        tgv_unpack_fields(s->work, p, 2 * s->moves, MOVE_BITS);
        for (int32_t i = 0; i < s->moves; i++) {
            const int32_t dy = s->work[2 * i + 1];
            if (dy < -h || dy >= h) return 0;
            s->work[i] = s->rows[h + dy] + s->work[2 * i];
        }
        p += move_bytes;
    }
    if (s->copies != 0) memmove(s->raw, p, 16 * (size_t)s->copies), p += 16 * (size_t)s->copies;
    if (s->twos != 0) tgv_expand_blocks(p, s->raw + 16 * s->copies, s->twos), p += 8 * (size_t)s->twos;
    if (code_bytes != 0) tgv_unpack_codes(s->codes, p, blocks, s->code_bits);
    const int32_t cols = w >> 2, rows = h >> 2;
    if (cols < 1 || rows < 1 || !moves_inside(s, cols, rows)) return 0;
    tgv_copy_blocks(s->codes, s->previous + TGV_SHAPE_HEADER, s->shape + TGV_SHAPE_HEADER, s->moves, s->work, s->raw, w,
                    cols, rows);
    return 1;
}

/* 0048e8b4 */
uint8_t *tgv_decode(TgvState *s, const uint8_t *frame, size_t size)
{
    const uint32_t tag = size >= 4 ? u32_at(frame + AT_TAG) : 0;
    if (tag == TAG_KEY || tag == TAG_KEY_SWAPPED) tgv_key_frame(s, frame, size);
    else if (tag == TAG_FRAME || tag == TAG_FRAME_SWAPPED) tgv_frame(s, frame, size);
    else fprintf(stderr, "tgvdecode: Invalid chunk type\n");
    return s->shape;
}
