/* Runs a harness (fuzz.h) over files and directories given on the command line: the regression cases in tests/fuzz.
 * Without libFuzzer, so every build replays them; a crash or a sanitizer report fails the ctest. */
#include "fuzz/fuzz.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int run_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return fprintf(stderr, "replay: cannot open %s\n", path), 1;
    uint8_t *data = NULL;
    size_t size = 0, cap = 0;
    for (;;) {
        if (size == cap) {
            cap = cap ? cap * 2 : 4096;
            uint8_t *grown = realloc(data, cap);
            if (grown == NULL) return fclose(f), free(data), fprintf(stderr, "replay: out of memory\n"), 1;
            data = grown;
        }
        const size_t got = fread(data + size, 1, cap - size, f);
        if (got == 0) break;
        size += got;
    }
    fclose(f);
    /* an exact-size copy, so a read one past the input shows under ASan */
    uint8_t *exact = malloc(size ? size : 1);
    if (exact == NULL) return free(data), fprintf(stderr, "replay: out of memory\n"), 1;
    if (size) memcpy(exact, data, size);
    free(data);
    LLVMFuzzerTestOneInput(exact, size);
    free(exact);
    return 0;
}

static int run(const char *path, int *count)
{
    struct stat st;
    if (stat(path, &st) != 0) return fprintf(stderr, "replay: no %s\n", path), 1;
    if (!S_ISDIR(st.st_mode)) return ++*count, run_file(path);
    DIR *d = opendir(path);
    if (d == NULL) return fprintf(stderr, "replay: cannot list %s\n", path), 1;
    int bad = 0;
    for (struct dirent *e; (e = readdir(d)) != NULL;) {
        if (e->d_name[0] == '.') continue;
        char child[4096];
        if (snprintf(child, sizeof child, "%s/%s", path, e->d_name) >= (int)sizeof child) { bad = 1; continue; }
        bad |= run(child, count);
    }
    closedir(d);
    return bad;
}

int main(int argc, char **argv)
{
    int bad = 0, count = 0;
    for (int i = 1; i < argc; i++) bad |= run(argv[i], &count);
    printf("replay: %d inputs\n", count);
    return bad;
}
