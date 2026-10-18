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

static void put_short(uint8_t *image, uint32_t at, int16_t v)
{
    image[at - 0x00524df0] = (uint8_t)v, image[at - 0x00524df0 + 1] = (uint8_t)((uint16_t)v >> 8);
}

/* the end of 0042a010 without a pad */
void game_file_controls_to_keys(uint8_t *image)
{
    /* each control word, the pair of keys it gets back, and those keys */
    static const struct { uint32_t control, keys; int16_t first, second; } RESET[3] = {
        { 0x0052ad26, 0x0052ad2c, 0x4b00, 0x4d00 },
        { 0x0052ad2a, 0x0052ad34, 0x41, 0x5a },
        { 0x0052ad28, 0x0052ad30, 0x4800, 0x5000 }
    };
    for (int i = 0; i < 3; i++) {
        const uint8_t *b = image + (RESET[i].control - 0x00524df0);
        const int16_t control = (int16_t)(uint16_t)(b[0] | b[1] << 8);
        if (control <= 0 || control >= 4) continue;
        put_short(image, RESET[i].control, 0);
        put_short(image, RESET[i].keys, RESET[i].first);
        put_short(image, RESET[i].keys + 2, RESET[i].second);
    }
}

/* 0042a010 */
int game_file_load_config(uint8_t *image, const char *dir, int keep_controls, const GameFileOps *ops)
{
    char path[GAME_FILE_PATH];
    void *data;
    int32_t flags = 0, size;
    int result = 0;
    snprintf(path, sizeof path, "%sconfig.dat", dir);
    ops->read(ops->ctx, path, &data, &flags, &size);
    if (data == NULL) {
        ops->defaults(ops->ctx);
        result = 1;
    } else if (size != CONFIG_DAT_SIZE) {
        ops->release(ops->ctx, data);
        ops->remove_file(ops->ctx, path);
        ops->defaults(ops->ctx);
        result = 1;
    } else {
        uint8_t *block = ops->alloc(ops->ctx, "config", size, 0);
        ops->copy(ops->ctx, data, block, size);
        ops->release(ops->ctx, data);
        memcpy(image, block, (size_t)size);
        ops->block_free(ops->ctx, block);
    }
    if (!keep_controls) game_file_controls_to_keys(image);
    return result;
}
