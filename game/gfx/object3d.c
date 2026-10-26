#include "game/gfx/object3d.h"

#include "game/sys/fixed_math.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static const int32_t IDENTITY[9] = { 0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x10000 };

static int load_polygon(const Mesh *mesh, uint32_t index, Object3DPolygon *out)
{
    MeshPolygon p;
    uint32_t vertices[4], uvs[4];
    if (mesh_polygon(mesh, index, &p) != MESH_OK || mesh_polygon_corners(mesh, &p, vertices, uvs) != MESH_OK)
        return 0;
    /* The game reads the record as one word: flags, unknown_1, texture, unknown_3. */
    out->flags = p.flags | (uint32_t)p.unknown_1 << 8 | (uint32_t)p.texture << 16 | (uint32_t)p.unknown_3 << 24;
    const int corners = (int)(out->flags & POLY_CORNERS);
    for (int i = 0; i < 4; i++) {
        out->vertex[i] = vertices[i];
        out->uv[i] = p.uv_slot != p.vertex_slot ? uvs[i] : UINT32_MAX;
        if (i < corners && vertices[i] >= mesh->vertex_count) return 0;
    }
    out->link = UINT32_MAX;
    if ((out->flags & POLY_LINKED) && mesh_vertex_map(mesh, p.vertex_slot + (uint32_t)corners, &out->link) != MESH_OK)
        out->link = UINT32_MAX;
    return 1;
}

static int load_textures(Object3D *obj)
{
    for (uint32_t i = 0; i < obj->texture_count; i++) {
        MeshTexture t;
        if (mesh_texture(obj->mesh, i, &t) != MESH_OK) return 0;
        /* 00490cc0 -> 00490c38: the colour bytes r, g, b of +0xc */
        obj->textures[i].rgb = t.unknown_1[0];
        obj->textures[i].texture = NULL;
    }
    return 1;
}

enum { LABEL_SIZE = 8 };

/* 004a2da2 over n bytes: equal up to n or a NUL, A..Z as a..z. */
static int labels_match(const char *a, const char *b, int n)
{
    for (; n > 0; n--, a++, b++) {
        unsigned char x = (unsigned char)*a, y = (unsigned char)*b;
        if (x > 0x40 && x < 0x5b) x += 0x20;
        if (y > 0x40 && y < 0x5b) y += 0x20;
        if (x != y) return 0;
        if (y == 0) return 1;
    }
    return 1;
}

int32_t object3d_texture_named(const Object3D *obj, const char *label)
{
    const int32_t count = (int32_t)obj->mesh->texture_count;
    for (int32_t i = 0; i < count; i++) {
        MeshTexture t;
        if (mesh_texture(obj->mesh, (uint32_t)i, &t) != MESH_OK) return -1;  /* port guard */
        if (labels_match(t.label, label, LABEL_SIZE)) return i;
    }
    return -1;
}

int32_t object3d_part_named(const Object3D *obj, const char *name)
{
    const int32_t count = (int32_t)obj->mesh->section_a_count;
    for (int32_t i = 0; i < count; i++) {
        MeshSectionA a;
        if (mesh_section_a(obj->mesh, (uint32_t)i, &a) != MESH_OK) return -1;  /* port guard */
        if (labels_match(a.name, name, LABEL_SIZE)) return i;
    }
    return -1;
}

int32_t object3d_point_named(const Object3D *obj, const char *name)
{
    const int32_t count = (int32_t)obj->mesh->section_c_count;
    for (int32_t i = 0; i < count; i++) {
        MeshSectionC c;
        if (mesh_section_c(obj->mesh, (uint32_t)i, &c) != MESH_OK) return -1;  /* port guard */
        if (labels_match(c.name, name, LABEL_SIZE)) return (int32_t)c.value;
    }
    return -1;
}

int32_t object3d_label_named(const Object3D *obj, const char *name)
{
    const int32_t count = (int32_t)obj->mesh->label_count;
    for (int32_t i = 0; i < count; i++) {
        MeshLabel l;
        if (mesh_label(obj->mesh, (uint32_t)i, &l) != MESH_OK) return -1;  /* port guard */
        if (labels_match(l.name, name, LABEL_SIZE)) return (int32_t)l.polygon;
    }
    return -1;
}

void object3d_set_texture(Object3D *obj, int32_t slot, const FshEntry *texture)
{
    if ((uint32_t)slot < obj->texture_count) obj->textures[slot].texture = texture;
}

int object3d_match_colours(Object3D *obj, const PaletteMatch *m)
{
    for (uint32_t slot = 0; slot < obj->texture_count; slot++) {
        MeshTexture t;
        if (mesh_texture(obj->mesh, slot, &t) != MESH_OK) return 0;
        const uint32_t word = t.unknown_1[0];
        obj->textures[slot].rgb = word;
        obj->textures[slot].colour = (uint32_t)palette_match_rgb(m, word & 0xFFFFFFu);
    }
    return 1;
}

/* 004917f7 with -1: every polygon, unless polygon 0 has POLY_NO_CULL. The
 * original tests the first record's flag on every pass of its loop (the
 * record pointer is never advanced), so the list is all or nothing. */
static void build_order(Object3D *obj)
{
    obj->order_count = 0;
    if (obj->polygon_count == 0 || (obj->polygons[0].flags & 0x400)) return;
    for (uint32_t i = 0; i < obj->polygon_count; i++) obj->order[obj->order_count++] = i;
}

int object3d_create(Object3D *obj, const Mesh *mesh)
{
    memset(obj, 0, sizeof *obj);
    obj->mesh = mesh;
    const uint32_t n = mesh->vertex_count;
    obj->model = calloc(n ? n : 1, sizeof *obj->model);
    obj->camera = calloc(n ? n : 1, sizeof *obj->camera);
    obj->screen = calloc(n ? n : 1, sizeof *obj->screen);
    obj->polygons = calloc(mesh->polygon_count ? mesh->polygon_count : 1, sizeof *obj->polygons);
    obj->order = calloc(mesh->polygon_count ? mesh->polygon_count : 1, sizeof *obj->order);
    obj->textures = calloc(mesh->texture_count ? mesh->texture_count : 1, sizeof *obj->textures);
    if (!obj->model || !obj->camera || !obj->screen || !obj->polygons || !obj->order || !obj->textures) {
        object3d_free(obj);
        return 0;
    }
    obj->vertex_count = n;
    obj->polygon_count = mesh->polygon_count;
    obj->texture_count = mesh->texture_count;
    for (uint32_t i = 0; i < n; i++) {
        MeshVertex v;
        if (mesh_vertex(mesh, i, &v) != MESH_OK) {
            object3d_free(obj);
            return 0;
        }
        obj->model[i][0] = v.x, obj->model[i][1] = v.y, obj->model[i][2] = v.z;
    }
    for (uint32_t i = 0; i < obj->polygon_count; i++) {
        if (!load_polygon(mesh, i, &obj->polygons[i])) {
            object3d_free(obj);
            return 0;
        }
    }
    if (!load_textures(obj)) {
        object3d_free(obj);
        return 0;
    }
    /* 0048d798 keeps 0x40 clear; 0048da62 with 004f47d0 marks the matrix. */
    object3d_set_matrix(obj, IDENTITY);
    build_order(obj);
    return 1;
}

void object3d_mirror(Object3D *obj)
{
    for (uint32_t i = 0; i < obj->order_count && i < obj->polygon_count; i++) obj->polygons[i].flags ^= POLY_REVERSED;
}

void object3d_free(Object3D *obj)
{
    free(obj->model);
    free(obj->camera);
    free(obj->screen);
    free(obj->polygons);
    free(obj->order);
    free(obj->textures);
    free(obj->normal);
    memset(obj, 0, sizeof *obj);
}

int object3d_bind(Object3D *obj, const Fsh *fsh)
{
    int bound = 0;
    for (uint32_t i = 0; i < obj->texture_count; i++) {
        MeshTexture t;
        uint32_t index;
        if (mesh_texture(obj->mesh, i, &t) != MESH_OK) continue;
        if (fsh_find(fsh, t.name, &index) != FSH_OK) continue;
        if (fsh_entry(fsh, index, &obj->textures[i].entry) != FSH_OK) continue;
        obj->textures[i].texture = &obj->textures[i].entry;
        bound++;
    }
    return bound;
}

void object3d_set_matrix(Object3D *obj, const int32_t matrix[9])
{
    memcpy(obj->matrix, matrix, sizeof obj->matrix);
    obj->flags |= OBJECT3D_FLAG_MATRIX;
}

void object3d_transform(Object3D *obj, const int32_t camera_matrix[9], const int32_t camera_position[3],
                        int recombine_always)
{
    if ((obj->flags & OBJECT3D_FLAG_MATRIX) || recombine_always) {
        mat3_mul(obj->matrix, camera_matrix, obj->combined);
        obj->flags &= ~(uint32_t)OBJECT3D_FLAG_MATRIX;
    }
    int32_t rel[3], offset[3];
    for (int i = 0; i < 3; i++) rel[i] = (int32_t)((uint32_t)obj->position[i] - (uint32_t)camera_position[i]);
    mat3_apply(rel, camera_matrix, offset);
    /* 0048cf60 then 0048cec0 */
    for (uint32_t v = 0; v < obj->vertex_count; v++) {
        int32_t r[3];
        mat3_apply(obj->model[v], obj->combined, r);
        for (int i = 0; i < 3; i++) obj->camera[v][i] = (int32_t)((uint32_t)r[i] + (uint32_t)offset[i]);
    }
}

void object3d_project(Object3D *obj, const View *view)
{
    /* 00491690's other path (+0x10 & 0x20: 004a30c6, 16.16 then rounded) is for objects created
     * with flag 4; the race creates them with 0 and 1 only (004576f0, 00457d40). */
    for (uint32_t v = 0; v < obj->vertex_count; v++) view_project(view, obj->camera[v], obj->screen[v]);
}

int object3d_enable_normals(Object3D *obj)
{
    if (obj->normal != NULL) return 1;
    obj->normal = calloc(obj->vertex_count ? obj->vertex_count : 1, sizeof *obj->normal);
    return obj->normal != NULL;
}

static int32_t winding_camera(const Object3D *obj, const Object3DPolygon *p);

void object3d_update_normals(Object3D *obj)
{
    if (obj->normal == NULL) return;
    memset(obj->normal, 0, (obj->vertex_count ? obj->vertex_count : 1) * sizeof *obj->normal);
    for (uint32_t i = 0; i < obj->polygon_count; i++) {
        const Object3DPolygon *p = &obj->polygons[i];
        const int corners = (int)(p->flags & POLY_CORNERS);
        if (corners < 3 || corners > 4 || p->vertex[0] >= obj->vertex_count || p->vertex[1] >= obj->vertex_count ||
            p->vertex[2] >= obj->vertex_count || (corners == 4 && p->vertex[3] >= obj->vertex_count))
            continue;
        if (!(p->flags & (POLY_TWO_SIDED | POLY_NO_CULL)) && ((p->flags & POLY_REVERSED) == 0) != (winding_camera(obj, p) > 0)) continue;
        const int32_t *a = obj->camera[p->vertex[0]], *b = obj->camera[p->vertex[1]], *c = obj->camera[p->vertex[2]];
        const double u[3] = { (double)b[0] - a[0], (double)b[1] - a[1], (double)b[2] - a[2] },
                     w[3] = { (double)c[0] - a[0], (double)c[1] - a[1], (double)c[2] - a[2] };
        double n[3] = { u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0] };
        const double len = sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (len < 1e-9) continue;
        double toward = n[0] * a[0] + n[1] * a[1] + n[2] * a[2];   /* the eye is the origin: a normal on the eye's side has this negative */
        const double sign = toward > 0 ? -1.0 : 1.0;
        for (int k = 0; k < 3; k++) n[k] = sign * n[k] / len;
        for (int k = 0; k < corners; k++)
            for (int m = 0; m < 3; m++) obj->normal[p->vertex[k]][m] += (float)n[m];
    }
    for (uint32_t v = 0; v < obj->vertex_count; v++) {
        float *n = obj->normal[v];
        const float len = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (len > 1e-6f) n[0] /= len, n[1] /= len, n[2] /= len;
        else n[0] = n[1] = n[2] = 0;
    }
}

/* 004a3110: Shell sort, largest key first, the order list moved with it. */
static void shell_sort(int n, int32_t *keys, uint32_t *order)
{
    for (int gap = n >> 1; gap >= 1; gap >>= 1) {
        for (int i = gap; i < n; i++) {
            for (int j = i - gap; j >= 0; j -= gap) {
                if (keys[j + gap] <= keys[j]) break;
                const int32_t k = keys[j];
                keys[j] = keys[j + gap];
                keys[j + gap] = k;
                const uint32_t o = order[j];
                order[j] = order[j + gap];
                order[j + gap] = o;
            }
        }
    }
}

/* 004918e1: the key is the depth of the first two corners. */
void object3d_sort_list(const Object3D *obj, Object3DList *list)
{
    static int32_t keys[OBJECT3D_MAX_SORTED];
    const int n = list->count > OBJECT3D_MAX_SORTED ? OBJECT3D_MAX_SORTED : (int)list->count;
    for (int i = 0; i < n; i++) {
        uint32_t z = 0;  /* a polygon or corner index out of range counts as depth 0 */
        if (list->order[i] < obj->polygon_count) {
            const Object3DPolygon *p = &obj->polygons[list->order[i]];
            for (int k = 0; k < 2; k++)
                if (p->vertex[k] < obj->vertex_count) z += (uint32_t)obj->camera[p->vertex[k]][2];
        }
        keys[i] = (int32_t)z;
    }
    shell_sort(n, keys, list->order);
}

void object3d_sort(Object3D *obj)
{
    Object3DList all = { obj->order_count, obj->order };
    object3d_sort_list(obj, &all);
}

/* 004a31df: some corner on or in front of the near plane. */
static int touches_near(const Object3D *obj, const Object3DPolygon *p, int corners, int32_t near_z)
{
    for (int i = 0; i < corners; i++)
        if (obj->camera[p->vertex[i]][2] <= near_z) return 1;
    return 0;
}

/* 004a335b -> 0048f750: twice the signed screen area of the first three
 * corners, 32-bit as the original. */
static int32_t winding(const Object3D *obj, const Object3DPolygon *p)
{
    const int32_t *a = obj->screen[p->vertex[0]], *b = obj->screen[p->vertex[1]], *c = obj->screen[p->vertex[2]];
    const uint32_t x2 = (uint32_t)c[0] - (uint32_t)b[0], y2 = (uint32_t)c[1] - (uint32_t)b[1];
    const uint32_t x0 = (uint32_t)a[0] - (uint32_t)b[0], y0 = (uint32_t)a[1] - (uint32_t)b[1];
    return (int32_t)(y2 * x0 - x2 * y0);
}

/* port: the sign 004a335b's winding has for corners all in front of the eye, from the camera-space corners, where
 * it also holds for corners behind it: the corner points' triple product (A x B) . C. */
static int32_t winding_camera(const Object3D *obj, const Object3DPolygon *p)
{
    const int32_t *a = obj->camera[p->vertex[0]], *b = obj->camera[p->vertex[1]], *c = obj->camera[p->vertex[2]];
    const double det = (double)a[0] * ((double)b[1] * c[2] - (double)b[2] * c[1]) -
                       (double)a[1] * ((double)b[0] * c[2] - (double)b[2] * c[0]) +
                       (double)a[2] * ((double)b[0] * c[1] - (double)b[1] * c[0]);
    return det > 0 ? 1 : det < 0 ? -1 : 0;
}

static Object3DTable table_for(uint32_t flags, int corners, int alt)
{
    if (corners == 3) return OBJECT3D_TABLE_TRIANGLE;
    const int split = alt || (flags & POLY_TRIANGLES);
    if (flags & POLY_UV) return split ? OBJECT3D_TABLE_QUAD_UV_TRIANGLES : OBJECT3D_TABLE_QUAD_UV;
    return split ? OBJECT3D_TABLE_QUAD_WHOLE_TRIANGLES : OBJECT3D_TABLE_QUAD_WHOLE;
}

static void gather(const Object3D *obj, const Object3DPolygon *p, int corners, Object3DCorners *c, float normal[12])
{
    memset(c, 0, sizeof *c);
    c->corners = corners;
    for (int i = 0; i < corners && i < 4; i++) {
        c->screen[2 * i] = obj->screen[p->vertex[i]][0];
        c->screen[2 * i + 1] = obj->screen[p->vertex[i]][1];
        for (int k = 0; k < 3; k++) c->camera[3 * i + k] = obj->camera[p->vertex[i]][k];
        if (obj->normal != NULL && p->vertex[i] < obj->vertex_count) memcpy(normal + 3 * i, obj->normal[p->vertex[i]], 3 * sizeof *normal);
        MeshUv uv;
        if (p->uv[i] != UINT32_MAX && mesh_uv(obj->mesh, p->uv[i], &uv) == MESH_OK) {
            c->uv[2 * i] = uv.u;
            c->uv[2 * i + 1] = uv.v;
        }
    }
}

/* 00491a82 */
static void draw_polygon(const Object3D *obj, uint32_t index, int32_t near_z, const Object3DSink *sink, int depth)
{
    if (index >= obj->polygon_count || depth > (int)obj->polygon_count) return;
    const Object3DPolygon *p = &obj->polygons[index];
    const uint32_t flags = p->flags;
    const int corners = (int)(flags & POLY_CORNERS);
    if (corners < 1 || corners > 4) return;
    const uint32_t slot = (flags >> 16) & 0xFF;
    const Object3DTexture *t = slot < obj->texture_count ? &obj->textures[slot] : NULL;
    const int textured = (flags & POLY_TEXTURED) && t != NULL && t->texture != NULL;
    const int camera = corners >= 3 && (textured ? sink->textured_camera != NULL : sink->flat_camera != NULL);
    if (!camera && (obj->flags & OBJECT3D_FLAG_NEAR) && touches_near(obj, p, corners, near_z)) return;
    if (!(flags & (POLY_TWO_SIDED | POLY_NO_CULL))) {
        /* A line still takes its winding from a third slot of its run. */
        if (corners < 3 && (p->vertex[1] >= obj->vertex_count || p->vertex[2] >= obj->vertex_count)) return;
        const int32_t facing = camera ? winding_camera(obj, p) : winding(obj, p);
        if (((flags & POLY_REVERSED) == 0) != (facing > 0)) return;
    }
    Object3DCorners c;
    float normal[12] = { 0 };
    gather(obj, p, corners, &c, normal);
    if (obj->normal != NULL) c.normal = normal;
    if (textured) {
        const int mode = (int)((flags >> 3) & 0xF);
        const Object3DTable table = table_for(flags, corners, (obj->flags & OBJECT3D_FLAG_ALT_MAPPERS) != 0);
        if (camera) sink->textured_camera(sink->user, table, mode, t->texture, &c);
        else sink->textured(sink->user, table, mode, t->texture, &c);
    } else if (camera) {
        sink->flat_camera(sink->user, t != NULL ? t->rgb : 0, &c);
    } else if (corners >= 2) {
        sink->flat(sink->user, t != NULL ? t->rgb : 0, &c);
    }
    if (flags & POLY_LINKED) draw_polygon(obj, p->link, near_z, sink, depth + 1);
}

void object3d_draw_list(const Object3D *obj, const Object3DList *list, int32_t near_z, const Object3DSink *sink)
{
    for (uint32_t i = 0; i < list->count; i++) draw_polygon(obj, list->order[i], near_z, sink, 0);
}

void object3d_draw(const Object3D *obj, int32_t near_z, const Object3DSink *sink)
{
    const Object3DList all = { obj->order_count, obj->order };
    object3d_draw_list(obj, &all, near_z, sink);
}

int object3d_section(const Object3D *obj, const char *name)
{
    for (uint32_t i = 0; i < obj->mesh->section_b_count; i++) {
        MeshSectionB b;
        if (mesh_section_b(obj->mesh, i, &b) == MESH_OK && strncmp(b.name, name, 8) == 0) return (int)i;
    }
    return -1;
}

int object3d_section_list(const Object3D *obj, int section, Object3DList *out)
{
    MeshSectionB b;
    out->count = 0;
    out->order = NULL;
    if (section < 0 || mesh_section_b(obj->mesh, (uint32_t)section, &b) != MESH_OK) return 0;
    const uint32_t start = b.unknown[1], count = b.unknown[2];
    out->order = calloc(count ? count : 1, sizeof *out->order);
    if (out->order == NULL) return 0;
    for (uint32_t i = 0; i < count; i++) {
        if (mesh_vertex_map(obj->mesh, start + i, &out->order[i]) != MESH_OK) {
            free(out->order);
            out->order = NULL;
            return 0;
        }
    }
    out->count = count;
    return 1;
}

void object3d_morph_part(Object3D *obj, int32_t part, int32_t weight)
{
    const Mesh *mesh = obj->mesh;
    MeshSectionA sa;
    if (part < 0 || (uint32_t)part >= mesh->section_a_count || mesh_section_a(mesh, (uint32_t)part, &sa) != MESH_OK)
        return;
    for (uint32_t k = 0; k < sa.unknown[0]; k++) {
        uint32_t v;
        MeshVertex base, offset;
        if (mesh_vertex_map(mesh, sa.unknown[1] + k, &v) != MESH_OK || v >= obj->vertex_count ||
            mesh_vertex(mesh, v, &base) != MESH_OK)
            return;  /* port guard: past the mesh's tables */
        int32_t *out = obj->model[v];
        if (weight == 0) {
            out[0] = base.x, out[1] = base.y, out[2] = base.z;
            continue;
        }
        if (mesh_vertex(mesh, sa.unknown[2] + k, &offset) != MESH_OK) return;
        if (weight != 0x10000) {
            offset.x = fx_mul_round(offset.x, weight);
            offset.y = fx_mul_round(offset.y, weight);
            offset.z = fx_mul_round(offset.z, weight);
        }
        out[0] = (int32_t)((uint32_t)base.x + (uint32_t)offset.x);
        out[1] = (int32_t)((uint32_t)base.y + (uint32_t)offset.y);
        out[2] = (int32_t)((uint32_t)base.z + (uint32_t)offset.z);
    }
}
