/* See mesh_build.h. */
#include "formats/mesh_build.h"

#include <stdlib.h>
#include <string.h>

enum {
    HEADER_SIZE = 0x70, VERTEX_SIZE = 12, UV_SIZE = 8, POLYGON_SIZE = 12, TEXTURE_SIZE = 20, PART_SIZE = 20, GROUP_SIZE = 28,
    POINT_SIZE = 12, LABEL_SIZE = 12, SLOT_SIZE = 4, CORNERS = 4
};

static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24); }

static int corners_of(uint8_t flags) { return flags & 7; }

int mesh_desc_name(char *field, size_t size, const char *text, ExchangeError *err)
{
    const size_t n = strlen(text);
    if (n > size) {
        exchange_fail(err, "the name '%s' is longer than its %zu bytes", text, size);
        return 0;
    }
    memset(field, 0, size);
    memcpy(field, text, n);
    return 1;
}

void mesh_desc_free(MeshDesc *d)
{
    if (d == NULL) return;
    for (uint32_t i = 0; d->parts != NULL && i < d->part_count; i++) free(d->parts[i].vertices);
    for (uint32_t i = 0; d->groups != NULL && i < d->group_count; i++) free(d->groups[i].polygons);
    free(d->vertices), free(d->uvs), free(d->polygons), free(d->textures), free(d->parts), free(d->groups), free(d->points), free(d->labels);
    memset(d, 0, sizeof *d);
}

/* ---- describe */

/* `count` slots of the map from `slot` into `out`; 0 when they run past it */
static int slots(const Mesh *m, uint32_t slot, uint32_t count, uint32_t *out)
{
    for (uint32_t k = 0; k < count; k++)
        if (slot > UINT32_MAX - k || mesh_vertex_map(m, slot + k, &out[k]) != MESH_OK) return 0;
    return 1;
}

static void *table(uint32_t count, size_t size) { return calloc(count > 0 ? count : 1, size); }

static int describe_polygons(const Mesh *m, MeshDesc *d, ExchangeError *err)
{
    for (uint32_t i = 0; i < m->polygon_count; i++) {
        MeshPolygon p;
        MeshDescPolygon *q = &d->polygons[i];
        if (mesh_polygon(m, i, &p) != MESH_OK) { exchange_fail(err, "polygon %u cannot be read", i); return 0; }
        const int n = corners_of(p.flags);
        if (n < 1 || n > CORNERS) { exchange_fail(err, "polygon %u has %d corners", i, n); return 0; }
        q->flags = p.flags, q->unknown_1 = p.unknown_1, q->texture = p.texture, q->unknown_3 = p.unknown_3;
        q->has_uv = p.uv_slot != p.vertex_slot;
        if (!slots(m, p.vertex_slot, (uint32_t)n, q->vertex) || (q->has_uv && !slots(m, p.uv_slot, (uint32_t)n, q->uv))) {
            exchange_fail(err, "polygon %u's corners run past the vertex map", i);
            return 0;
        }
    }
    return 1;
}

static int describe_lists(const Mesh *m, MeshDesc *d, ExchangeError *err)
{
    for (uint32_t i = 0; i < m->section_a_count; i++) {
        MeshSectionA a;
        MeshDescPart *part = &d->parts[i];
        if (mesh_section_a(m, i, &a) != MESH_OK || (part->vertices = table(a.unknown[0], sizeof(uint32_t))) == NULL ||
            !slots(m, a.unknown[1], a.unknown[0], part->vertices)) {
            exchange_fail(err, "part %u's vertex list cannot be read", i);
            return 0;
        }
        memcpy(part->name, a.name, sizeof part->name);
        part->count = a.unknown[0], part->first_offset_vertex = a.unknown[2];
    }
    for (uint32_t i = 0; i < m->section_b_count; i++) {
        MeshSectionB b;
        MeshDescGroup *g = &d->groups[i];
        if (mesh_section_b(m, i, &b) != MESH_OK || (g->polygons = table(b.unknown[2], sizeof(uint32_t))) == NULL ||
            !slots(m, b.unknown[1], b.unknown[2], g->polygons)) {
            exchange_fail(err, "group %u's polygon list cannot be read", i);
            return 0;
        }
        memcpy(g->name, b.name, sizeof g->name);
        g->count = b.unknown[2], g->unknown_0 = b.unknown[0], g->unknown_3 = b.unknown[3];
    }
    return 1;
}

int mesh_describe(const Mesh *m, MeshDesc *d, ExchangeError *err)
{
    memset(d, 0, sizeof *d);
    memcpy(d->name, m->name, sizeof d->name);
    d->unknown_08 = m->unknown_08, d->real_vertices = m->unknown_14, d->unknown_64 = m->unknown_64;
    d->vertex_count = m->vertex_count, d->uv_count = m->uv_count, d->polygon_count = m->polygon_count;
    d->texture_count = m->texture_count, d->part_count = m->section_a_count, d->group_count = m->section_b_count;
    d->point_count = m->section_c_count, d->label_count = m->label_count;
    d->vertices = table(d->vertex_count, sizeof *d->vertices), d->uvs = table(d->uv_count, sizeof *d->uvs);
    d->polygons = table(d->polygon_count, sizeof *d->polygons), d->textures = table(d->texture_count, sizeof *d->textures);
    d->parts = table(d->part_count, sizeof *d->parts), d->groups = table(d->group_count, sizeof *d->groups);
    d->points = table(d->point_count, sizeof *d->points), d->labels = table(d->label_count, sizeof *d->labels);
    if (!d->vertices || !d->uvs || !d->polygons || !d->textures || !d->parts || !d->groups || !d->points || !d->labels) {
        mesh_desc_free(d);
        exchange_fail(err, "out of memory");
        return 0;
    }
    int ok = 1;
    for (uint32_t i = 0; ok && i < d->vertex_count; i++) ok = mesh_vertex(m, i, &d->vertices[i]) == MESH_OK;
    for (uint32_t i = 0; ok && i < d->uv_count; i++) ok = mesh_uv(m, i, &d->uvs[i]) == MESH_OK;
    for (uint32_t i = 0; ok && i < d->texture_count; i++) ok = mesh_texture(m, i, &d->textures[i]) == MESH_OK;
    for (uint32_t i = 0; ok && i < d->point_count; i++) ok = mesh_section_c(m, i, &d->points[i]) == MESH_OK;
    for (uint32_t i = 0; ok && i < d->label_count; i++) ok = mesh_label(m, i, &d->labels[i]) == MESH_OK;
    if (!ok) exchange_fail(err, "a table of the mesh cannot be read");
    ok = ok && describe_polygons(m, d, err) && describe_lists(m, d, err);
    if (!ok) mesh_desc_free(d);
    return ok;
}

/* ---- build */

static int check_polygons(const MeshDesc *d, ExchangeError *err)
{
    for (uint32_t i = 0; i < d->polygon_count; i++) {
        const MeshDescPolygon *p = &d->polygons[i];
        const int n = corners_of(p->flags);
        if (n != 3 && n != 4) { exchange_fail(err, "polygon %u has %d corners (3 or 4)", i, n); return 0; }
        if (p->texture >= d->texture_count) {
            exchange_fail(err, "polygon %u names texture %u, past the %u references", i, p->texture, d->texture_count);
            return 0;
        }
        for (int k = 0; k < n; k++) {
            if (p->vertex[k] >= d->vertex_count) {
                exchange_fail(err, "polygon %u's corner %d names vertex %u, past the %u vertices", i, k, p->vertex[k], d->vertex_count);
                return 0;
            }
            if (p->has_uv && p->uv[k] >= d->uv_count) {
                exchange_fail(err, "polygon %u's corner %d names uv %u, past the %u uvs", i, k, p->uv[k], d->uv_count);
                return 0;
            }
        }
    }
    return 1;
}

/* the name of a record as text, for a message (up to its NUL, within its field) */
static void shown(char *out, const char *field, size_t size)
{
    size_t n = 0;
    while (n < size && field[n] != '\0') n++;
    memcpy(out, field, n);
    out[n] = '\0';
}

static int check_groups(const MeshDesc *d, ExchangeError *err)
{
    uint8_t *seen = calloc(d->polygon_count, 1);
    if (seen == NULL) { exchange_fail(err, "out of memory"); return 0; }
    int ok = 1;
    for (uint32_t i = 0; ok && i < d->group_count; i++) {
        const MeshDescGroup *g = &d->groups[i];
        char name[MESH_NAME_SIZE + 1];
        shown(name, g->name, MESH_NAME_SIZE);
        for (uint32_t j = 0; ok && j < i; j++)
            if (strncmp(d->groups[j].name, g->name, MESH_NAME_SIZE) == 0) exchange_fail(err, "two groups are named '%s'", name), ok = 0;
        memset(seen, 0, d->polygon_count);
        for (uint32_t k = 0; ok && k < g->count; k++) {
            const uint32_t p = g->polygons[k];
            if (p >= d->polygon_count) exchange_fail(err, "group '%s' names polygon %u, past the %u polygons", name, p, d->polygon_count), ok = 0;
            else if (seen[p]++) exchange_fail(err, "group '%s' names polygon %u twice", name, p), ok = 0;
        }
    }
    free(seen);
    return ok;
}

static int check_rest(const MeshDesc *d, ExchangeError *err)
{
    for (uint32_t i = 0; i < d->part_count; i++) {
        const MeshDescPart *p = &d->parts[i];
        char name[MESH_PART_NAME_SIZE + 1];
        shown(name, p->name, MESH_PART_NAME_SIZE);
        if ((uint64_t)p->first_offset_vertex + p->count > d->vertex_count) {
            exchange_fail(err, "part '%s''s %u offsets from vertex %u run past the %u vertices", name, p->count, p->first_offset_vertex, d->vertex_count);
            return 0;
        }
        for (uint32_t k = 0; k < p->count; k++)
            if (p->vertices[k] >= d->vertex_count) {
                exchange_fail(err, "part '%s' names vertex %u, past the %u vertices", name, p->vertices[k], d->vertex_count);
                return 0;
            }
    }
    for (uint32_t i = 0; i < d->point_count; i++)
        if (d->points[i].value >= d->vertex_count) {
            exchange_fail(err, "point %u names vertex %u, past the %u vertices", i, d->points[i].value, d->vertex_count);
            return 0;
        }
    for (uint32_t i = 0; i < d->label_count; i++)
        if (d->labels[i].polygon >= d->polygon_count) {
            exchange_fail(err, "label %u names polygon %u, past the %u polygons", i, d->labels[i].polygon, d->polygon_count);
            return 0;
        }
    return 1;
}

static int check(const MeshDesc *d, ExchangeError *err)
{
    if (d->vertex_count == 0) { exchange_fail(err, "a mesh needs a vertex"); return 0; }
    if (d->polygon_count == 0) { exchange_fail(err, "a mesh needs a polygon"); return 0; }
    if (d->texture_count > MESH_MAX_TEXTURES) {
        exchange_fail(err, "%u texture references, more than the %d a polygon's byte can name", d->texture_count, MESH_MAX_TEXTURES);
        return 0;
    }
    if (d->real_vertices > d->vertex_count) {
        exchange_fail(err, "real_vertices %u is past the %u vertices", d->real_vertices, d->vertex_count);
        return 0;
    }
    return check_polygons(d, err) && check_groups(d, err) && check_rest(d, err);
}

/* the map's slots: the polygons' runs, then the parts' and the groups' lists */
static uint64_t map_slots(const MeshDesc *d)
{
    uint64_t n = 0;
    for (uint32_t i = 0; i < d->polygon_count; i++) n += (uint64_t)corners_of(d->polygons[i].flags) * (d->polygons[i].has_uv ? 2 : 1);
    for (uint32_t i = 0; i < d->part_count; i++) n += d->parts[i].count;
    for (uint32_t i = 0; i < d->group_count; i++) n += d->groups[i].count;
    return n;
}

typedef struct {
    uint32_t polygon, uv, texture, part, group, point, label, vertex, map, end;
} Offsets;

static Offsets offsets_of(const MeshDesc *d, uint64_t slots_count, int *fits)
{
    Offsets o;
    uint64_t at = HEADER_SIZE;
    o.polygon = (uint32_t)at, at += (uint64_t)d->polygon_count * POLYGON_SIZE;
    o.uv = (uint32_t)at, at += (uint64_t)d->uv_count * UV_SIZE;
    o.texture = (uint32_t)at, at += (uint64_t)d->texture_count * TEXTURE_SIZE;
    o.part = (uint32_t)at, at += (uint64_t)d->part_count * PART_SIZE;
    o.group = (uint32_t)at, at += (uint64_t)d->group_count * GROUP_SIZE;
    o.point = (uint32_t)at, at += (uint64_t)d->point_count * POINT_SIZE;
    o.label = (uint32_t)at, at += (uint64_t)d->label_count * LABEL_SIZE;
    o.vertex = (uint32_t)at, at += (uint64_t)d->vertex_count * VERTEX_SIZE;
    o.map = (uint32_t)at, at += slots_count * SLOT_SIZE;
    o.end = (uint32_t)at;
    *fits = at <= UINT32_MAX;
    return o;
}

static void write_header(uint8_t *b, const MeshDesc *d, const Offsets *o)
{
    memcpy(b, "ORIP", 4);
    put32(b + 0x04, o->end), put32(b + 0x08, d->unknown_08), put32(b + 0x0c, 0);
    put32(b + 0x10, d->vertex_count), put32(b + 0x14, d->real_vertices), put32(b + 0x18, o->vertex);
    put32(b + 0x1c, d->uv_count), put32(b + 0x20, o->uv), put32(b + 0x24, d->polygon_count), put32(b + 0x28, o->polygon);
    memcpy(b + 0x2c, d->name, MESH_NAME_SIZE);
    put32(b + 0x38, d->texture_count), put32(b + 0x3c, o->texture), put32(b + 0x40, d->part_count), put32(b + 0x44, o->part);
    put32(b + 0x48, d->group_count), put32(b + 0x4c, o->group), put32(b + 0x50, o->map), put32(b + 0x54, d->point_count);
    put32(b + 0x58, o->point), put32(b + 0x5c, d->label_count), put32(b + 0x60, o->label), put32(b + 0x64, d->unknown_64);
    put32(b + 0x68, o->polygon), put32(b + 0x6c, 0);
}

static void write_tables(uint8_t *b, const MeshDesc *d, const Offsets *o)
{
    for (uint32_t i = 0; i < d->uv_count; i++) put32(b + o->uv + 8 * i, (uint32_t)d->uvs[i].u), put32(b + o->uv + 8 * i + 4, (uint32_t)d->uvs[i].v);
    for (uint32_t i = 0; i < d->texture_count; i++) {
        uint8_t *t = b + o->texture + TEXTURE_SIZE * i;
        memcpy(t, d->textures[i].label, 8), memcpy(t + 8, d->textures[i].name, 4);
        put32(t + 12, d->textures[i].unknown_1[0]), put32(t + 16, d->textures[i].unknown_1[1]);
    }
    for (uint32_t i = 0; i < d->point_count; i++) {
        memcpy(b + o->point + POINT_SIZE * i, d->points[i].name, 8);
        put32(b + o->point + POINT_SIZE * i + 8, d->points[i].value);
    }
    for (uint32_t i = 0; i < d->label_count; i++) {
        memcpy(b + o->label + LABEL_SIZE * i, d->labels[i].name, 8);
        put32(b + o->label + LABEL_SIZE * i + 8, d->labels[i].polygon);
    }
    for (uint32_t i = 0; i < d->vertex_count; i++) {
        uint8_t *v = b + o->vertex + VERTEX_SIZE * i;
        put32(v, (uint32_t)d->vertices[i].x), put32(v + 4, (uint32_t)d->vertices[i].y), put32(v + 8, (uint32_t)d->vertices[i].z);
    }
}

/* the polygons with their runs in the map, then the parts and groups with their lists */
static void write_map(uint8_t *b, const MeshDesc *d, const Offsets *o)
{
    uint32_t slot = 0;
    uint8_t *map = b + o->map;
    for (uint32_t i = 0; i < d->polygon_count; i++) {
        const MeshDescPolygon *p = &d->polygons[i];
        const uint32_t n = (uint32_t)corners_of(p->flags);
        uint8_t *r = b + o->polygon + POLYGON_SIZE * i;
        r[0] = p->flags, r[1] = p->unknown_1, r[2] = p->texture, r[3] = p->unknown_3;
        const uint32_t uv_slot = slot;
        if (p->has_uv)
            for (uint32_t k = 0; k < n; k++) put32(map + SLOT_SIZE * slot++, p->uv[k]);
        const uint32_t vertex_slot = slot;
        for (uint32_t k = 0; k < n; k++) put32(map + SLOT_SIZE * slot++, p->vertex[k]);
        put32(r + 4, vertex_slot), put32(r + 8, p->has_uv ? uv_slot : vertex_slot);
    }
    for (uint32_t i = 0; i < d->part_count; i++) {
        const MeshDescPart *p = &d->parts[i];
        uint8_t *r = b + o->part + PART_SIZE * i;
        memcpy(r, p->name, MESH_PART_NAME_SIZE);
        put32(r + 8, p->count), put32(r + 12, slot), put32(r + 16, p->first_offset_vertex);
        for (uint32_t k = 0; k < p->count; k++) put32(map + SLOT_SIZE * slot++, p->vertices[k]);
    }
    for (uint32_t i = 0; i < d->group_count; i++) {
        const MeshDescGroup *g = &d->groups[i];
        uint8_t *r = b + o->group + GROUP_SIZE * i;
        memcpy(r, g->name, MESH_NAME_SIZE);
        put32(r + 12, g->unknown_0), put32(r + 16, slot), put32(r + 20, g->count), put32(r + 24, g->unknown_3);
        for (uint32_t k = 0; k < g->count; k++) put32(map + SLOT_SIZE * slot++, g->polygons[k]);
    }
}

int mesh_build(const MeshDesc *d, uint8_t **out, size_t *out_len, ExchangeError *err)
{
    *out = NULL, *out_len = 0;
    if (!check(d, err)) return 0;
    int fits;
    const Offsets o = offsets_of(d, map_slots(d), &fits);
    if (!fits) { exchange_fail(err, "the mesh would pass 4 GB"); return 0; }
    uint8_t *b = calloc(o.end, 1);
    if (b == NULL) { exchange_fail(err, "out of memory"); return 0; }
    write_header(b, d, &o);
    write_tables(b, d, &o);
    write_map(b, d, &o);
    *out = b, *out_len = o.end;
    return 1;
}
