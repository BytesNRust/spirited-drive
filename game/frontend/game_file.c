#include "game/frontend/game_file.h"

#include <stdio.h>

/* 0042a484 */
void game_file_load(const char *name, uint8_t game[GAME_FILE_SIZE], const GameFileOps *ops)
{
    char path[GAME_FILE_PATH];
    void *data;
    int32_t flags = 0, size;
    snprintf(path, sizeof path, "%s", name);
    ops->read(ops->ctx, path, &data, &flags, &size);
    if (size != GAME_FILE_SIZE) ops->fatal(ops->ctx, "\nBUG: Loadgame, wrong file size\n");
    ops->copy(ops->ctx, data, game, size);
    ops->release(ops->ctx, data);
}
