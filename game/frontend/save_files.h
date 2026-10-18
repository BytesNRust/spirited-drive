/* The saved games' and replays' files in the player's directory (glue, no oracle): EA's file calls as game_file.c
 * and game_list.c take them. */
#ifndef TNFS_SAVE_FILES_H
#define TNFS_SAVE_FILES_H

#include "game/frontend/game_file.h"

/* 00488428 .. 0048855b on the player's directory: the paths are plain names there (no directories), so the saves'
 * and replays' directories are "" */
extern const GameFileOps save_files_ops;

/* 00489084 / 00489020: the player's files matching pattern (*.SAV, *.RPL), in order, NULL after the last */
const char *save_files_first(const char *pattern);
const char *save_files_next(void);

int32_t save_files_exists(const char *name);  /* 004886e0 */
void save_files_remove(const char *name);     /* 0048855b */

#endif
