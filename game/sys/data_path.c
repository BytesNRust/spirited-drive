/* port: see data_path.h. */
#include "game/sys/data_path.h"

#include <stdio.h>
#include <string.h>

static char base_dir[DATA_PATH_MAX];

void data_path_set_base(const char *base)
{
    base_dir[0] = '\0';
    if (base != NULL && strlen(base) < sizeof base_dir) snprintf(base_dir, sizeof base_dir, "%s", base);
}

int data_path_candidates_for(const char *base, const char *source_dir, const char *relative, char paths[DATA_PATH_CANDIDATES][DATA_PATH_MAX])
{
    int n = 0;
    for (int i = 0; i < DATA_PATH_CANDIDATES; i++) paths[i][0] = '\0';
    if (relative == NULL || relative[0] == '\0') return 0;
    if (base != NULL && base[0] != '\0') {
        const size_t len = strlen(base);
        const char *slash = base[len - 1] == '/' ? "" : "/";
        if (snprintf(paths[0], DATA_PATH_MAX, "%s%sdata/%s", base, slash, relative) < DATA_PATH_MAX) n++;
        else paths[0][0] = '\0';
    }
    if (snprintf(paths[1], DATA_PATH_MAX, "data/%s", relative) < DATA_PATH_MAX) n++;
    else paths[1][0] = '\0';
    if (source_dir != NULL && source_dir[0] != '\0') {
        if (snprintf(paths[2], DATA_PATH_MAX, "%s/data/%s", source_dir, relative) < DATA_PATH_MAX) n++;
        else paths[2][0] = '\0';
    }
    return n;
}

int data_path_candidates(const char *relative, char paths[DATA_PATH_CANDIDATES][DATA_PATH_MAX])
{
#ifdef TNFS_SOURCE_DIR
    return data_path_candidates_for(base_dir, TNFS_SOURCE_DIR, relative, paths);
#else
    return data_path_candidates_for(base_dir, NULL, relative, paths);
#endif
}
