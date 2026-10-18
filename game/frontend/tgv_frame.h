/* A TGV movie's decoder state and its frames (0048e888 .. 0048e928): 'kVGT' key frames (a palette and the
 * picture, QFS or raw) and 'fVGT' frames (4x4 blocks moved from the last picture, copied raw or from 2-bit
 * blocks) into 8-bit shapes, the 0x10-byte header 004a2b90's type 0x7b, the width and the height, then the
 * pixels. The original reads its frames unbounded; the port takes each frame's size and refuses (0, nothing
 * decoded) a frame its header says is longer, a picture over 256 colours, or a block moved out of the last
 * picture's buffer. */
#ifndef TNFS_TGV_FRAME_H
#define TNFS_TGV_FRAME_H

#include <stddef.h>
#include <stdint.h>

enum { TGV_SHAPE_HEADER = 0x10, TGV_PALETTE = 0x300 };

/* 0048e888's 0x344 bytes */
typedef struct TgvState {
    int32_t width, height, unknown_8, colors;  /* +0x00: the last key frame's u16s +8, +0xa, +0xc, +0xe */
    int32_t moves, copies, twos, code_bits;    /* +0x10: the last frame's u16s +8, +0xa, +0xc, +0xe */
    uint32_t work_moves, work_blocks;          /* +0x20, +0x24: the work buffer's two parts, in bytes */
    uint8_t *shape, *previous;                 /* +0x28, +0x2c: the picture and the one before, header first */
    int32_t *codes;                            /* +0x30: a code per 4x4 block */
    int32_t *rows;                             /* +0x34: 2 x height row offsets, (i - height) * width */
    uint8_t *raw;                              /* +0x38: the frame's raw blocks, in the work buffer */
    int32_t *work;                             /* +0x3c (+0x40 the same): the moves' offsets, then the blocks */
    uint8_t palette[TGV_PALETTE];              /* +0x44: 3 bytes a colour */
} TgvState;

TgvState *tgv_open(void);       /* 0048e888: a zeroed state; NULL when out of memory */
void tgv_close(TgvState *s);    /* 0048e928: its buffers and itself freed; NULL does nothing */

/* 0048e598 (cdecl state, frame): a key frame; 0 when the buffers could not be had (or the port refuses it) */
int tgv_key_frame(TgvState *s, const uint8_t *frame, size_t size);
/* 0048e308 (cdecl state, frame): a frame over the last picture; 0 when the work buffer could not be had */
int tgv_frame(TgvState *s, const uint8_t *frame, size_t size);
/* 0048e8b4 (cdecl state, frame): either by its tag ('kVGT' / 'fVGT' as a little-endian dword or the other way
 * round); the picture's shape after it (the last one when the tag is neither, with the original's message) */
uint8_t *tgv_decode(TgvState *s, const uint8_t *frame, size_t size);

#endif
