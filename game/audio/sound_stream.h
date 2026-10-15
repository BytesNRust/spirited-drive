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
    uint8_t volume;        /* 004c7978 */
    uint32_t file;         /* 004c797c: the reader's */
    uint32_t header;       /* 004c7980: the '1SNh' block's EACS header */
    int32_t played;        /* 004c7984: blocks handed to the mixer */
    int32_t frame_scale;   /* 004c7988: 4 compressed, else 1 */
    uint32_t tail;         /* 0052eec8: the last block queued */
    int32_t handle;        /* 0052eecc: the stream's sound handle */
    uint32_t head;         /* 0052eed0: the block playing, 0 none */
} SoundStream;

/* 0048dda8 (cdecl &block, &frames; the mixer's): with blocks queued past the
 * head, the next one - its data (+8) and (its size +4 - 8) / the frame's
 * bytes (signed) - becomes the head, the old head freed and one more
 * played; at the tail, a stream's end frees it and ends the stream (state
 * 1, NULL and 0); else the head again (the mixer repeats it until more
 * come). No head (0): the original reads address 0, the port gives NULL
 * and 0. */
void sound_stream_next(SoundStream *s, const uint8_t **block, int32_t *frames);

#endif
