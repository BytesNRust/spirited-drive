/* port: see data_path.h. */
#include "game/sys/data_path.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static char base_dir[DATA_PATH_MAX];
static char mod_dir[DATA_PATH_MAX];

void data_path_set_base(const char *base)
{
    base_dir[0] = '\0';
    if (base != NULL && strlen(base) < sizeof base_dir) snprintf(base_dir, sizeof base_dir, "%s", base);
}

void data_path_set_mod(const char *mod)
{
    mod_dir[0] = '\0';
    if (mod != NULL && strlen(mod) < sizeof mod_dir) snprintf(mod_dir, sizeof mod_dir, "%s", mod);
}

static int join(char *out, const char *dir, const char *middle, const char *relative)
{
    const size_t len = strlen(dir);
    const char *slash = dir[len - 1] == '/' ? "" : "/";
    if (snprintf(out, DATA_PATH_MAX, "%s%s%s%s", dir, slash, middle, relative) < DATA_PATH_MAX) return 1;
    out[0] = '\0';
    return 0;
}

int data_path_candidates_for(const char *mod, const char *base, const char *source_dir, const char *relative, char paths[DATA_PATH_CANDIDATES][DATA_PATH_MAX])
{
    int n = 0;
    for (int i = 0; i < DATA_PATH_CANDIDATES; i++) paths[i][0] = '\0';
    if (relative == NULL || relative[0] == '\0') return 0;
    if (mod != NULL && mod[0] != '\0') n += join(paths[0], mod, "", relative);
    if (base != NULL && base[0] != '\0') n += join(paths[1], base, "data/", relative);
    if (snprintf(paths[2], DATA_PATH_MAX, "data/%s", relative) < DATA_PATH_MAX) n++;
    else paths[2][0] = '\0';
    if (source_dir != NULL && source_dir[0] != '\0') n += join(paths[3], source_dir, "data/", relative);
    return n;
}

int data_path_candidates(const char *relative, char paths[DATA_PATH_CANDIDATES][DATA_PATH_MAX])
{
#ifdef TNFS_SOURCE_DIR
    return data_path_candidates_for(mod_dir, base_dir, TNFS_SOURCE_DIR, relative, paths);
#else
    return data_path_candidates_for(mod_dir, base_dir, NULL, relative, paths);
#endif
}

int data_path_exists(const char *path)
{
    struct stat st;
    return path != NULL && path[0] != '\0' && stat(path, &st) == 0 && (st.st_mode & S_IFMT) == S_IFREG;
}
