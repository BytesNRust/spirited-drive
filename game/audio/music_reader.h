/* The music's file reader, the port's own: in the original a thread
 * (0048ad40, created by 0048a4d4 through 0048a674) reads the file into a
 * ring ahead of the game, and 0048bcfc hands the game its chunks one by one
 * ('1SNh', '1SNd', '1SNl', '1SNe': a tag, the size with its 8-byte header,
 * the data), each whole in the ring. The stream queue links and frees them
 * there (sound_stream.h: the first word a link, -2 when played), and the
 * reader reuses the ring from the oldest block on once it is freed
 * (0048bc28). A seek (0048ab4c, from a '1SNl' block: the loop) drops what
 * was read past the last chunk handed out (the thread's state 2 starts its
 * writing at the read position) and goes on from the offset.
 *
 * This reader reads one chunk when asked for one, so nothing is read ahead
 * and a seek only moves the file position: the game sees the same chunks in
 * the same order, without the thread's timing. */
#ifndef TNFS_MUSIC_READER_H
#define TNFS_MUSIC_READER_H

#include "game/audio/sound_stream.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    SoundRing ring;        /* shared with the stream queue (SoundStream.ring) */
    const uint8_t *file;
    size_t size;
    size_t at;             /* the next chunk's offset */
    uint32_t oldest;       /* the oldest block not yet reused */
    uint32_t write;        /* where the next block goes */
    uint32_t wrapped_at;   /* the end of the blocks before the ring's start, 0 none */
    uint32_t blocks;       /* blocks in the ring */
} MusicReader;

/* The reader over `file` (kept by the caller), from offset 0, its ring empty */
void music_reader_open(MusicReader *r, SoundRing ring, const uint8_t *file, size_t size);

/* 0048bcfc's part: the next chunk copied whole into the ring, its address;
 * 0 when the file has no more (or a chunk is malformed: a size below 8 or
 * past the file's end) or the ring has no room yet (the stream frees blocks
 * as it plays them). */
uint32_t music_reader_next(MusicReader *r);

/* 0048ab4c (the loop): the next chunk read from `offset` */
void music_reader_seek(MusicReader *r, uint32_t offset);

#endif
