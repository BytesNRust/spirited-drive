#include "formats/mesh_exchange.h"

#include "formats/chunk.h"
#include "formats/le.h"
#include "formats/mesh.h"
#include "formats/png.h"
#include "formats/shpi_exchange.h"
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

/* one newmtl per texture reference; map_Kd at the first bank that has the entry */
static void write_mtl(const Mesh *mesh, const MeshTextureBank *banks, size_t bank_count, Text *mtl)
{
    for (uint32_t i = 0; i < mesh->texture_count; ++i) {
        MeshTexture t;
        char label[5], file[SHPI_FILE_NAME_SIZE];
        mesh_texture(mesh, i, &t);
        texture_label(&t, label);
        text_printf(mtl, "newmtl t%u_%s\nKd 1 1 1\n", i, label);
        for (size_t b = 0; t.name[0] != '\0' && b < bank_count; ++b) {
            if (!shpi_entry_file(banks[b].shpi, banks[b].size, t.name, file)) continue;
            text_printf(mtl, "map_Kd %s/%s\n", banks[b].dir, file);
            break;
        }
        text_put(mtl, "\n", 1);
    }
}

void mesh_edit_shown(const char label[8], const char name[4], char shown[4])
{
    static const char ZERO[4];
    memcpy(shown, name, 4);
    if (memcmp(name, ZERO, 4) == 0 && (strncmp(label, "frntires", 8) == 0 || strncmp(label, "rertires", 8) == 0)) memcpy(shown, "tyr1", 4);
}

enum { BANK_KEY = 0xff };   /* the car banks' transparent key (car_texture.c's TRANSPARENT) */

/* fix/edited-preview: NAME.edit.mtl, the 3D editor's own: each reference shows its bitmap (mesh_edit_shown), from the first bank that holds
 * it, or, when it has the key colour, from an RGBA copy NAME.edit.<label>.png beside the OBJ with the key transparent (map_Kd and
 * map_d). `stem` the files' stem, `base` its last part. 0 when memory runs out */
static int write_edit_mtl(const Mesh *mesh, const MeshTextureBank *banks, size_t bank_count, const char *stem, const char *base, Text *mtl,
                             FileSet *out)
{
    for (uint32_t i = 0; i < mesh->texture_count; ++i) {
        MeshTexture t;
        char label[5], file[SHPI_FILE_NAME_SIZE], shown[4];
        mesh_texture(mesh, i, &t);
        texture_label(&t, label);
        mesh_edit_shown(t.label, t.name, shown);
        text_printf(mtl, "newmtl t%u_%s\nKd 1 1 1\n", i, label);
        for (size_t b = 0; shown[0] != '\0' && b < bank_count; ++b) {
            if (!shpi_entry_file(banks[b].shpi, banks[b].size, shown, file)) continue;
            uint8_t *rgba = NULL, *png = NULL;
            uint32_t w, h, keyed;
            size_t png_len = 0;
            if (!shpi_entry_rgba(banks[b].shpi, banks[b].size, shown, BANK_KEY, &rgba, &w, &h, &keyed) || keyed == 0) {
                free(rgba);
                text_printf(mtl, "map_Kd %s/%s\n", banks[b].dir, file);
                break;
            }
            MeshTexture as = t;
            memcpy(as.name, shown, 4);
            char shown_label[5], suffix[32];
            texture_label(&as, shown_label);
            snprintf(suffix, sizeof suffix, ".edit.%s.png", shown_label);
            char *copy = named(stem, suffix);
            int ok = copy != NULL;
            if (ok && fileset_find(out, copy) == NULL) {   /* one copy a bitmap, however many references show it */
                ok = png_write_rgba(rgba, w, h, &png, &png_len) == PNG_OK && fileset_add_owned(out, copy, png, png_len);
                if (!ok) free(png);
            }
            free(rgba), free(copy);
            if (!ok) return 0;
            text_printf(mtl, "map_Kd %s%s\nmap_d %s%s\n", base, suffix, base, suffix);
            break;
        }
        text_put(mtl, "\n", 1);
    }
    return 1;
}

/* the size of the bitmap `name` in the first of `banks` that holds it (its entry header's words 4 and 6); 1 x 1 when none does */
static void bitmap_size(const MeshTextureBank *banks, size_t bank_count, const char name[4], uint32_t *w, uint32_t *h)
{
    *w = 1, *h = 1;
    for (size_t b = 0; name[0] != '\0' && b < bank_count; ++b) {
        const uint8_t *d = banks[b].shpi;
        const size_t size = banks[b].size;
        if (size < 16 || memcmp(d, "SHPI", 4) != 0) continue;
        for (uint32_t i = 0; i < le_u32(d + 8) && 16 + 8 * (size_t)i + 8 <= size; ++i) {
            const uint32_t at = le_u32(d + 16 + 8 * (size_t)i + 4);
            if (memcmp(d + 16 + 8 * (size_t)i, name, 4) != 0 || (size_t)at + 8 > size || d[at] != 0x7b) continue;
            const uint32_t bw = (uint32_t)(d[at + 4] | d[at + 5] << 8), bh = (uint32_t)(d[at + 6] | d[at + 7] << 8);
            if (bw > 0 && bh > 0) *w = bw, *h = bh;
            return;
        }
    }
}

/* +0.0 for -0.0: the same text for a zero either way */
static double unsigned_zero(double v) { return v + 0.0; }

/* dep71: NAME.edit.obj (mesh_exchange.h); `mtl` the MTL's file name. 0 with the reason in `err` */
static int edit_obj(const Mesh *mesh, const char *name, const char *mtl, const MeshTextureBank *banks, size_t bank_count, Text *obj,
                       ExchangeError *err)
{
    text_printf(obj, "# Spirited Drive mesh '%s' for the 3D editor: metres (%d units), Y up, -Z forward; car import --raw takes it back\n",
                name, MESH_EDIT_UNITS);
    text_printf(obj, "mtllib %s\no %s\n", mtl, name[0] ? name : "mesh");
    for (uint32_t i = 0; i < mesh->vertex_count; ++i) {
        MeshVertex v;
        mesh_vertex(mesh, i, &v);
        text_printf(obj, "v %.6f %.6f %.6f\n", unsigned_zero((double)v.x / MESH_EDIT_UNITS), unsigned_zero((double)v.y / MESH_EDIT_UNITS),
                    unsigned_zero(-(double)v.z / MESH_EDIT_UNITS));
    }
    for (int pass = 0; pass < 2; ++pass) {   /* every corner's vt, then the faces */
        uint32_t corner_vt = 0;
        int current = -1;
        for (uint32_t p = 0; p < mesh->polygon_count; ++p) {
            MeshPolygon poly;
            MeshTexture t;
            uint32_t vi[4], ui[4], w, h;
            if (mesh_polygon(mesh, p, &poly) != MESH_OK || mesh_polygon_corners(mesh, &poly, vi, ui) != MESH_OK ||
                mesh_texture(mesh, poly.texture, &t) != MESH_OK) {
                exchange_fail(err, "%s: polygon %u: not readable", name, p);
                return 0;
            }
            const int corners = poly.flags & 7, with_uv = poly.uv_slot != poly.vertex_slot;
            char shown[4];
            mesh_edit_shown(t.label, t.name, shown);
            bitmap_size(banks, bank_count, shown, &w, &h);
            if (pass == 1 && poly.texture != current) {
                char label[5];
                texture_label(&t, label);
                text_printf(obj, "usemtl t%d_%s\n", poly.texture, label);
                current = poly.texture;
            }
            if (pass == 1) text_put(obj, corners == 2 ? "l" : "f", 1);
            for (int k = 0; k < corners; ++k) {
                MeshUv uv = { 0, 0 };
                if (with_uv) mesh_uv(mesh, ui[k], &uv);
                if (pass == 0 && with_uv) text_printf(obj, "vt %.6f %.6f\n", unsigned_zero((double)uv.u / w), unsigned_zero(1.0 - (double)uv.v / h));
                else if (pass == 1 && with_uv) text_printf(obj, " %u/%u", vi[k] + 1, ++corner_vt);
                else if (pass == 1) text_printf(obj, " %u", vi[k] + 1);
            }
            if (pass == 1) text_put(obj, "\n", 1);
        }
    }
    return 1;
}

/* one row of NAME.lists.tsv: kind, record index, its name to the NUL, the `count` map values from `slot` (as far as the map goes) */
static void list_row(Text *t, const Mesh *m, const char *kind, uint32_t index, const char *name, size_t name_size, uint32_t slot, uint32_t count)
{
    text_printf(t, "%s\t%u\t", kind, index);
    for (size_t i = 0; i < name_size && name[i] != '\0'; i++) {
        const char c = name[i];
        text_put(t, isprint((unsigned char)c) && c != '\t' ? &c : "_", 1);
    }
    text_put(t, "\t", 1);
    uint32_t v;
    for (uint32_t k = 0; k < count && mesh_vertex_map(m, slot + k, &v) == MESH_OK; k++) text_printf(t, k ? " %u" : "%u", v);
    text_put(t, "\n", 1);
}

/* NAME.lists.tsv: each part's vertex list (its words 0, the count, and 1, the slot) and each group's polygon list (words 2 and 1) */
static int add_lists(const Mesh *m, const char *stem, FileSet *out, ExchangeError *err)
{
    Text t = {0};
    text_printf(&t, "# lists: kind\tindex\tname\tvalues (a part's vertices, a group's polygons, as the vertex map holds them)\n");
    for (uint32_t i = 0; i < m->section_a_count; i++) {
        MeshSectionA a;
        if (mesh_section_a(m, i, &a) == MESH_OK) list_row(&t, m, "part", i, a.name, sizeof a.name, a.unknown[1], a.unknown[0]);
    }
    for (uint32_t i = 0; i < m->section_b_count; i++) {
        MeshSectionB b;
        if (mesh_section_b(m, i, &b) == MESH_OK) list_row(&t, m, "group", i, b.name, sizeof b.name, b.unknown[1], b.unknown[2]);
    }
    char *name = named(stem, ".lists.tsv");
    const int ok = !t.failed && name != NULL && fileset_add(out, name, (uint8_t *)t.data, t.len);
    if (!ok) exchange_fail(err, "%s: cannot write the lists", stem);
    free(name);
    free(t.data);
    return ok;
}

int mesh_export(const uint8_t *orip, size_t size, const char *stem, const MeshTextureBank *banks, size_t bank_count, FileSet *out,
                ExchangeError *err)
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
    if (ok) write_mtl(&mesh, banks, bank_count, &mtl);
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
    if (ok) {   /* dep71: the same mesh for the 3D editor, with its own MTL (fix/edited-preview) */
        Text edited = { 0 }, bmtl = { 0 };
        const char *const base = strchr(stem, '/') ? strrchr(stem, '/') + 1 : stem;
        char *mtl_name = named(base, ".edit.mtl"), *f = named(stem, ".edit.obj"), *fm = named(stem, ".edit.mtl");
        ok = mtl_name && f && fm && edit_obj(&mesh, name, mtl_name, banks, bank_count, &edited, err);
        if (ok && !write_edit_mtl(&mesh, banks, bank_count, stem, base, &bmtl, out)) exchange_fail(err, "%s: cannot write the the 3D editor previews", stem), ok = 0;
        if (ok && (edited.failed || bmtl.failed || !fileset_add(out, f, (uint8_t *)edited.data, edited.len) ||
                   !fileset_add(out, fm, (uint8_t *)bmtl.data, bmtl.len)))
            exchange_fail(err, "%s: cannot write the the 3D editor OBJ", stem), ok = 0;
        free(mtl_name), free(f), free(fm), free(edited.data), free(bmtl.data);
    }
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
           add_table(&labels_spec, orip, mesh.label_offset, mesh.label_count, stem, out, err) &&
           add_lists(&mesh, stem, out, err);
}

/* ---------------------------------------------------------------- import */

int mesh_material_texture(const char *name)
{
    if (name[0] != 't' || !isdigit((unsigned char)name[1])) return -1;
    int index = 0;
    const char *p = name + 1;
    for (; isdigit((unsigned char)*p); p++)
        if ((index = index * 10 + (*p - '0')) > 255) return -2;
    return *p == '_' ? index : -2;
}

int mesh_table_read(const FileSet *in, const char *stem, MeshTable table, uint8_t **records, size_t *rows, ExchangeError *err)
{
    static const TableSpec *const SPECS[] = { NULL, &polygons_spec, &textures_spec, &parts_spec, &groups_spec, &sectionc_spec, &labels_spec };
    *records = NULL, *rows = 0;
    if ((size_t)table >= COUNT(SPECS)) { exchange_fail(err, "no table %d", (int)table); return 0; }
    const TableSpec *spec = SPECS[table];
    const size_t stride = spec != NULL ? spec->stride : 0x70, cap = spec != NULL ? MESH_TABLE_MAX_ROWS : 1;
    char *name = named(stem, spec != NULL ? spec->suffix : ".header.tsv");
    uint8_t *buf = name != NULL ? calloc(cap, stride) : NULL;
    int ok = buf != NULL;
    if (!ok) exchange_fail(err, "out of memory");
    else if (fileset_find(in, name) == NULL) exchange_fail(err, "%s not found", name), ok = 0;
    else if (spec == NULL) ok = record_import(buf, stride, header_fields, COUNT(header_fields), in, name, err), *rows = 1;
    else ok = table_import(buf, stride, 0, cap, spec->fields, spec->field_count, in, name, rows, err);
    free(name);
    if (!ok) { free(buf); *rows = 0; return 0; }
    *records = buf;
    return 1;
}

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
