#include "game/frontend/save_files.h"

#include "game/platform/platform.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 00474c8c's blocks: their size kept before them for 0047596c */
typedef struct Block {
    int32_t size;
    uint8_t data[];
} Block;

static void file_read(void *ctx, const char *path, void **data, int32_t *flags, int32_t *size)
{
    (void)ctx, (void)flags;
    size_t n = 0;
    *data = platform_read_user_file(path, &n);
    *size = *data != NULL ? (int32_t)n : 0;
}
static void fatal(void *ctx, const char *message)
{
    (void)ctx;
    fprintf(stderr, "%s\n", message);
    exit(1);
}
static void copy(void *ctx, const void *from, void *to, int32_t n) { (void)ctx; memcpy(to, from, (size_t)n); }
static void release(void *ctx, void *data) { (void)ctx; platform_free_file(data); }
static int32_t has_room(void *ctx, int32_t bytes) { (void)ctx, (void)bytes; return 1; }
static uint8_t *alloc(void *ctx, const char *tag, int32_t size, int32_t flags)
{
    (void)ctx, (void)tag, (void)flags;
    Block *const b = malloc(sizeof *b + (size_t)size);
    if (b == NULL) fatal(ctx, "save_files: out of memory");
    b->size = size;
    return b->data;
}
static Block *block_of(const uint8_t *data) { return (Block *)(void *)(data - offsetof(Block, data)); }
static int32_t block_size(void *ctx, const uint8_t *block) { (void)ctx; return block_of(block)->size; }
static int32_t file_write(void *ctx, const char *path, const uint8_t *data, int32_t size)
{
    (void)ctx;
    return platform_write_user_file(path, data, (size_t)size) ? size : 0;
}
static void block_free(void *ctx, uint8_t *block) { (void)ctx; free(block_of(block)); }
static void no_defaults(void *ctx) { (void)ctx; }
static void remove_file(void *ctx, const char *path) { (void)ctx; save_files_remove(path); }

const GameFileOps save_files_ops = { file_read, fatal, copy, release, has_room, alloc, block_size, file_write,
                                     block_free, no_defaults, remove_file, NULL };

static char **found;
static int found_count, found_next;

const char *save_files_first(const char *pattern)
{
    platform_free_names(found);
    found = platform_list_user_files(pattern, &found_count);
    found_next = 0;
    return save_files_next();
}

const char *save_files_next(void)
{
    return found_next < found_count ? found[found_next++] : NULL;
}

int32_t save_files_exists(const char *name) { return platform_user_file_exists(name); }

void save_files_remove(const char *name)
{
    if (!platform_remove_user_file(name)) fprintf(stderr, "save_files: cannot remove %s\n", name);
}
