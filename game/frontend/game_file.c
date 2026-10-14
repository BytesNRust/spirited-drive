#include "game/frontend/game_file.h"

#include "game/frontend/config_dat.h"

#include <stdio.h>
#include <string.h>

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

/* 0042a580 */
int32_t game_file_save(const char *path, const uint8_t game[GAME_FILE_SIZE], const GameFileOps *ops)
{
    if (!ops->has_room(ops->ctx, GAME_FILE_SIZE)) return 0;
    uint8_t *block = ops->alloc(ops->ctx, "SAVEGAME", GAME_FILE_SIZE, 0);
    memcpy(block, game, GAME_FILE_SIZE);
    const int32_t written = ops->write(ops->ctx, path, block, ops->block_size(ops->ctx, block));
    ops->block_free(ops->ctx, block);
    return written;
}

/* 0042a5e8 */
int32_t game_file_copy_replay(const char *path, const GameFileOps *ops)
{
    void *data;
    int32_t flags = 0, size;
    ops->read(ops->ctx, "gamedata/replay/replay.rpl", &data, &flags, &size);
    uint8_t *block = ops->alloc(ops->ctx, "LOADREPLAY", size, 0);
    ops->copy(ops->ctx, data, block, size);
    ops->release(ops->ctx, data);
    if (!ops->has_room(ops->ctx, size)) return 0;
    const int32_t written = ops->write(ops->ctx, path, block, size);
    ops->block_free(ops->ctx, block);
    return written;
}

/* 0042a504 */
void game_file_save_config(const uint8_t *image, const char *dir, const GameFileOps *ops)
{
    if (!ops->has_room(ops->ctx, CONFIG_DAT_SIZE)) return;
    char path[GAME_FILE_PATH];
    snprintf(path, sizeof path, "%sconfig.dat", dir);
    uint8_t *block = ops->alloc(ops->ctx, "config", CONFIG_DAT_SIZE, 0);
    memcpy(block, image, CONFIG_DAT_SIZE);
    ops->write(ops->ctx, path, block, ops->block_size(ops->ctx, block));
    ops->block_free(ops->ctx, block);
}
