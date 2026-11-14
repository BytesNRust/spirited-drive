/* The game's file calls for the harnesses of tnfs_game's readers: no file is there and nothing is written, so a reader runs on the
 * fuzzer's bytes only */
#include "game/platform/platform.h"

#include <stdlib.h>

uint8_t *platform_read_file(const char *path, size_t *size) { (void)path, (void)size; return NULL; }
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size) { (void)name, (void)data, (void)size; return false; }
void platform_free_file(uint8_t *data) { free(data); }
