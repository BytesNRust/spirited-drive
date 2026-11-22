/* dep71: an OBJ rewritten the way the 3D editor's exporter writes one it imported, for the tests (the 3D editor is not run: the real one is checked
 * by hand, notes/dep71-edited-roundtrip-plan.md). What changes: its comments become the 3D editor's, the object is renamed `NAME.001`, the
 * numbers are printed %.6f, a normal is added and every corner names it, `s 0` before the faces, and the texture coordinates are
 * merged (one vt for equal values) and put in another order (sorted by their text). What it keeps: the vertices, the faces and their
 * corners in order, the materials. by_material() before it gives the faces in material blocks, as the 3D editor may write them. */
#ifndef TNFS_TESTS_MODELLER_SIM_H
#define TNFS_TESTS_MODELLER_SIM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t len, cap;
} BlText;

static void bl_put(BlText *t, const char *s)
{
    const size_t n = strlen(s);
    if (t->len + n + 1 > t->cap) {
        t->cap = (t->len + n + 1) * 2;
        t->data = realloc(t->data, t->cap);
    }
    memcpy(t->data + t->len, s, n + 1);
    t->len += n;
}

static int bl_by_text(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

/* `obj` with its faces grouped by material, the materials in the order they first appear (each block under one usemtl), each block
 * keeping its faces' order: the 3D editor's exporter writing a mesh's faces by material slot, as it may (malloc'd) */
static char *by_material(const char *obj)
{
    char *copy = strdup(obj), **lines = calloc(strlen(obj) + 2, sizeof *lines), **mats = calloc(strlen(obj) + 2, sizeof *mats);
    const char **face_mat = calloc(strlen(obj) + 2, sizeof *face_mat);
    size_t n = 0, n_mats = 0;
    const char *mat = "";
    BlText out = { 0 };
    for (char *l = strtok(copy, "\n"); l != NULL; l = strtok(NULL, "\n")) {
        if (strncmp(l, "usemtl ", 7) == 0) {
            mat = l;
            size_t m = 0;
            while (m < n_mats && strcmp(mats[m], l) != 0) m++;
            if (m == n_mats) mats[n_mats++] = l;
        } else if ((l[0] == 'f' || l[0] == 'l') && l[1] == ' ') face_mat[n] = mat, lines[n++] = l;
        else bl_put(&out, l), bl_put(&out, "\n");
    }
    for (size_t m = 0; m < n_mats; m++) {
        bl_put(&out, mats[m]), bl_put(&out, "\n");
        for (size_t i = 0; i < n; i++)
            if (strcmp(face_mat[i], mats[m]) == 0) bl_put(&out, lines[i]), bl_put(&out, "\n");
    }
    for (size_t i = 0; i < n; i++)
        if (face_mat[i][0] == '\0') bl_put(&out, lines[i]), bl_put(&out, "\n");
    free(lines), free(mats), free(face_mat), free(copy);
    return out.data;
}

/* the rewrite of `obj` (malloc'd), or NULL out of memory */
static char *modeller_sim(const char *obj)
{
    char *copy = strdup(obj), **vt = calloc(strlen(obj) / 4 + 1, sizeof *vt), **sorted = NULL;
    size_t n_vt = 0;
    BlText out = { 0 };
    char line[512];
    for (char *l = strtok(copy, "\n"); l != NULL; l = strtok(NULL, "\n")) {   /* the uvs, printed as the 3D editor would */
        double u, v;
        if (sscanf(l, "vt %lf %lf", &u, &v) == 2) {
            snprintf(line, sizeof line, "vt %.6f %.6f", u, v);
            vt[n_vt++] = strdup(line);
        }
    }
    sorted = malloc((n_vt ? n_vt : 1) * sizeof *sorted);
    memcpy(sorted, vt, n_vt * sizeof *vt);
    qsort(sorted, n_vt, sizeof *sorted, bl_by_text);
    size_t unique = 0;
    for (size_t i = 0; i < n_vt; i++)
        if (unique == 0 || strcmp(sorted[unique - 1], sorted[i]) != 0) sorted[unique++] = sorted[i];
    free(copy);
    copy = strdup(obj);
    bl_put(&out, "# Blender 4.2.0\n# www.blender.org\n");
    int normals = 0, smooth = 0;
    for (char *l = strtok(copy, "\n"); l != NULL; l = strtok(NULL, "\n")) {
        double x, y, z;
        if (l[0] == '#' || strncmp(l, "vt ", 3) == 0) continue;
        if (strncmp(l, "o ", 2) == 0) snprintf(line, sizeof line, "%s.001\n", l);
        else if (sscanf(l, "v %lf %lf %lf", &x, &y, &z) == 3) snprintf(line, sizeof line, "v %.6f %.6f %.6f\n", x, y, z);
        else if ((l[0] == 'f' || l[0] == 'l') && l[1] == ' ') {
            if (!normals) {
                for (size_t i = 0; i < unique; i++) bl_put(&out, sorted[i]), bl_put(&out, "\n");
                bl_put(&out, "vn -0.0000 1.0000 -0.0000\n");
                normals = 1;
            }
            if (!smooth) bl_put(&out, "s 0\n"), smooth = 1;
            snprintf(line, sizeof line, "%c", l[0]);
            for (char *c = l + 1; *c != '\0';) {
                while (*c == ' ') c++;
                if (*c == '\0') break;
                const int v = (int)strtol(c, &c, 10);
                char corner[64];
                if (*c == '/') {
                    const int t = (int)strtol(c + 1, &c, 10);
                    size_t at = 0;
                    while (at < unique && strcmp(sorted[at], vt[t - 1]) != 0) at++;
                    snprintf(corner, sizeof corner, " %d/%zu/1", v, at + 1);
                } else snprintf(corner, sizeof corner, " %d//1", v);
                strncat(line, corner, sizeof line - strlen(line) - 1);
            }
            strncat(line, "\n", sizeof line - strlen(line) - 1);
        } else snprintf(line, sizeof line, "%s\n", l);
        bl_put(&out, line);
    }
    for (size_t i = 0; i < n_vt; i++) free(vt[i]);
    free(vt), free(sorted), free(copy);
    return out.data;
}

#endif
