/* The game state's and the replay's files (EA's file calls). */
#ifndef TNFS_GAME_FILE_H
#define TNFS_GAME_FILE_H

#include <stdint.h>

enum {
    GAME_FILE_PATH = 0x50,
    GAME_FILE_SIZE = 0x5b6  /* the game state 0052ad44 a saved game holds */
};

typedef struct GameFileOps {
    /* 00488428 (cdecl): the whole file read; *flags goes in as 0 (what it
     * says to EA's loader is not known), *data and *size come out. */
    void (*read)(void *ctx, const char *path, void **data, int32_t *flags, int32_t *size);
    void (*fatal)(void *ctx, const char *message);                    /* 00477a40 (cdecl) */
    void (*copy)(void *ctx, const void *from, void *to, int32_t n);  /* 004884d8 (cdecl): from, to, n */
    void (*release)(void *ctx, void *data);                          /* 004884c8 (cdecl): 00488428's data */
    int32_t (*has_room)(void *ctx, int32_t bytes);                   /* 0046733c: eax (disk_has_room) */
    uint8_t *(*alloc)(void *ctx, const char *tag, int32_t size, int32_t flags);  /* 00474c8c (cdecl) */
    int32_t (*block_size)(void *ctx, const uint8_t *block);          /* 0047596c (cdecl): an alloc's size */
    int32_t (*write)(void *ctx, const char *path, const uint8_t *data, int32_t size);  /* 004885b4 (cdecl) */
    void (*block_free)(void *ctx, uint8_t *block);                   /* 00475260 (cdecl) */
    void *ctx;
} GameFileOps;

/* 0042a484: the saved game `name` ("%s"; it must fit) read whole into the
 * game state; a file of another size is a fatal "BUG" first. */
void game_file_load(const char *name, uint8_t game[GAME_FILE_SIZE], const GameFileOps *ops);

/* 0042a580: with room on the disk for it, the game state copied into a
 * "SAVEGAME" block and written to path (004885b4, the block's own size);
 * returns what 004885b4 gave, or 0 without room. */
int32_t game_file_save(const char *path, const uint8_t game[GAME_FILE_SIZE], const GameFileOps *ops);

/* 0042a5e8: gamedata/replay/replay.rpl read whole and copied into a
 * "LOADREPLAY" block of its size; with room on the disk for it, written to
 * path and the block freed, returning what 004885b4 gave; without room 0,
 * and the block is not freed. */
int32_t game_file_copy_replay(const char *path, const GameFileOps *ops);

#endif
