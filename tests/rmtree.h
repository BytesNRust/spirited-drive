/* A folder and everything under it removed, the same on every system (the tests' scratch folders: `rm -rf` and `find -delete` are not there on Windows). 0 when it is gone. */
#ifndef TNFS_TEST_RMTREE_H
#define TNFS_TEST_RMTREE_H

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int test_rmtree(const char *path)
{
    struct stat st;
    if (lstat(path, &st) != 0) return 0;   /* nothing there */
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        if (d == NULL) return -1;
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
            char child[PATH_MAX + 64];
            snprintf(child, sizeof child, "%s/%s", path, e->d_name);
            test_rmtree(child);
        }
        closedir(d);
        return rmdir(path);
    }
    chmod(path, 0700);   /* a read-only file does not go on Windows */
    return unlink(path);
}

#endif
