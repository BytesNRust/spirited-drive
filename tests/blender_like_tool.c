/* dep71: tests/blender_like.h as a command, for tests/car_import.sh. A test program, not a tnfs-studio command.
 *
 *   blender_like_tool [--by-material] IN.obj OUT.obj   (--by-material: the faces in material blocks first, by_material())
 *
 * As Blender's exporter does, OUT.obj's mtllib names OUT.mtl, written beside it with a newmtl for each material the faces use. */
#include "blender_like.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    const int grouped = argc == 4 && strcmp(argv[1], "--by-material") == 0;
    if (argc != 3 && !grouped) { fprintf(stderr, "usage: blender_like_tool [--by-material] IN.obj OUT.obj\n"); return 2; }
    argv += grouped;
    FILE *in = fopen(argv[1], "rb");
    if (in == NULL) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    char *text = NULL;
    size_t len = 0, cap = 0, n;
    char chunk[4096];
    while ((n = fread(chunk, 1, sizeof chunk, in)) > 0) {
        if (len + n + 1 > cap) {
            cap = (len + n + 1) * 2;
            char *grown = realloc(text, cap);
            if (grown == NULL) { free(text), fclose(in); return 1; }
            text = grown;
        }
        memcpy(text + len, chunk, n), len += n;
    }
    fclose(in);
    if (text == NULL) { fprintf(stderr, "%s is empty\n", argv[1]); return 1; }
    text[len] = '\0';
    if (grouped) {
        char *by = by_material(text);
        free(text);
        if (by == NULL) return 1;
        text = by;
    }
    char *out = blender_like(text);
    free(text);
    if (out == NULL) return 1;
    const size_t stem = strlen(argv[2]) > 4 ? strlen(argv[2]) - 4 : strlen(argv[2]);
    const char *slash = strrchr(argv[2], '/'), *base = slash != NULL ? slash + 1 : argv[2];
    char mtl[4096];
    snprintf(mtl, sizeof mtl, "%.*s.mtl", (int)stem, argv[2]);
    FILE *f = fopen(argv[2], "wb"), *m = fopen(mtl, "wb");
    int ok = f != NULL && m != NULL;
    for (char *line = out; ok && *line != '\0';) {
        char *nl = strchr(line, '\n');
        const size_t n = nl != NULL ? (size_t)(nl - line) + 1 : strlen(line);
        if (strncmp(line, "mtllib ", 7) == 0) ok = fprintf(f, "mtllib %.*s.mtl\n", (int)(strlen(base) - (strlen(argv[2]) - stem)), base) > 0;
        else ok = fwrite(line, 1, n, f) == n;
        if (ok && strncmp(line, "usemtl ", 7) == 0) ok = fprintf(m, "newmtl %.*s\nKd 0.8 0.8 0.8\n\n", (int)(n - 8), line + 7) > 0;
        line += n;
    }
    if (f != NULL && fclose(f) != 0) ok = 0;
    if (m != NULL && fclose(m) != 0) ok = 0;
    free(out);
    if (!ok) fprintf(stderr, "cannot write %s or %s\n", argv[2], mtl);
    return ok ? 0 : 1;
}
