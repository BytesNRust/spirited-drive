/* The music reader on synthetic chunk files: the chunks in order, whole in
 * the ring; the ring reused once the stream frees blocks (-2), wrapping
 * before a chunk that would pass its end; full until then; a seek goes on
 * from its offset; the end of the file and malformed chunks give 0. */
#include "game/audio/music_reader.h"
#include "test.h"

#include <string.h>

enum { BASE = 0x10000000, RING = 0x40 };

static uint8_t file[0x100];
static size_t file_size;

static void put_u32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i));
}

static size_t chunk(char kind, uint32_t size, uint8_t fill)
{
    const size_t at = file_size;
    memcpy(file + at, "1SN", 3);
    file[at + 3] = (uint8_t)kind;
    put_u32(file + at + 4, size);
    memset(file + at + 8, fill, size - 8);
    file_size += size;
    return at;
}

static uint8_t ring_bytes[RING];

static MusicReader reader(void)
{
    MusicReader r;
    music_reader_open(&r, (SoundRing){ ring_bytes, BASE, RING }, file, file_size);
    return r;
}

static void test_in_order(void)
{
    file_size = 0;
    chunk('h', 0x10, 1), chunk('d', 0x10, 2), chunk('e', 8, 0);
    MusicReader r = reader();
    const uint32_t a = music_reader_next(&r), b = music_reader_next(&r), c = music_reader_next(&r);
    CHECK_EQ_INT(a, BASE);
    CHECK_EQ_INT(b, BASE + 0x10);
    CHECK_EQ_INT(c, BASE + 0x20);
    CHECK_MEM(sound_ring_at(&r.ring, b), file + 0x10, 0x10);
    CHECK_EQ_INT(music_reader_next(&r), 0); /* the file's end */
}

static void test_full_then_reused(void)
{
    file_size = 0;
    for (int i = 0; i < 6; i++) chunk('d', 0x18, (uint8_t)i);
    MusicReader r = reader();
    const uint32_t a = music_reader_next(&r), b = music_reader_next(&r);
    CHECK_EQ_INT(a, BASE);
    CHECK_EQ_INT(b, BASE + 0x18);
    CHECK_EQ_INT(music_reader_next(&r), 0); /* 0x30 + 0x18 passes the end, the start is taken */
    CHECK_EQ_INT(r.at, 0x30);              /* still to read */
    sound_ring_put(&r.ring, a, SOUND_BLOCK_FREE);
    const uint32_t c = music_reader_next(&r); /* wraps to the start */
    CHECK_EQ_INT(c, BASE);
    CHECK_EQ_INT(sound_ring_at(&r.ring, c)[8], 2);
    CHECK_EQ_INT(music_reader_next(&r), 0); /* b still queued */
    sound_ring_put(&r.ring, b, 0);         /* linked, not played: still in use */
    CHECK_EQ_INT(music_reader_next(&r), 0);
    sound_ring_put(&r.ring, b, SOUND_BLOCK_FREE);
    CHECK_EQ_INT(music_reader_next(&r), BASE + 0x18); /* past the wrap the oldest is c */
    sound_ring_put(&r.ring, c, SOUND_BLOCK_FREE);
    sound_ring_put(&r.ring, BASE + 0x18, SOUND_BLOCK_FREE);
    CHECK_EQ_INT(music_reader_next(&r), BASE); /* all free: from the start again */
}

static void test_seek(void)
{
    file_size = 0;
    chunk('h', 0x10, 1);
    const size_t loop = chunk('d', 0x10, 2);
    chunk('l', 0xc, 0), put_u32(file + file_size - 4, (uint32_t)loop);
    chunk('e', 8, 0);
    MusicReader r = reader();
    for (int i = 0; i < 3; i++) sound_ring_put(&r.ring, music_reader_next(&r), SOUND_BLOCK_FREE);
    music_reader_seek(&r, (uint32_t)loop);
    const uint32_t d = music_reader_next(&r);
    CHECK(d != 0);
    CHECK_MEM(sound_ring_at(&r.ring, d), file + loop, 0x10);
}

static void test_malformed(void)
{
    file_size = 0;
    chunk('d', 0x10, 1);
    put_u32(file + 4, 4); /* below its header */
    MusicReader r = reader();
    CHECK_EQ_INT(music_reader_next(&r), 0);
    put_u32(file + 4, 0x11); /* past the file */
    CHECK_EQ_INT(music_reader_next(&r), 0);
    put_u32(file + 4, 0x10);
    file_size = 0x0c; /* a header cut short */
    r = reader();
    CHECK_EQ_INT(music_reader_next(&r), 0);
    file_size = 0;
    chunk('d', RING + 8, 1); /* larger than the ring */
    r = reader();
    CHECK_EQ_INT(music_reader_next(&r), 0);
}

int main(void)
{
    test_in_order();
    test_full_then_reused();
    test_seek();
    test_malformed();
    return TEST_REPORT();
}
