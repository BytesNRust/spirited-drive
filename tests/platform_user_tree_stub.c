/* For the tests that link the game alone with their own platform_read_user_file (CMakeLists.txt names them): the player folder's tree as that
 * store, by each file's path below the folder (editions/<id>.ini, mods/<id>/mod.ini), with no folder on disk to list. The records read
 * the edition in use (race_records_merge.h), which reads its file and its cars' mod.ini this way. */
#include "game/platform/platform.h"

uint8_t *platform_user_read_file_at(const char *rel, size_t *size) { return platform_read_user_file(rel, size); }
bool platform_user_dir(char *out, size_t out_size) { (void)out, (void)out_size; return false; }
bool platform_user_is_dir(const char *rel) { (void)rel; return false; }
char **platform_list_files(const char *dir, const char *pattern, int *count) { (void)dir, (void)pattern, *count = 0; return NULL; }
void platform_free_names(char **names) { (void)names; }
