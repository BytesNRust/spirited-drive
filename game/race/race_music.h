/* The race's music: the stream fed from its reader (0048bcfc) into the
 * sound driver (0048df3c) - 004640d0 fills it ahead, 00464240 feeds it a
 * block a frame and starts its voice once enough is queued. */
#ifndef TNFS_RACE_MUSIC_H
#define TNFS_RACE_MUSIC_H

#include <stdint.h>

typedef struct {
    int16_t setting;          /* 00524e63: 0 no music */
    int16_t volume;           /* 00524e65 */
    int16_t unknown_52b31e;   /* 0052b31e: set, no music at all */
    int16_t playback;         /* 0052b312 */
    int16_t sound_on;         /* 0052b304: the sound device opened */
    int16_t small;            /* 004c5cca: the small ring (0x19000; else 0x7d000) */
    int16_t open;             /* 004c5cc8: a track open (00464428) */
    int16_t started;          /* 004c5cc6: its voice started */
    int32_t handle;           /* 0052480c: the voice's handle */
    uint32_t block;           /* 0052481c: the reader's last block */
    uint32_t reader;          /* 00524830 */
} RaceMusic;

typedef struct {
    void *ctx;
    void (*service)(void *ctx);                                       /* 00475c14(0) */
    int32_t (*ticks)(void *ctx);                                      /* 00473334: the ticks since its last call */
    uint32_t (*next)(void *ctx, uint32_t reader);                     /* 0048bcfc */
    int32_t (*queued)(void *ctx);                                     /* 0048e054(0): the bytes queued */
    void (*block)(void *ctx, uint32_t block);                         /* 0048df3c */
    int32_t (*start)(void *ctx);                                      /* 0048dedc */
    void (*set_volume)(void *ctx, int32_t handle, int32_t volume);    /* 00473b4c */
    void (*ramp)(void *ctx, int32_t handle, int32_t steps, int32_t volume);  /* 00473b00 */
    void (*stop)(void *ctx, int32_t handle);                          /* 00473c68 */
    int32_t (*stopped)(void *ctx, int32_t handle);                    /* 00473d20 */
    void (*close)(void *ctx, uint32_t reader);                        /* 0048a92c */
    void (*settle)(void *ctx);                                        /* 004770ac */
    void (*path)(void *ctx, const char *name);                        /* 00473d43(00524798, "%s%s", 005242d8, name) */
    uint32_t (*open)(void *ctx, int32_t ring);                        /* 0048a674(ring, 0x800, 0x20) */
    void (*request)(void *ctx, uint32_t reader);                      /* 0048aa78(reader, 00524798) */
    void (*reset)(void *ctx, uint32_t reader);                        /* 0048de54 */
} RaceMusicStreamOps;

/* The bytes queued before the voice starts (and that 004640d0 fills to);
 * the reader's ring */
enum { RACE_MUSIC_AHEAD = 0x64000, RACE_MUSIC_AHEAD_SMALL = 0x14000, RACE_MUSIC_RING = 0x7d000, RACE_MUSIC_RING_SMALL = 0x19000 };

/* 00464240 (watcall (), each frame while the music setting is on, and
 * while paused): nothing while 0052b31e; while playing back, 00475c14(0)
 * first. With the setting on, the sound open and a track open: the next
 * block (0048bcfc, into 0052481c); the voice not started and more than the
 * ahead bytes queued (0048e054(0); 0x64000, the small ring 0x14000): it
 * started (0048dedc, its handle 0052480c) at the volume (00473b4c, the word
 * 00524e65), started set; then the block, when there is one, to the stream
 * (0048df3c). */
void race_music_pump(const RaceMusicStreamOps *ops, RaceMusic *m);

/* 004640d0 (watcall (), from 35 places before long work): nothing while
 * 0052b31e or with the setting off; else 00475c14(0), and with the sound
 * and a track open, 00473334 (its count reset) and, while fewer than the
 * ahead bytes are queued: the ticks since summed, the next block (into
 * 0052481c) to the stream when there is one, the bytes queued again - until
 * the ahead bytes are queued, or the ticks summed (as a short) pass 100
 * with none queued. */
void race_music_fill(const RaceMusicStreamOps *ops, RaceMusic *m);

/* 00464568 (watcall ()): with 0052b31e clear, the setting on, the sound
 * and a track open: 00475c14(0) until the voice has stopped (00473d20) or
 * was never started, then the reader closed (0048a92c), open and started
 * cleared. */
void race_music_stop(const RaceMusicStreamOps *ops, RaceMusic *m);

/* 00464428 (watcall (name), from 00456244 and the menus): with 0052b31e
 * clear, the setting and the sound on: a track open has its voice stopped
 * (00473c68) and is closed (00464568); 004770ac; the path made (00473d43
 * into 00524798: 005242d8's folder and the name), a reader for it (0048a674,
 * the ring 0x7d000 or the small 0x19000, into 00524830) asked to read it
 * (0048aa78), the stream reset to it (0048de54), open set. */
void race_music_play(const RaceMusicStreamOps *ops, RaceMusic *m, const char *name);

/* 00464604 (watcall (volume), the menus' music volume): with 0052b31e
 * clear, the setting and the sound on, the voice's volume ramped to it over
 * 50 ticks (00473b00); 0046463c at once (steps 0: one tick). */
void race_music_volume(const RaceMusicStreamOps *ops, const RaceMusic *m, int16_t volume);
void race_music_volume_now(const RaceMusicStreamOps *ops, const RaceMusic *m, int16_t volume);

#endif
