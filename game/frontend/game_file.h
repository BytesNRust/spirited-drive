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
    void *ctx;
} GameFileOps;

/* 0042a484: the saved game `name` ("%s"; it must fit) read whole into the
 * game state; a file of another size is a fatal "BUG" first. */
void game_file_load(const char *name, uint8_t game[GAME_FILE_SIZE], const GameFileOps *ops);

#endif
