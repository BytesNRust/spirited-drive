/* The sound driver's stream queue (the music's '1SNd' blocks): the blocks a
 * reader put in its ring, linked through their first word into a queue the
 * mixer's streamed spans (00498119, 004982a5) take from (0048dda8). */
#ifndef TNFS_SOUND_STREAM_H
#define TNFS_SOUND_STREAM_H

#include <stdint.h>

/* The blocks' links are 32-bit addresses, as in the original: the ring's
 * first byte has the address `base` (never 0: 0 is no block). */
typedef struct SoundRing {
    uint8_t *bytes;
    uint32_t base;
    uint32_t size;
} SoundRing;

/* A ring address's byte, and its 32-bit word read and written */
uint8_t *sound_ring_at(const SoundRing *r, uint32_t address);
uint32_t sound_ring_get(const SoundRing *r, uint32_t address);
void sound_ring_put(const SoundRing *r, uint32_t address, uint32_t w);

/* A block's first word once queued: the next block's address, or one of */
#define SOUND_BLOCK_LAST 0u           /* the queue's tail, more may follow */
#define SOUND_BLOCK_END 0xffffffffu   /* '1SNe': the stream's end */
#define SOUND_BLOCK_FREE 0xfffffffeu  /* played: the reader may reuse it (0048bde8) */

typedef struct SoundStream {
    SoundRing ring;
    int32_t state;         /* 004c7968: 3 set up, 2 playing, 1 ended, 0 failed */
    int32_t channel;       /* 004c796c */
    int32_t frame_bytes;   /* 004c7970 */
    uint32_t rate;         /* 004c7974 */
    uint32_t volume;       /* 004c7978: written as a word, read as its low byte */
    uint32_t file;         /* 004c797c: the reader's */
    uint32_t header;       /* 004c7980: the '1SNh' block's EACS header */
    int32_t played;        /* 004c7984: blocks handed to the mixer */
    int32_t frame_scale;   /* 004c7988: 4 compressed, else 1 */
    uint32_t tail;         /* 0052eec8: the last block queued */
    int32_t handle;        /* 0052eecc: the stream's sound handle */
    uint32_t head;         /* 0052eed0: the block playing, 0 none */
    /* 0048ab4c (cdecl file, offset): asks the reader to go on from the
     * file's `offset` (a '1SNl' block's +8: the loop) */
    void (*loop)(void *ctx, uint32_t file, uint32_t offset);
    void *loop_ctx;
} SoundStream;

/* 0048dda8 (cdecl &block, &frames; the mixer's): with blocks queued past the
 * head, the next one - its data (+8) and (its size +4 - 8) / the frame's
 * bytes (signed) - becomes the head, the old head freed and one more
 * played; at the tail, a stream's end frees it and ends the stream (state
 * 1, NULL and 0); else the head again (the mixer repeats it until more
 * come). No head (0): the original reads address 0, the port gives NULL
 * and 0. */
void sound_stream_next(SoundStream *s, const uint8_t **block, int32_t *frames);

/* 0048e018: every block from the head to the tail freed (-2; the tail
 * first, then again), the head left at the tail. No head or tail: the
 * original writes address 0; the port does nothing. */
void sound_stream_drop(SoundStream *s);

/* 0048e054's units */
enum { SOUND_STREAM_BYTES, SOUND_STREAM_FRAMES, SOUND_STREAM_TIME };

/* 0048e054 (eax the unit): with the stream not failed (state != 0; else
 * -1), the bytes queued from the head - its size (+4) and each next
 * block's while that block links on (the walk stops at a block whose word
 * is 0, so the tail itself counts only as the head), or at -1 / -2 - then
 * as bytes, frames (/ 004c7970 x 004c7988) or time ((frames << 12) /
 * (rate >> 4) x 004c7988: 1/65536 s); another unit -1. No head: 0. */
int32_t sound_stream_queued(const SoundStream *s, uint32_t unit);

/* 0048dcec (eax the block's address): with the stream not failed (else
 * -1), the block queued after the tail (or as the head of an empty queue)
 * and made the tail, its word 0 unless it is -1 (the end); 0. */
int32_t sound_stream_append(SoundStream *s, uint32_t block);

/* 0048de54 (eax the reader's file): the stream set up (state 3) with no
 * channel (-1), the volume 0x7f, nothing played or queued; 0. */
int32_t sound_stream_reset(SoundStream *s, uint32_t file);

#endif
