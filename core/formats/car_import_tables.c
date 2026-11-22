/* The car importer's raw mode (formats/car_import.h; the owner's decision 4): the mesh from mesh_export's OBJ, its tables and its
 * lists.tsv, in the mesh's own frame and units. What the OBJ does not carry (flags, references, parts, groups, points, labels,
 * header) comes from the tables; a face of the OBJ is a row of polygons.tsv, in order. An OBJ that is not mesh_export's is the one
 * the 3D editor exported from NAME.edit.obj (dep71): it is turned back on the tables' own NAME.obj first (formats/mesh_edit.h). */
#include "formats/car_import_internal.h"
#include "formats/le.h"
#include "formats/mesh_edit.h"
#include "formats/mesh_exchange.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char kind[8];
    uint32_t index;
    uint32_t *values;
    uint32_t count;
} List;

typedef struct {
    List *list;
    size_t count;
} Lists;

static void lists_free(Lists *l)
{
    for (size_t i = 0; i < l->count; i++) free(l->list[i].values);
    free(l->list);
    memset(l, 0, sizeof *l);
}

static int parse_row(char *line, List *out, ExchangeError *err, uint32_t at)
{
    char *save = NULL, *kind = strtok_r(line, "\t", &save), *index = strtok_r(NULL, "\t", &save);
    strtok_r(NULL, "\t", &save);   /* the name, for people */
    char *values = strtok_r(NULL, "\t", &save);
    memset(out, 0, sizeof *out);
    if (kind == NULL || index == NULL || strlen(kind) >= sizeof out->kind) { exchange_fail(err, "lists.tsv line %u: not kind, index, name, values", at); return 0; }
    snprintf(out->kind, sizeof out->kind, "%s", kind);
    out->index = (uint32_t)strtoul(index, NULL, 10);
    for (char *v = values != NULL ? strtok_r(values, " ", &save) : NULL; v != NULL; v = strtok_r(NULL, " ", &save)) {
        char *end;
        const unsigned long x = strtoul(v, &end, 10);
        if (*end != '\0' && *end != '\n' && *end != '\r') { exchange_fail(err, "lists.tsv line %u: '%s' is not a number", at, v); return 0; }
        if (!ci_grow((void **)&out->values, out->count + 1, sizeof *out->values, err)) return 0;
        out->values[out->count++] = (uint32_t)x;
    }
    return 1;
}

static int read_lists(const SetFile *f, Lists *out, ExchangeError *err)
{
    memset(out, 0, sizeof *out);
    char *text = malloc(f->size + 1);
    if (text == NULL) { exchange_fail(err, "out of memory"); return 0; }
    memcpy(text, f->data, f->size), text[f->size] = '\0';
    int ok = 1;
    uint32_t at = 0;
    for (char *save = NULL, *line = strtok_r(text, "\n", &save); ok && line != NULL; line = strtok_r(NULL, "\n", &save)) {
        at++;
        if (line[0] == '#' || line[0] == '\0') continue;
        ok = ci_grow((void **)&out->list, out->count + 1, sizeof *out->list, err) && parse_row(line, &out->list[out->count], err, at);
        if (ok) out->count++;
    }
    free(text);
    if (!ok) lists_free(out);
    return ok;
}

static const List *list_of(const Lists *l, const char *kind, uint32_t index)
{
    for (size_t i = 0; i < l->count; i++)
        if (strcmp(l->list[i].kind, kind) == 0 && l->list[i].index == index) return &l->list[i];
    return NULL;
}

/* the stem of the tables: the name of the one NAME.lists.tsv */
static int stem_of(const FileSet *tables, char *stem, size_t cap, const SetFile **lists, ExchangeError *err)
{
    static const char SUFFIX[] = ".lists.tsv";
    *lists = NULL;
    for (size_t i = 0; tables != NULL && i < tables->count; i++) {
        const char *name = tables->file[i].name;
        const size_t n = strlen(name);
        if (n <= sizeof SUFFIX - 1 || strcmp(name + n - (sizeof SUFFIX - 1), SUFFIX) != 0) continue;
        if (*lists != NULL) { exchange_fail(err, "two lists.tsv files beside the OBJ"); return 0; }
        *lists = &tables->file[i];
        snprintf(stem, cap, "%.*s", (int)(n - (sizeof SUFFIX - 1)), name);
    }
    if (*lists == NULL) {
        exchange_fail(err, "raw mode needs the mesh's lists.tsv beside the OBJ (unpack writes it): its parts' and groups' lists are nowhere else");
        return 0;
    }
    return 1;
}

static int read_table(const FileSet *t, const char *stem, MeshTable which, uint8_t **rec, size_t *rows, ExchangeError *err)
{
    return mesh_table_read(t, stem, which, rec, rows, err);
}

/* polygons.tsv's rows on the OBJ's faces: corners, uvs and the texture of each face's usemtl t<i>_ */
static int raw_polygons(const ObjModel *obj, const uint8_t *rec, size_t rows, MeshDesc *m, ExchangeError *err)
{
    if (rows != obj->face_count) { exchange_fail(err, "the OBJ has %u faces and polygons.tsv %zu rows", obj->face_count, rows); return 0; }
    if (!ci_grow((void **)&m->polygons, rows, sizeof *m->polygons, err)) return 0;
    for (uint32_t f = 0; f < obj->face_count; f++) {
        const ObjFace *face = &obj->faces[f];
        const uint8_t *r = rec + 12 * (size_t)f;
        MeshDescPolygon *p = &m->polygons[f];
        memset(p, 0, sizeof *p);
        p->flags = r[0], p->unknown_1 = r[1], p->texture = r[2], p->unknown_3 = r[3];
        if (face->corners != (uint32_t)(p->flags & 7)) {
            exchange_fail(err, "line %u: a face of %u corners on a row of polygons.tsv saying %d", face->line, face->corners, p->flags & 7);
            return 0;
        }
        if (face->corners > 4) {   /* a polygon holds 4 corners (MeshDescPolygon); the row and the face agreeing on more is refused */
            exchange_fail(err, "line %u: a polygon of %u corners: the race's have 3 or 4", face->line, face->corners);
            return 0;
        }
        const int texture = face->material >= 0 ? mesh_material_texture(obj->materials[face->material].name) : -1;
        if (texture == -2) {
            exchange_fail(err, "line %u: the material %s is not one of the mesh's (t<0..255>_<name>)", face->line, obj->materials[face->material].name);
            return 0;
        }
        if (texture >= 0) p->texture = (uint8_t)texture;
        p->has_uv = 1;
        for (uint32_t k = 0; k < face->corners; k++) p->has_uv &= obj->corner_uv[face->first + k] >= 0;
        for (uint32_t k = 0; k < face->corners; k++) {
            p->vertex[k] = obj->corner_vertex[face->first + k];
            if (p->has_uv) p->uv[k] = (uint32_t)obj->corner_uv[face->first + k];
        }
    }
    m->polygon_count = obj->face_count;
    return 1;
}

static uint32_t *copy_list(const List *l, ExchangeError *err)
{
    uint32_t *v = malloc((l->count ? l->count : 1) * sizeof *v);
    if (v == NULL) exchange_fail(err, "out of memory");
    else memcpy(v, l->values, l->count * sizeof *v);
    return v;
}

/* the parts and groups: their records from the tables, their lists from lists.tsv */
static int raw_lists(const uint8_t *parts, size_t n_parts, const uint8_t *groups, size_t n_groups, const Lists *lists, MeshDesc *m, ExchangeError *err)
{
    if (!ci_grow((void **)&m->parts, n_parts, sizeof *m->parts, err) || !ci_grow((void **)&m->groups, n_groups, sizeof *m->groups, err)) return 0;
    for (size_t i = 0; i < n_parts; i++) {
        const List *l = list_of(lists, "part", (uint32_t)i);
        MeshDescPart *p = &m->parts[m->part_count];
        memset(p, 0, sizeof *p);
        if (l == NULL) { exchange_fail(err, "lists.tsv has no row for part %zu", i); return 0; }
        memcpy(p->name, parts + 20 * i, MESH_PART_NAME_SIZE);
        p->first_offset_vertex = le_u32(parts + 20 * i + 16), p->count = l->count;
        if ((p->vertices = copy_list(l, err)) == NULL) return 0;
        m->part_count++;
    }
    for (size_t i = 0; i < n_groups; i++) {
        const List *l = list_of(lists, "group", (uint32_t)i);
        MeshDescGroup *g = &m->groups[m->group_count];
        memset(g, 0, sizeof *g);
        if (l == NULL) { exchange_fail(err, "lists.tsv has no row for group %zu", i); return 0; }
        memcpy(g->name, groups + 28 * i, MESH_NAME_SIZE);
        g->unknown_0 = le_u32(groups + 28 * i + 12), g->unknown_3 = le_u32(groups + 28 * i + 24), g->count = l->count;
        if ((g->polygons = copy_list(l, err)) == NULL) return 0;
        m->group_count++;
    }
    return 1;
}

/* records of `stride` bytes into an array of the same layout (MeshTexture, MeshSectionC, MeshLabel) */
static int raw_records(void **out, uint32_t *count, size_t item, const uint8_t *rec, size_t rows, ExchangeError *err)
{
    if (!ci_grow(out, rows, item, err)) return 0;
    if (rows > 0) memcpy(*out, rec, rows * item);
    *count = (uint32_t)rows;
    return 1;
}

static int raw_geometry(const ObjModel *obj, MeshDesc *m, ExchangeError *err)
{
    if (!ci_grow((void **)&m->vertices, obj->vertex_count, sizeof *m->vertices, err) || !ci_grow((void **)&m->uvs, obj->uv_count, sizeof *m->uvs, err))
        return 0;
    for (uint32_t i = 0; i < obj->vertex_count; i++)
        m->vertices[i] = (MeshVertex){ (int32_t)lround(obj->vertices[i].x), (int32_t)lround(obj->vertices[i].y), (int32_t)lround(obj->vertices[i].z) };
    for (uint32_t i = 0; i < obj->uv_count; i++) m->uvs[i] = (MeshUv){ (int32_t)lround(obj->uvs[i].u), (int32_t)lround(obj->uvs[i].v) };
    m->vertex_count = obj->vertex_count, m->uv_count = obj->uv_count;
    return 1;
}

static int raw_build(const ObjModel *obj, const FileSet *t, const char *stem, const Lists *lists, MeshDesc *m, ExchangeError *err)
{
    uint8_t *rec[7] = { NULL };
    size_t rows[7] = { 0 };
    int ok = 1;
    for (int k = MESH_TABLE_HEADER; ok && k <= MESH_TABLE_LABELS; k++) ok = read_table(t, stem, (MeshTable)k, &rec[k], &rows[k], err);
    if (ok) {
        memcpy(m->name, rec[MESH_TABLE_HEADER] + 0x2c, sizeof m->name);
        m->unknown_08 = le_u32(rec[MESH_TABLE_HEADER] + 8), m->real_vertices = le_u32(rec[MESH_TABLE_HEADER] + 0x14);
        m->unknown_64 = le_u32(rec[MESH_TABLE_HEADER] + 0x64);
    }
    ok = ok && raw_geometry(obj, m, err) && raw_polygons(obj, rec[MESH_TABLE_POLYGONS], rows[MESH_TABLE_POLYGONS], m, err) &&
         raw_records((void **)&m->textures, &m->texture_count, sizeof *m->textures, rec[MESH_TABLE_TEXTURES], rows[MESH_TABLE_TEXTURES], err) &&
         raw_lists(rec[MESH_TABLE_PARTS], rows[MESH_TABLE_PARTS], rec[MESH_TABLE_GROUPS], rows[MESH_TABLE_GROUPS], lists, m, err) &&
         raw_records((void **)&m->points, &m->point_count, sizeof *m->points, rec[MESH_TABLE_SECTIONC], rows[MESH_TABLE_SECTIONC], err) &&
         raw_records((void **)&m->labels, &m->label_count, sizeof *m->labels, rec[MESH_TABLE_LABELS], rows[MESH_TABLE_LABELS], err);
    for (int k = 0; k < 7; k++) free(rec[k]);
    return ok;
}

/* the texture references' bitmap sizes, from the donor's bank of the level (the bitmaps raw mode keeps), of the bitmap each shows
 * (mesh_edit_shown, as mesh_export: a tyre reference with no name, tyr1's): 1 x 1 for a reference that shows none, a bitmap of no
 * size, or a tyre reference whose tyr1 the bank does not hold (as mesh_export); 0 x 0 for a name of its own the bank does not hold
 * (mesh_edit.h) */
static MeshEditSize *bitmap_sizes(const FileSet *t, const char *stem, const ShpiDescEntry *bank, size_t n, size_t *rows, ExchangeError *err)
{
    uint8_t *rec = NULL;
    if (!read_table(t, stem, MESH_TABLE_TEXTURES, &rec, rows, err)) return NULL;
    MeshEditSize *sizes = malloc((*rows ? *rows : 1) * sizeof *sizes);
    if (sizes == NULL) exchange_fail(err, "out of memory");
    for (size_t i = 0; sizes != NULL && i < *rows; i++) {
        const char *label = (const char *)rec + 20 * i, *name = label + 8;
        char shown[4];
        mesh_edit_shown(label, name, shown);
        const ShpiDescEntry *e = shown[0] != '\0' ? ci_bank_find(bank, n, shown) : NULL;
        if (shown[0] == '\0') sizes[i] = (MeshEditSize){ 1, 1 };
        else if (e == NULL || e->type != 0x7b) sizes[i] = name[0] != '\0' ? (MeshEditSize){ 0, 0 } : (MeshEditSize){ 1, 1 };
        else sizes[i] = e->width > 0 && e->height > 0 ? (MeshEditSize){ e->width, e->height } : (MeshEditSize){ 1, 1 };
    }
    free(rec);
    return sizes;
}

/* the 3D editor's OBJ back in the mesh's frame on the tables' own NAME.obj (mesh_edit.h) */
static int from_edit(const ObjModel *edited, const FileSet *t, const char *stem, const ShpiDescEntry *bank, size_t n, ObjModel *out,
                        ExchangeError *err)
{
    char name[300];
    snprintf(name, sizeof name, "%s.obj", stem);
    const SetFile *f = fileset_find(t, name);
    if (f == NULL) {
        exchange_fail(err, "the OBJ is not mesh_export's, so it is read as the 3D editor's export of %s.edit.obj: that needs the tables' own %s "
                           "beside them (unpack writes it)", stem, name);
        return 0;
    }
    ObjModel original;
    if (!obj_read(f->data, f->size, NULL, 0, &original, err)) return 0;
    size_t rows = 0;
    MeshEditSize *sizes = NULL;
    int ok = original.from_exporter;
    if (!ok) exchange_fail(err, "%s beside the tables is not mesh_export's (its first line is not '# ORIP mesh')", name);
    ok = ok && (sizes = bitmap_sizes(t, stem, bank, n, &rows, err)) != NULL && mesh_edit_back(edited, &original, sizes, (uint32_t)rows, out, err);
    free(sizes);
    obj_free(&original);
    return ok;
}

int ci_raw_mesh(const CarImportIn *in, int level, const MeshDesc *donor, const ShpiDescEntry *donor_bank, size_t donor_count,
                MeshDesc *out, CarImportBank *bank, ExchangeError *err)
{
    (void)donor;
    const ObjModel *obj = level == 0 ? in->high : in->low;
    const FileSet *t = level == 0 ? in->high_tables : in->low_tables;
    memset(out, 0, sizeof *out);
    memset(bank, 0, sizeof *bank);
    char stem[256];
    const SetFile *lists_file = NULL;
    Lists lists;
    if (!stem_of(t, stem, sizeof stem, &lists_file, err) || !read_lists(lists_file, &lists, err)) return 0;
    ObjModel back;
    const int edited = !obj->from_exporter;
    const int converted = edited && from_edit(obj, t, stem, donor_bank, donor_count, &back, err);
    int ok = (!edited || converted) && raw_build(edited ? &back : obj, t, stem, &lists, out, err);   /* the bank: the donor's (car_import.c) */
    if (converted) obj_free(&back);
    lists_free(&lists);
    if (!ok) mesh_desc_free(out), car_import_bank_free(bank);
    return ok;
}
