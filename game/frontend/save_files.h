/* The saved games' and replays' files in the player's directory (glue, no oracle): EA's file calls as game_file.c
 * and game_list.c take them. */
#ifndef TNFS_SAVE_FILES_H
#define TNFS_SAVE_FILES_H

#include "game/frontend/game_file.h"

#include <stddef.h>
#include <stdint.h>

/* 00488428 .. 0048855b on the player's directory: the paths are plain names there (no directories), so the saves'
 * and replays' directories are "" */
extern const GameFileOps save_files_ops;

/* 00489084 / 00489020: the player's files matching pattern (*.SAV, *.RPL), in order, NULL after the last; the
 * replays then the CD's (gamedata/replay/: REPLAY.RPL .. REPLAY3.RPL, which the original's replay directory
 * holds beside the player's) that the player's directory has no file of the same name for */
const char *save_files_first(const char *pattern);
const char *save_files_next(void);

/* A replay's file by its name (".RPL" added when it has no extension): the player's, else the CD's; NULL none.
 * Freed with platform_free_file. */
uint8_t *save_files_read_replay(const char *name, size_t *size);

int32_t save_files_exists(const char *name);  /* 004886e0 */
void save_files_remove(const char *name);     /* 0048855b */

#endif
