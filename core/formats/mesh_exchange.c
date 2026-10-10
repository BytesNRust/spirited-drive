#include "formats/mesh_exchange.h"

#include "formats/chunk.h"
#include "formats/le.h"
#include "formats/mesh.h"
#include "formats/table_exchange.h"

#include <ctype.h>
#include <stdarg.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- tables */

static const RecordField header_fields[] = {
    { 0x08, REC_U32, 1, "unknown_08" },
    { 0x14, REC_U32, 1, "unknown_14" },
    { 0x2c, REC_BYTES, 12, "name" },
    { 0x64, REC_U32, 1, "unknown_64" }
};

static const RecordField polygon_fields[] = {
    { 0, REC_U8, 1, "flags" },
    { 1, REC_U8, 1, "unknown_1" },
    { 2, REC_U8, 1, "texture" },
    { 3, REC_U8, 1, "unknown_3" },
    { 4, REC_U32, 1, "vertex_slot" },
    { 8, REC_U32, 1, "uv_slot" }
};

static const RecordField texture_fields[] = {
    { 0, REC_BYTES, 8, "label" },       /* 8 characters: the part names the car loader looks up (frntires, tire1, left_tur...) */
    { 8, REC_BYTES, 4, "name" },
    { 12, REC_U32, 2, "unknown_1" }
};

static const RecordField part_fields[] = {
    { 0, REC_BYTES, 8, "name" },
    { 8, REC_U32, 3, "unknown" }
};

static const RecordField group_fields[] = {
    { 0, REC_BYTES, 12, "name" },
    { 12, REC_U32, 4, "unknown" }
};

static const RecordField sectionc_fields[] = {
    { 0, REC_BYTES, 8, "name" },
    { 8, REC_U32, 1, "value" }
};

static const RecordField label_fields[] = {
    { 0, REC_BYTES, 8, "name" },
    { 8, REC_U32, 1, "polygon" }
};

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

typedef struct {
    const char *suffix;
    const char *header;
    const RecordField *fields;
    size_t field_count;
    size_t stride;
} TableSpec;

/* In the order they are exported and imported. */
static const TableSpec polygons_spec = { ".polygons.tsv", "polygons: flags low 3 bits = corners; texture follows the OBJ's usemtl", polygon_fields, COUNT(polygon_fields), 12 };
static const TableSpec textures_spec = { ".textures.tsv", "texture references; name = FSH entry name", texture_fields, COUNT(texture_fields), 20 };
static const TableSpec parts_spec = { ".parts.tsv", "section A (parts)", part_fields, COUNT(part_fields), 20 };
static const TableSpec groups_spec = { ".groups.tsv", "section B (groups)", group_fields, COUNT(group_fields), 28 };
static const TableSpec sectionc_spec = { ".sectionc.tsv", "section C", sectionc_fields, COUNT(sectionc_fields), 12 };
static const TableSpec labels_spec = { ".labels.tsv", "labels: polygon index", label_fields, COUNT(label_fields), 12 };

/* ---------------------------------------------------------------- text buffer */

typedef struct {
    char *data;
    size_t len, cap;
    int failed;
} Text;

static void text_put(Text *t, const char *s, size_t n)
{
    if (t->failed) return;
    if (t->len + n + 1 > t->cap) {
        size_t cap = t->cap ? t->cap : 4096;
        while (cap < t->len + n + 1) cap *= 2;
        char *grown = realloc(t->data, cap);
        if (grown == NULL) { t->failed = 1; return; }
        t->data = grown;
        t->cap = cap;
    }
    memcpy(t->data + t->len, s, n);
    t->len += n;
    t->data[t->len] = '\0';
}

static void text_printf(Text *t, const char *format, ...) __attribute__((format(printf, 2, 3)));
static void text_printf(Text *t, const char *format, ...)
{
    char buf[256];
    va_list args;
    va_start(args, format);
    const int n = vsnprintf(buf, sizeof buf, format, args);
    va_end(args);
    if (n > 0) text_put(t, buf, (size_t)(n < (int)sizeof buf ? n : (int)sizeof buf - 1));
}

/* ---------------------------------------------------------------- export */

static char *named(const char *stem, const char *suffix)
{
    char *name = malloc(strlen(stem) + strlen(suffix) + 1);
    if (name != NULL) { strcpy(name, stem); strcat(name, suffix); }
    return name;
}

static void texture_label(const MeshTexture *t, char label[5])
{
    for (int i = 0; i < 4; ++i) label[i] = (isalnum((unsigned char)t->name[i]) || t->name[i] == '-') ? t->name[i] : '_';
    label[4] = '\0';
}

static int add_table(const TableSpec *spec, const uint8_t *base, uint32_t offset, uint32_t count, const char *stem, FileSet *out, ExchangeError *err)
{
    char *name = named(stem, spec->suffix);
    const int ok = name != NULL && table_export(base + offset, spec->stride, count, spec->fields, spec->field_count, spec->header, out, name, err);
    free(name);
    return ok;
}

int mesh_export(const uint8_t *orip, size_t size, const char *stem, const char *texture_dir, FileSet *out, ExchangeError *err)
{
    Chunk chunk;
    Mesh mesh;
    const ChunkResult cr = chunk_open(&chunk, orip, size);
    if (cr != CHUNK_OK || chunk.kind != CHUNK_KIND_MESH || mesh_open(&mesh, &chunk) != MESH_OK) {
        exchange_fail(err, "%s: not a readable ORIP mesh", stem);
        return 0;
    }
    Text obj = {0}, mtl = {0};
    char name[13];
    memcpy(name, mesh.name, 12);
    name[12] = '\0';
    for (char *p = name; *p; ++p) if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-') *p = '_';
    text_printf(&obj, "# ORIP mesh '%s': integer units as stored, no axis flip\nmtllib %s.mtl\no %s\n", name, strchr(stem, '/') ? strrchr(stem, '/') + 1 : stem, name[0] ? name : "mesh");
    for (uint32_t i = 0; i < mesh.vertex_count; ++i) {
        MeshVertex v;
        mesh_vertex(&mesh, i, &v);
        text_printf(&obj, "v %d %d %d\n", v.x, v.y, v.z);
    }
    for (uint32_t i = 0; i < mesh.uv_count; ++i) {
        MeshUv uv;
        mesh_uv(&mesh, i, &uv);
        text_printf(&obj, "vt %d %d\n", uv.u, uv.v);
    }
    int ok = 1, current = -1;
    for (uint32_t p = 0; p < mesh.polygon_count && ok; ++p) {
        MeshPolygon poly;
        uint32_t vi[4], ui[4];
        if (mesh_polygon(&mesh, p, &poly) != MESH_OK || mesh_polygon_corners(&mesh, &poly, vi, ui) != MESH_OK) {
            exchange_fail(err, "%s: polygon %u: vertex map slots outside the chunk", stem, p);
            ok = 0;
            break;
        }
        const int corners = poly.flags & 7;
        if (corners < 2 || corners > 4) { exchange_fail(err, "%s: polygon %u has %d corners", stem, p, corners); ok = 0; break; }
        if (poly.texture != current) {
            current = poly.texture;
            MeshTexture t;
            char label[5];
            if (mesh_texture(&mesh, poly.texture, &t) != MESH_OK) { exchange_fail(err, "%s: polygon %u: texture %d does not exist", stem, p, poly.texture); ok = 0; break; }
            texture_label(&t, label);
            text_printf(&obj, "usemtl t%d_%s\n", poly.texture, label);
        }
        const int with_uv = poly.uv_slot != poly.vertex_slot;
        text_put(&obj, corners == 2 ? "l" : "f", 1);
        for (int k = 0; k < corners; ++k) {
            if (vi[k] >= mesh.vertex_count || (with_uv && ui[k] >= mesh.uv_count)) {
                exchange_fail(err, "%s: polygon %u corner %d: index past the vertex or uv table", stem, p, k);
                ok = 0;
                break;
            }
            if (with_uv) text_printf(&obj, " %u/%u", vi[k] + 1, ui[k] + 1);
            else text_printf(&obj, " %u", vi[k] + 1);
        }
        text_put(&obj, "\n", 1);
    }
    for (uint32_t i = 0; ok && i < mesh.texture_count; ++i) {
        MeshTexture t;
        char label[5];
        mesh_texture(&mesh, i, &t);
        texture_label(&t, label);
        text_printf(&mtl, "newmtl t%u_%s\nKd 1 1 1\n", i, label);
        if (texture_dir != NULL && t.name[0] != '\0') text_printf(&mtl, "map_Kd %s/%s.png\n", texture_dir, label);
        text_put(&mtl, "\n", 1);
    }
    if (ok && (obj.failed || mtl.failed)) { exchange_fail(err, "out of memory"); ok = 0; }
    if (ok) {
        char *f1 = named(stem, ".obj"), *f2 = named(stem, ".mtl");
        ok = f1 && f2 && fileset_add(out, f1, (uint8_t *)obj.data, obj.len) && fileset_add(out, f2, (uint8_t *)mtl.data, mtl.len);
        if (!ok) exchange_fail(err, "%s: cannot write the OBJ or MTL file", stem);
        free(f1);
        free(f2);
    }
    free(obj.data);
    free(mtl.data);
    if (ok) {
        char *hname = named(stem, ".header.tsv");
        ok = hname && record_export(orip, 0x70, header_fields, COUNT(header_fields), "ORIP header words", out, hname, err);
        free(hname);
    }
    return ok &&
           add_table(&polygons_spec, orip, mesh.polygon_offset, mesh.polygon_count, stem, out, err) &&
           add_table(&textures_spec, orip, mesh.texture_offset, mesh.texture_count, stem, out, err) &&
           add_table(&parts_spec, orip, mesh.section_a_offset, mesh.section_a_count, stem, out, err) &&
           add_table(&groups_spec, orip, mesh.section_b_offset, mesh.section_b_count, stem, out, err) &&
           add_table(&sectionc_spec, orip, mesh.section_c_offset, mesh.section_c_count, stem, out, err) &&
           add_table(&labels_spec, orip, mesh.label_offset, mesh.label_count, stem, out, err);
}

/* ---------------------------------------------------------------- import */

static void put_u32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i)); }

static int parse_int(const char *s, char **end, int32_t *out)
{
    errno = 0;
    const double v = strtod(s, end);
    if (*end == s || errno != 0 || !(v > -2147483648.5 && v < 2147483647.5)) return 0;
    *out = (int32_t)lround(v);
    return 1;
}

typedef struct {
    int32_t v[4], t[4];
    int has_t, corners;
} Face;

/* Parses "a", "a/b", "a//c" or "a/b/c" into 0-based indices; negative values count from the end. */
static int parse_corner(const char *s, uint32_t nv, uint32_t nt, int32_t *v, int32_t *t, int *has_t, char **end)
{
    char *p;
    long a = strtol(s, &p, 10);
    if (p == s) return 0;
    *t = -1;
    *has_t = 0;
    if (*p == '/') {
        ++p;
        if (*p != '/' && *p != ' ' && *p != '\0' && *p != '\r' && *p != '\n') {
            const long b = strtol(p, &p, 10);
            *has_t = 1;
            *t = (int32_t)(b < 0 ? (long)nt + b : b - 1);
        }
        if (*p == '/') { ++p; strtol(p, &p, 10); }
    }
    *v = (int32_t)(a < 0 ? (long)nv + a : a - 1);
    *end = p;
    return 1;
}

int mesh_import(const uint8_t *orip, size_t size, const char *stem, const FileSet *in, uint8_t **out, size_t *out_len, ExchangeError *err)
{
    Chunk chunk;
    Mesh mesh;
    if (chunk_open(&chunk, orip, size) != CHUNK_OK || chunk.kind != CHUNK_KIND_MESH || mesh_open(&mesh, &chunk) != MESH_OK) {
        exchange_fail(err, "%s: the original is not a readable ORIP mesh", stem);
        return 0;
    }
    uint8_t *buf = malloc(mesh.size);
    if (buf == NULL) { exchange_fail(err, "out of memory"); return 0; }
    memcpy(buf, orip, mesh.size);
    size_t found = 0;
    int ok = 1;

    char *hname = named(stem, ".header.tsv");
    ok = hname && record_import(buf, 0x70, header_fields, COUNT(header_fields), in, hname, err);
    free(hname);
    const TableSpec *specs[] = { &polygons_spec, &textures_spec, &parts_spec, &groups_spec, &sectionc_spec, &labels_spec };
    const uint32_t offsets[] = { mesh.polygon_offset, mesh.texture_offset, mesh.section_a_offset, mesh.section_b_offset, mesh.section_c_offset, mesh.label_offset };
    const uint32_t counts[] = { mesh.polygon_count, mesh.texture_count, mesh.section_a_count, mesh.section_b_count, mesh.section_c_count, mesh.label_count };
    for (size_t i = 0; ok && i < COUNT(specs); ++i) {
        char *name = named(stem, specs[i]->suffix);
        ok = name && table_import(buf + offsets[i], specs[i]->stride, counts[i], counts[i], specs[i]->fields, specs[i]->field_count, in, name, &found, err);
        free(name);
    }

    char *obj_name = named(stem, ".obj");
    const SetFile *objf = ok ? fileset_find(in, obj_name) : NULL;
    if (ok && objf == NULL) { exchange_fail(err, "%s not found", obj_name); ok = 0; }
    if (ok) {
        char *text = malloc(objf->size + 1);
        if (text == NULL) { exchange_fail(err, "out of memory"); ok = 0; }
        else {
            memcpy(text, objf->data, objf->size);
            text[objf->size] = '\0';
            uint32_t nv = 0, nt = 0, nf = 0;
            int32_t material = -1;
            uint32_t line_no = 0;
            char *save = text;
            while (ok && *save) {
                char *line = save;
                char *nl = strchr(line, '\n');
                if (nl) { *nl = '\0'; save = nl + 1; } else save += strlen(line);
                ++line_no;
                while (*line == ' ' || *line == '\t') ++line;
                char *end;
                if (strncmp(line, "v ", 2) == 0) {
                    int32_t c[3];
                    char *p = line + 2;
                    for (int k = 0; k < 3; ++k) {
                        if (!parse_int(p, &end, &c[k])) { exchange_fail(err, "%s:%u: bad vertex coordinate", obj_name, line_no); ok = 0; break; }
                        p = end;
                    }
                    if (!ok) break;
                    if (nv >= mesh.vertex_count) { exchange_fail(err, "%s:%u: more than the original's %u vertices (adding geometry is not supported)", obj_name, line_no, mesh.vertex_count); ok = 0; break; }
                    uint8_t *dst = buf + mesh.vertex_offset + 12 * (size_t)nv;
                    for (int k = 0; k < 3; ++k) put_u32(dst + 4 * k, (uint32_t)c[k]);
                    ++nv;
                } else if (strncmp(line, "vt ", 3) == 0) {
                    int32_t c[2];
                    char *p = line + 3;
                    for (int k = 0; k < 2; ++k) {
                        if (!parse_int(p, &end, &c[k])) { exchange_fail(err, "%s:%u: bad texture coordinate", obj_name, line_no); ok = 0; break; }
                        p = end;
                    }
                    if (!ok) break;
                    if (nt >= mesh.uv_count) { exchange_fail(err, "%s:%u: more than the original's %u uvs", obj_name, line_no, mesh.uv_count); ok = 0; break; }
                    uint8_t *dst = buf + mesh.uv_offset + 8 * (size_t)nt;
                    put_u32(dst, (uint32_t)c[0]);
                    put_u32(dst + 4, (uint32_t)c[1]);
                    ++nt;
                } else if (strncmp(line, "usemtl ", 7) == 0) {
                    char *m = line + 7;
                    if (m[0] != 't' || !isdigit((unsigned char)m[1])) { exchange_fail(err, "%s:%u: material '%s' is not one of the exported t<index>_<name>", obj_name, line_no, m); ok = 0; break; }
                    const long idx = strtol(m + 1, NULL, 10);
                    if (idx < 0 || (uint32_t)idx >= mesh.texture_count) { exchange_fail(err, "%s:%u: material index %ld is not a texture reference of this mesh", obj_name, line_no, idx); ok = 0; break; }
                    material = (int32_t)idx;
                } else if ((line[0] == 'f' || line[0] == 'l') && line[1] == ' ') {
                    /* vertices and uvs precede faces in an export; a face needs all of them already read */
                    if (nf >= mesh.polygon_count) { exchange_fail(err, "%s:%u: more than the original's %u polygons", obj_name, line_no, mesh.polygon_count); ok = 0; break; }
                    uint8_t *poly = buf + mesh.polygon_offset + 12 * (size_t)nf;
                    const int want = poly[0] & 7;
                    int32_t v[4], t[4];
                    int has_t[4], n = 0;
                    char *p = line + 2;
                    while (*p) {
                        while (*p == ' ' || *p == '\t' || *p == '\r') ++p;
                        if (!*p) break;
                        if (n == 4) { n = 5; break; }
                        if (!parse_corner(p, mesh.vertex_count, mesh.uv_count, &v[n], &t[n], &has_t[n], &end)) { exchange_fail(err, "%s:%u: bad face corner", obj_name, line_no); ok = 0; break; }
                        p = end;
                        ++n;
                    }
                    if (!ok) break;
                    if (n != want) { exchange_fail(err, "%s:%u: polygon %u has %d corners in the original, %d here (changing topology is not supported)", obj_name, line_no, nf, want, n); ok = 0; break; }
                    const uint32_t vs = le_u32(poly + 4), us = le_u32(poly + 8);
                    const uint32_t slots = mesh.vertex_map_slots;
                    for (int k = 0; k < n && ok; ++k) {
                        if (v[k] < 0 || (uint32_t)v[k] >= mesh.vertex_count) { exchange_fail(err, "%s:%u: vertex index %d out of range", obj_name, line_no, v[k] + 1); ok = 0; break; }
                        if ((uint64_t)vs + k >= slots) { exchange_fail(err, "%s:%u: polygon %u vertex slot outside the vertex map", obj_name, line_no, nf); ok = 0; break; }
                        put_u32(buf + mesh.vertex_map_offset + 4 * ((size_t)vs + k), (uint32_t)v[k]);
                        if (us != vs && has_t[k]) {
                            if (t[k] < 0 || (uint32_t)t[k] >= mesh.uv_count || (uint64_t)us + k >= slots) { exchange_fail(err, "%s:%u: uv index out of range", obj_name, line_no); ok = 0; break; }
                            put_u32(buf + mesh.vertex_map_offset + 4 * ((size_t)us + k), (uint32_t)t[k]);
                        }
                    }
                    if (ok && material >= 0) poly[2] = (uint8_t)material;
                    ++nf;
                }
            }
            if (ok && (nv != mesh.vertex_count || nt != mesh.uv_count || nf != mesh.polygon_count)) {
                exchange_fail(err, "%s has %u vertices, %u uvs and %u faces; the original has %u, %u and %u (adding or removing geometry is not supported)", obj_name, nv, nt, nf, mesh.vertex_count, mesh.uv_count, mesh.polygon_count);
                ok = 0;
            }
            free(text);
        }
    }
    free(obj_name);
    if (ok) { *out = buf; *out_len = mesh.size; }
    else free(buf);
    return ok;
}
