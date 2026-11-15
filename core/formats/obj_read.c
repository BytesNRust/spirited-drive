/* See obj_read.h. */
#include "formats/obj_read.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { LINE_SIZE = 4096 };

typedef struct {
    ObjModel *m;
    uint32_t cap_v, cap_uv, cap_c, cap_cu, cap_f, cap_mat, cap_obj;
    int32_t material;
    uint32_t line;
    const char *file;   /* "line" or "MTL line", for messages */
    ExchangeError *err;
} Reader;

static int fail(Reader *r, const char *what)
{
    exchange_fail(r->err, "%s %u: %s", r->file, r->line, what);
    return 0;
}

/* room for `need` items of `size` in *p */
static int grow(Reader *r, void **p, uint32_t *cap, uint32_t need, size_t size)
{
    if (need <= *cap) return 1;
    const uint32_t next = *cap ? (*cap * 2 > need ? *cap * 2 : need) : (need > 16 ? need : 16);
    void *q = realloc(*p, (size_t)next * size);
    if (q == NULL) return fail(r, "out of memory");
    *p = q, *cap = next;
    return 1;
}

static char *trim(char *s)
{
    while (isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    return s;
}

/* `count` numbers (and up to `extra` more, ignored) from `s` */
static int numbers(Reader *r, char *s, double *out, int count, int extra)
{
    for (int i = 0; i < count + extra; i++) {
        while (isspace((unsigned char)*s)) s++;
        if (*s == '\0' && i >= count) return 1;
        char *end;
        errno = 0;
        const double v = strtod(s, &end);
        if (end == s || errno == ERANGE || !isfinite(v) || (*end != '\0' && !isspace((unsigned char)*end)))
            return fail(r, i < count ? "a coordinate is missing or not a number" : "a coordinate is not a number");
        if (i < count) out[i] = v;
        s = end;
    }
    while (isspace((unsigned char)*s)) s++;
    return *s == '\0' ? 1 : fail(r, "more coordinates than a vertex has");
}

static int copy_name(Reader *r, char *dst, size_t cap, const char *src, const char *what)
{
    if (strlen(src) >= cap) {
        char m[96];
        snprintf(m, sizeof m, "%s is longer than %zu characters", what, cap - 1);
        return fail(r, m);
    }
    memcpy(dst, src, strlen(src) + 1);
    return 1;
}

static int add_object(Reader *r, const char *name)
{
    ObjModel *m = r->m;
    if (!grow(r, (void **)&m->objects, &r->cap_obj, m->object_count + 1, sizeof *m->objects)) return 0;
    ObjObject *o = &m->objects[m->object_count];
    memset(o, 0, sizeof *o);
    if (!copy_name(r, o->name, sizeof o->name, name, "an object's name")) return 0;
    o->first_vertex = m->vertex_count;
    m->object_count++;
    return 1;
}

static int find_material(const ObjModel *m, const char *name)
{
    for (uint32_t i = 0; i < m->material_count; i++)
        if (strcmp(m->materials[i].name, name) == 0) return (int)i;
    return -1;
}

static int add_material(Reader *r, const char *name)
{
    ObjModel *m = r->m;
    if (!grow(r, (void **)&m->materials, &r->cap_mat, m->material_count + 1, sizeof *m->materials)) return -1;
    ObjMaterial *x = &m->materials[m->material_count];
    memset(x, 0, sizeof *x);
    if (!copy_name(r, x->name, sizeof x->name, name, "a material's name")) return -1;
    return (int)m->material_count++;
}

/* an OBJ index (1-based, or negative from the last of `count`) as 0-based */
static int index_of(Reader *r, const char *text, uint32_t count, const char *kind, uint32_t *out)
{
    char *end;
    errno = 0;
    const long v = strtol(text, &end, 10);
    char m[128];
    if (end == text || errno == ERANGE || (*end != '\0' && *end != '/')) {
        snprintf(m, sizeof m, "a corner's %s index is not a number", kind);
        return fail(r, m);
    }
    const long at = v > 0 ? v - 1 : (long)count + v;
    if (v == 0 || at < 0 || at >= (long)count) {
        snprintf(m, sizeof m, "%s index %ld is outside the %u read so far", kind, v, count);
        return fail(r, m);
    }
    *out = (uint32_t)at;
    return 1;
}

/* one corner: v, v/vt, v//vn or v/vt/vn */
static int corner(Reader *r, char *token)
{
    ObjModel *m = r->m;
    if (!grow(r, (void **)&m->corner_vertex, &r->cap_c, m->corner_count + 1, sizeof *m->corner_vertex) ||
        !grow(r, (void **)&m->corner_uv, &r->cap_cu, m->corner_count + 1, sizeof *m->corner_uv))
        return 0;
    uint32_t v, t;
    if (!index_of(r, token, m->vertex_count, "vertex", &v)) return 0;
    int32_t u = -1;
    const char *slash = strchr(token, '/');
    if (slash != NULL && slash[1] != '/' && slash[1] != '\0') {
        if (!index_of(r, slash + 1, m->uv_count, "uv", &t)) return 0;
        u = (int32_t)t;
    }
    m->corner_vertex[m->corner_count] = v, m->corner_uv[m->corner_count] = u;
    m->corner_count++;
    return 1;
}

static int face(Reader *r, char *rest)
{
    ObjModel *m = r->m;
    const uint32_t first = m->corner_count;
    for (char *save = NULL, *tok = strtok_r(rest, " \t", &save); tok != NULL; tok = strtok_r(NULL, " \t", &save))
        if (!corner(r, tok)) return 0;
    if (m->corner_count - first < 3) return fail(r, "a face has fewer than 3 corners");
    if (!grow(r, (void **)&m->faces, &r->cap_f, m->face_count + 1, sizeof *m->faces)) return 0;
    m->faces[m->face_count++] = (ObjFace){ first, m->corner_count - first, r->material, m->object_count - 1, r->line };
    m->objects[m->object_count - 1].face_count++;
    return 1;
}

static int vertex(Reader *r, char *rest)
{
    ObjModel *m = r->m;
    double c[3];
    if (!numbers(r, rest, c, 3, 1)) return 0;   /* x y z [w] */
    if (!grow(r, (void **)&m->vertices, &r->cap_v, m->vertex_count + 1, sizeof *m->vertices)) return 0;
    m->vertices[m->vertex_count++] = (ObjVec3){ c[0], c[1], c[2] };
    m->objects[m->object_count - 1].vertex_count++;
    return 1;
}

static int texture_vertex(Reader *r, char *rest)
{
    ObjModel *m = r->m;
    double c[2];
    if (!numbers(r, rest, c, 2, 1)) return 0;   /* u v [w] */
    if (!grow(r, (void **)&m->uvs, &r->cap_uv, m->uv_count + 1, sizeof *m->uvs)) return 0;
    m->uvs[m->uv_count++] = (ObjVec2){ c[0], c[1] };
    return 1;
}

/* the keyword of `line` in `key`, its rest returned */
static char *keyword(char *line, char key[16])
{
    size_t n = 0;
    while (line[n] != '\0' && !isspace((unsigned char)line[n]) && n < 15) key[n] = line[n], n++;
    key[n] = '\0';
    return trim(line + n);
}

static int obj_line(Reader *r, char *line)
{
    char key[16];
    char *rest = keyword(line, key);
    ObjModel *m = r->m;
    if (strcmp(key, "v") == 0) return vertex(r, rest);
    if (strcmp(key, "vt") == 0) return texture_vertex(r, rest);
    if (strcmp(key, "f") == 0) return face(r, rest);
    if (strcmp(key, "l") == 0 || strcmp(key, "p") == 0) return fail(r, "a line or point element: a car's model has faces only");
    if (strcmp(key, "o") == 0 || strcmp(key, "g") == 0)
        return strcmp(m->objects[m->object_count - 1].name, rest) == 0 ? 1 : add_object(r, rest);
    if (strcmp(key, "usemtl") == 0) {
        int i = find_material(m, rest);
        if (i < 0) i = add_material(r, rest);
        r->material = i;
        return i >= 0;
    }
    return 1;   /* vn, s, mtllib, comments and the rest */
}

static int mtl_line(Reader *r, char *line, int *current)
{
    char key[16];
    char *rest = keyword(line, key);
    if (strcmp(key, "newmtl") == 0) {
        if (rest[0] == '\0') return fail(r, "newmtl names no material");
        const int i = find_material(r->m, rest);
        *current = i >= 0 ? i : add_material(r, rest);
        return *current >= 0;
    }
    if (strcmp(key, "map_Kd") == 0 && *current >= 0)
        return copy_name(r, r->m->materials[*current].texture, OBJ_PATH_SIZE, rest, "a texture's path");
    return 1;
}

/* each line of `text`, an OBJ's or (mtl) an MTL's, counted from 1 */
static int each_line(Reader *r, const uint8_t *text, size_t len, int mtl)
{
    char line[LINE_SIZE];
    int current = -1;
    r->line = 0;
    for (size_t at = 0; at < len;) {
        size_t end = at;
        while (end < len && text[end] != '\n') end++;
        r->line++;
        if (end - at >= LINE_SIZE) return fail(r, "the line is longer than 4095 characters");
        memcpy(line, text + at, end - at);
        line[end - at] = '\0';
        at = end + 1;
        char *s = trim(line);
        if (s[0] == '#' || s[0] == '\0') continue;
        if (!(mtl ? mtl_line(r, s, &current) : obj_line(r, s))) return 0;
    }
    return 1;
}

int obj_read(const uint8_t *obj, size_t obj_len, const uint8_t *mtl, size_t mtl_len, ObjModel *out, ExchangeError *err)
{
    memset(out, 0, sizeof *out);
    Reader r = { out, 0, 0, 0, 0, 0, 0, 0, -1, 0, "MTL line", err };
    int ok = add_object(&r, "");
    if (ok && mtl != NULL) ok = each_line(&r, mtl, mtl_len, 1);
    static const char EXPORTER[] = "# ORIP mesh";
    out->from_exporter = obj_len >= sizeof EXPORTER - 1 && memcmp(obj, EXPORTER, sizeof EXPORTER - 1) == 0;
    r.file = "line";
    if (ok) ok = each_line(&r, obj, obj_len, 0);
    if (!ok) obj_free(out);
    return ok;
}

void obj_free(ObjModel *m)
{
    free(m->vertices), free(m->uvs), free(m->corner_vertex), free(m->corner_uv);
    free(m->faces), free(m->materials), free(m->objects);
    memset(m, 0, sizeof *m);
}
