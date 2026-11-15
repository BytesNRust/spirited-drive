/* The car importer's mesh from an author's OBJ (formats/car_import.h): objects by name, Blender's frame turned into the mesh's,
 * the flag bytes from the donor, the points and lights the OBJ lacks made. */
#include "formats/car_import_internal.h"
#include "formats/car_steer.h"
#include "formats/car_texture.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { WHEELS = 4, POINTS = 9, LIGHTS = 4, GROUPS = 3, TYRE_REFS = 6, REVERSED = 0x02 };
typedef enum { KIND_SURFACE, KIND_INSIDE, KIND_OUTSIDE, KIND_WHEEL, KIND_POINT, KIND_LIGHT } Kind;

static const char *const WHEEL_NAMES[WHEELS] = { "wheel_fl", "wheel_fr", "wheel_rl", "wheel_rr" };
static const char *const POINT_NAMES[POINTS] = { "smok", "fl0", "fl1", "fr0", "fr1", "rl0", "rl1", "rr0", "rr1" };
static const char *const LIGHT_NAMES[LIGHTS] = { "lt_frnt", "rt_frnt", "lt_rear", "rt_rear" };
static const char *const GROUP_NAMES[GROUPS] = { "inside", "surface", "outside" };
static const char *const TYRE_LABELS[TYRE_REFS] = { "frntires", "rertires", "tire1", "tire2", "tire3", "tire4" };

typedef struct {
    const CarImportIn *in;
    const ObjModel *obj;
    const MeshDesc *donor;
    int level;
    Kind *kind;                       /* per object */
    int wheel[WHEELS], point[POINTS], light[LIGHTS];   /* the object, or -1 */
    int32_t *material_ref;            /* per material: its texture reference, or -1 */
    uint32_t *face_polygon;           /* per face: its first polygon */
    uint8_t *polygon_group;           /* per polygon: GROUP_NAMES index */
    uint8_t *donor_group;             /* per donor polygon: GROUP_NAMES index, or GROUPS */
    MeshDesc *out;
    CarImportBank *bank;
    ExchangeError *err;
} Build;

/* `name` is `want`, or `want` then '.' and Blender's number, case aside */
static int named(const char *name, const char *want, int prefix)
{
    const size_t n = strlen(want);
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)name[i]) != want[i]) return 0;
    return prefix || name[n] == '\0' || name[n] == '.';
}

static int claim(Build *b, int *slot, uint32_t object, const char *what)
{
    if (*slot >= 0) {
        exchange_fail(b->err, "two objects are %s: '%s' and '%s'", what, b->obj->objects[*slot].name, b->obj->objects[object].name);
        return 0;
    }
    *slot = (int)object;
    return 1;
}

static int classify_one(Build *b, uint32_t o)
{
    const char *name = b->obj->objects[o].name;
    b->kind[o] = named(name, "inside", 1) ? KIND_INSIDE : named(name, "outside", 1) ? KIND_OUTSIDE : KIND_SURFACE;
    if (b->level != 0) return 1;
    for (int w = 0; w < WHEELS; w++)
        if (named(name, WHEEL_NAMES[w], 1)) return b->kind[o] = KIND_WHEEL, claim(b, &b->wheel[w], o, WHEEL_NAMES[w]);
    for (int p = 0; p < POINTS && b->obj->objects[o].face_count == 0; p++)
        if (named(name, POINT_NAMES[p], 0)) return b->kind[o] = KIND_POINT, claim(b, &b->point[p], o, POINT_NAMES[p]);
    for (int l = 0; l < LIGHTS; l++)
        if (named(name, LIGHT_NAMES[l], 0)) return b->kind[o] = KIND_LIGHT, claim(b, &b->light[l], o, LIGHT_NAMES[l]);
    return 1;
}

static int classify(Build *b)
{
    for (int k = 0; k < WHEELS; k++) b->wheel[k] = -1;
    for (int k = 0; k < POINTS; k++) b->point[k] = -1;
    for (int k = 0; k < LIGHTS; k++) b->light[k] = -1;
    for (uint32_t o = 0; o < b->obj->object_count; o++)
        if (!classify_one(b, o)) return 0;
    if (b->level != 0) return 1;
    for (int w = 0; w < 2; w++)
        if (b->wheel[w] < 0) { exchange_fail(b->err, "no object %s: the steered %s front wheel", WHEEL_NAMES[w], w ? "right" : "left"); return 0; }
    for (int w = 2; w < WHEELS; w++)
        if (b->wheel[w] < 0) ci_note(b->in, "no object %s: the car has no %s rear wheel", WHEEL_NAMES[w], w == 2 ? "left" : "right");
    return 1;
}

/* Blender's frame to the mesh's: z negated, scaled */
static int vertices(Build *b)
{
    MeshDesc *m = b->out;
    if (!ci_grow((void **)&m->vertices, b->obj->vertex_count + 1, sizeof *m->vertices, b->err)) return 0;
    for (uint32_t i = 0; i < b->obj->vertex_count; i++) {
        const ObjVec3 v = b->obj->vertices[i];
        const double s = b->in->scale;
        m->vertices[i] = (MeshVertex){ (int32_t)lround(v.x * s), (int32_t)lround(v.y * s), (int32_t)lround(-v.z * s) };
    }
    m->vertex_count = b->obj->vertex_count;
    return 1;
}

static int add_ref(Build *b, MeshTexture t)
{
    MeshDesc *m = b->out;
    if (m->texture_count >= MESH_MAX_TEXTURES) { exchange_fail(b->err, "more than %d textures", MESH_MAX_TEXTURES); return -1; }
    if (!ci_grow((void **)&m->textures, m->texture_count + 1, sizeof *m->textures, b->err)) return -1;
    m->textures[m->texture_count] = t;
    return (int)m->texture_count++;
}

/* the donor's tyre references and the bitmaps they name (the race puts the tyre frames there, car-model.md) */
static int tyre_refs(Build *b, const ShpiDescEntry *donor_bank, size_t donor_count)
{
    for (int k = 0; k < TYRE_REFS; k++) {
        int found = 0;
        for (uint32_t i = 0; i < b->donor->texture_count && !found; i++) {
            const MeshTexture *t = &b->donor->textures[i];
            if (strncmp(t->label, TYRE_LABELS[k], sizeof t->label) != 0) continue;
            found = 1;
            const ShpiDescEntry *e = t->name[0] != '\0' ? ci_bank_find(donor_bank, donor_count, t->name) : NULL;
            if (add_ref(b, *t) < 0 || (e != NULL && !ci_bank_keep(b->bank, e, b->err))) return 0;
        }
        if (!found) { exchange_fail(b->err, "the donor has no texture reference '%s'", TYRE_LABELS[k]); return 0; }
    }
    return 1;
}

/* the donor's words of an unlabelled reference */
static MeshTexture plain_ref(const MeshDesc *donor)
{
    MeshTexture t;
    memset(&t, 0, sizeof t);
    for (uint32_t i = 0; i < donor->texture_count; i++)
        if (donor->textures[i].label[0] == '\0' && donor->textures[i].name[0] != '\0') {
            memcpy(t.unknown_1, donor->textures[i].unknown_1, sizeof t.unknown_1);
            break;
        }
    return t;
}

static int is_wheel_face(const Build *b, uint32_t f) { return b->kind[b->obj->faces[f].object] == KIND_WHEEL; }

/* a reference and a bitmap for each material a face of the body uses */
static int material_refs(Build *b)
{
    const ObjModel *obj = b->obj;
    for (uint32_t f = 0; f < obj->face_count; f++) {
        const int32_t mat = obj->faces[f].material;
        if (is_wheel_face(b, f) || (mat >= 0 && b->material_ref[mat] >= 0)) continue;
        if (mat < 0) { exchange_fail(b->err, "line %u: a face with no material (usemtl)", obj->faces[f].line); return 0; }
        const ObjMaterial *x = &obj->materials[mat];
        const SetFile *png = x->texture[0] && (b->level == 0 ? b->in->high_pngs : b->in->low_pngs) != NULL
                                 ? fileset_find(b->level == 0 ? b->in->high_pngs : b->in->low_pngs, x->texture) : NULL;
        if (x->texture[0] == '\0') { exchange_fail(b->err, "material '%s' has no texture (map_Kd)", x->name); return 0; }
        if (png == NULL) { exchange_fail(b->err, "material '%s': no texture %s", x->name, x->texture); return 0; }
        char (*taken)[4] = malloc((b->bank->count + 1) * 4);
        if (taken == NULL) { exchange_fail(b->err, "out of memory"); return 0; }
        for (size_t i = 0; i < b->bank->count; i++) memcpy(taken[i], b->bank->entry[i].name, 4);
        MeshTexture t = plain_ref(b->donor);
        car_texture_entry_name(x->name, (const char (*)[4])taken, b->bank->count, t.name);
        free(taken);
        if (!ci_bank_png(b->bank, t.name, png->data, png->size, b->in, b->err)) return 0;
        b->material_ref[mat] = add_ref(b, t);
        if (b->material_ref[mat] < 0) return 0;
    }
    return 1;
}

/* the donor's polygon groups, each polygon's GROUP_NAMES index (or GROUPS: none of them) */
static uint8_t *donor_groups(const MeshDesc *d)
{
    uint8_t *g = malloc(d->polygon_count ? d->polygon_count : 1);
    if (g == NULL) return NULL;
    memset(g, GROUPS, d->polygon_count);
    for (uint32_t k = 0; k < d->group_count; k++)
        for (int n = 0; n < GROUPS; n++)
            if (strncmp(d->groups[k].name, GROUP_NAMES[n], MESH_NAME_SIZE) == 0)
                for (uint32_t i = 0; i < d->groups[k].count; i++)
                    if (d->groups[k].polygons[i] < d->polygon_count) g[d->groups[k].polygons[i]] = (uint8_t)n;
    return g;
}

/* the donor's most common flag bytes for a polygon of `corners` corners, with or without uvs, in `group` (any when none has it),
 * drawn the unreversed way */
static MeshDescPolygon template_of(const MeshDesc *d, const uint8_t *dg, int corners, int has_uv, int group)
{
    MeshDescPolygon best = { 0 };
    for (int pass = 0, found = 0; pass < 3 && !found; pass++) {
        int best_n = 0;
        for (uint32_t i = 0; i < d->polygon_count; i++) {
            const MeshDescPolygon *p = &d->polygons[i];
            if ((pass < 2 && (p->flags & 7) != corners) || p->has_uv != has_uv || (pass == 0 && dg[i] != group)) continue;
            int n = 0;
            for (uint32_t j = 0; j < d->polygon_count; j++) {
                const MeshDescPolygon *q = &d->polygons[j];
                n += q->flags == p->flags && (q->unknown_1 & ~REVERSED) == (p->unknown_1 & ~REVERSED) && q->unknown_3 == p->unknown_3;
            }
            if (n > best_n) best_n = n, best = *p, found = 1;
        }
    }
    best.flags = (uint8_t)((best.flags & ~7) | corners), best.unknown_1 &= (uint8_t)~REVERSED;
    return best;
}

static int group_of_kind(Kind k) { return k == KIND_INSIDE ? 0 : k == KIND_OUTSIDE || k == KIND_WHEEL ? 2 : 1; }

/* a uv of the texture `t` (w x h): 0..1 from the bottom to texels from the top, kept to 0..w x 0..h */
static uint32_t add_uv(Build *b, const ShpiDescEntry *t, ObjVec2 uv, uint32_t *clamped)
{
    MeshDesc *m = b->out;
    double u = uv.u * t->width, v = (1 - uv.v) * t->height;
    if (u < 0 || v < 0 || u > t->width || v > t->height) (*clamped)++;
    u = u < 0 ? 0 : u > t->width ? t->width : u, v = v < 0 ? 0 : v > t->height ? t->height : v;
    m->uvs[m->uv_count] = (MeshUv){ (int32_t)lround(u), (int32_t)lround(v) };
    return m->uv_count++;
}

/* one polygon of face `f` on its corners `c` (3 or 4 of the face's) */
static int add_polygon(Build *b, uint32_t f, const uint32_t *c, int n, uint32_t *clamped)
{
    const ObjModel *obj = b->obj;
    const ObjFace *face = &obj->faces[f];
    MeshDesc *m = b->out;
    const int wheel = is_wheel_face(b, f);
    int has_uv = !wheel;
    for (int k = 0; k < n; k++) has_uv &= obj->corner_uv[face->first + c[k]] >= 0;
    const int front = b->wheel[0] == (int)face->object || b->wheel[1] == (int)face->object;
    MeshDescPolygon p = template_of(b->donor, b->donor_group, n, has_uv, group_of_kind(b->kind[face->object]));
    p.has_uv = (uint8_t)has_uv;
    p.texture = (uint8_t)(wheel ? (front ? 0 : 1) : b->material_ref[face->material]);   /* frntires and rertires come first */
    const ShpiDescEntry *t = has_uv ? ci_bank_find(b->bank->entry, b->bank->count, m->textures[p.texture].name) : NULL;
    if (!ci_grow((void **)&m->polygons, m->polygon_count + 1, sizeof *m->polygons, b->err) ||
        !ci_grow((void **)&m->uvs, m->uv_count + 4, sizeof *m->uvs, b->err) ||
        !ci_grow((void **)&b->polygon_group, m->polygon_count + 1, 1, b->err))
        return 0;
    memset(p.vertex, 0, sizeof p.vertex), memset(p.uv, 0, sizeof p.uv);
    for (int k = 0; k < n; k++) {
        p.vertex[k] = obj->corner_vertex[face->first + c[k]];
        if (t != NULL) p.uv[k] = add_uv(b, t, obj->uvs[obj->corner_uv[face->first + c[k]]], clamped);
    }
    b->polygon_group[m->polygon_count] = (uint8_t)group_of_kind(b->kind[face->object]);
    m->polygons[m->polygon_count++] = p;
    return 1;
}

/* every face a polygon of 3 or 4 corners; a longer one a fan of triangles */
static int polygons(Build *b)
{
    uint32_t clamped = 0, split = 0;
    for (uint32_t f = 0; f < b->obj->face_count; f++) {
        const uint32_t n = b->obj->faces[f].corners;
        b->face_polygon[f] = b->out->polygon_count;
        if (n <= 4) {
            const uint32_t c[4] = { 0, 1, 2, 3 };
            if (!add_polygon(b, f, c, (int)n, &clamped)) return 0;
            continue;
        }
        split++;
        for (uint32_t k = 1; k + 1 < n; k++) {
            const uint32_t c[3] = { 0, k, k + 1 };
            if (!add_polygon(b, f, c, 3, &clamped)) return 0;
        }
    }
    if (split > 0) ci_note(b->in, "%u faces of more than 4 corners cut into triangles", split);
    if (clamped > 0) ci_note(b->in, "%u uvs outside their texture kept to its edge", clamped);
    return 1;
}

static int add_group(Build *b, const char *name, int which)
{
    MeshDesc *m = b->out;
    if (!ci_grow((void **)&m->groups, m->group_count + 1, sizeof *m->groups, b->err)) return 0;
    MeshDescGroup *g = &m->groups[m->group_count++];
    memset(g, 0, sizeof *g);
    memcpy(g->name, name, strlen(name));
    for (uint32_t i = 0; i < b->donor->group_count; i++)
        if (strncmp(b->donor->groups[i].name, name, MESH_NAME_SIZE) == 0) g->unknown_0 = b->donor->groups[i].unknown_0;
    if (!ci_grow((void **)&g->polygons, m->polygon_count, sizeof *g->polygons, b->err)) return 0;
    for (uint32_t p = 0; p < m->polygon_count; p++)
        if (which < 0 || b->polygon_group[p] == which) g->polygons[g->count++] = p;
    return 1;
}

/* high: inside, surface, outside (each polygon in one); low: NON-SORT, as the CD's low meshes */
static int groups(Build *b)
{
    if (b->level != 0) return add_group(b, "NON-SORT", -1);
    for (int k = 0; k < GROUPS; k++)
        if (!add_group(b, GROUP_NAMES[k], k)) return 0;
    return 1;
}

static void box_of(const MeshDesc *m, uint32_t n, int32_t lo[3], int32_t hi[3])
{
    for (int k = 0; k < 3; k++) lo[k] = INT32_MAX, hi[k] = INT32_MIN;
    for (uint32_t i = 0; i < n; i++) {
        const int32_t v[3] = { m->vertices[i].x, m->vertices[i].y, m->vertices[i].z };
        for (int k = 0; k < 3; k++) lo[k] = v[k] < lo[k] ? v[k] : lo[k], hi[k] = v[k] > hi[k] ? v[k] : hi[k];
    }
}

/* a wheel's bottom corner: of its lowest vertices, the front one (`front`) or the back one */
static uint32_t wheel_corner(const Build *b, int wheel, int front)
{
    const ObjObject *o = &b->obj->objects[b->wheel[wheel]];
    const MeshVertex *v = b->out->vertices;
    int32_t low = INT32_MAX;
    for (uint32_t i = o->first_vertex; i < o->first_vertex + o->vertex_count; i++) low = v[i].y < low ? v[i].y : low;
    uint32_t best = o->first_vertex;
    int32_t best_z = front ? INT32_MIN : INT32_MAX;
    for (uint32_t i = o->first_vertex; i < o->first_vertex + o->vertex_count; i++)
        if (v[i].y <= low + 1 && (front ? v[i].z > best_z : v[i].z < best_z)) best_z = v[i].z, best = i;
    return best;
}

/* section C: the author's points, else made: fl0..rr1 a wheel's bottom corners (car-model.md "Importing a model"), smok a new
 * vertex at the exhaust's usual place (x the middle, y 0.75, z 0.18 of the body) */
static int points(Build *b)
{
    MeshDesc *m = b->out;
    if (!ci_grow((void **)&m->points, POINTS, sizeof *m->points, b->err)) return 0;
    for (int k = 0; k < POINTS; k++) {
        MeshSectionC *c = &m->points[m->point_count++];
        memset(c, 0, sizeof *c);
        memcpy(c->name, POINT_NAMES[k], strlen(POINT_NAMES[k]));
        const int o = b->point[k];
        if (o >= 0 && b->obj->objects[o].vertex_count > 0) { c->value = b->obj->objects[o].first_vertex; continue; }
        if (k == 0) {
            int32_t lo[3], hi[3];
            box_of(m, m->vertex_count, lo, hi);
            m->vertices[m->vertex_count] = (MeshVertex){ (lo[0] + hi[0]) / 2, lo[1] + (hi[1] - lo[1]) * 3 / 4, lo[2] + (hi[2] - lo[2]) * 18 / 100 };
            c->value = m->vertex_count++;
            ci_note(b->in, "made the point smok (the exhaust) at the back of the body");
            continue;
        }
        const int wheel = (k - 1) / 2;
        if (b->wheel[wheel] < 0) { exchange_fail(b->err, "no point %s and no %s to take it from", POINT_NAMES[k], WHEEL_NAMES[wheel]); return 0; }
        c->value = wheel_corner(b, wheel, (k - 1) % 2 == 0);
        ci_note(b->in, "made the point %s from %s's bottom %s corner", POINT_NAMES[k], WHEEL_NAMES[wheel], (k - 1) % 2 == 0 ? "front" : "back");
    }
    m->real_vertices = m->vertex_count;
    return 1;
}

/* the polygon of the body (not a wheel) whose middle is nearest (x, y, z) */
static uint32_t nearest_polygon(const Build *b, double x, double y, double z)
{
    const MeshDesc *m = b->out;
    uint32_t best = 0;
    double best_d = 1e300;
    for (uint32_t i = 0; i < m->polygon_count; i++) {
        const MeshDescPolygon *p = &m->polygons[i];
        if (p->texture < TYRE_REFS) continue;
        const int n = p->flags & 7;
        double c[3] = { 0, 0, 0 };
        for (int k = 0; k < n; k++) c[0] += m->vertices[p->vertex[k]].x, c[1] += m->vertices[p->vertex[k]].y, c[2] += m->vertices[p->vertex[k]].z;
        const double d = pow(c[0] / n - x, 2) + pow(c[1] / n - y, 2) + pow(c[2] / n - z, 2);
        if (d < best_d) best_d = d, best = i;
    }
    return best;
}

/* the lights' labels: the author's first face of each, else the polygon nearest its corner of the body */
static int lights(Build *b)
{
    MeshDesc *m = b->out;
    if (!ci_grow((void **)&m->labels, LIGHTS, sizeof *m->labels, b->err)) return 0;
    int32_t lo[3], hi[3];
    box_of(m, m->real_vertices, lo, hi);
    for (int k = 0; k < LIGHTS; k++) {
        MeshLabel *l = &m->labels[m->label_count++];
        memset(l, 0, sizeof *l);
        memcpy(l->name, LIGHT_NAMES[k], strlen(LIGHT_NAMES[k]));
        const int o = b->light[k];
        if (o >= 0) {
            for (uint32_t f = 0; f < b->obj->face_count; f++)
                if (b->obj->faces[f].object == (uint32_t)o) { l->polygon = b->face_polygon[f]; break; }
            continue;
        }
        const double x = k % 2 == 0 ? lo[0] : hi[0], z = k < 2 ? hi[2] : lo[2];
        l->polygon = nearest_polygon(b, x, (lo[1] + hi[1]) / 2.0, z);
        ci_note(b->in, "made the light %s: polygon %u, nearest the body's %s %s corner", LIGHT_NAMES[k], l->polygon, k % 2 ? "right" : "left",
                k < 2 ? "front" : "rear");
    }
    return 1;
}

/* the donor's turn of the part named `name`, or the one asked for */
static int turn_of(const Build *b, const char *name, double *degrees)
{
    if (b->in->steer_degrees != 0) { *degrees = b->in->steer_degrees; return 1; }
    for (uint32_t p = 0; p < b->donor->part_count; p++) {
        if (strncmp(b->donor->parts[p].name, name, MESH_PART_NAME_SIZE) != 0) continue;
        CarSteer s;
        if (!car_steer_fit(b->donor, p, &s, b->err)) return 0;
        *degrees = s.degrees;
        return 1;
    }
    exchange_fail(b->err, "the donor has no part %.8s to take the steering from", name);
    return 0;
}

/* left_turn and right_turn: the front wheels' vertices, and after the model's own their offsets at full lock (car_steer.h) */
static int parts(Build *b)
{
    static const char *const NAMES[2] = { "left_tur", "right_tu" };
    MeshDesc *m = b->out;
    if (!ci_grow((void **)&m->parts, 2, sizeof *m->parts, b->err)) return 0;
    for (int w = 0; w < 2; w++) {
        const ObjObject *o = &b->obj->objects[b->wheel[w]];
        double degrees = 0;
        if (o->vertex_count == 0) { exchange_fail(b->err, "%s has no vertex", WHEEL_NAMES[w]); return 0; }
        if (!turn_of(b, NAMES[w], &degrees) || !ci_grow((void **)&m->vertices, m->vertex_count + o->vertex_count, sizeof *m->vertices, b->err))
            return 0;
        MeshDescPart *p = &m->parts[m->part_count++];
        memset(p, 0, sizeof *p);
        memcpy(p->name, NAMES[w], MESH_PART_NAME_SIZE);
        p->vertices = malloc(o->vertex_count * sizeof *p->vertices);
        if (p->vertices == NULL) { exchange_fail(b->err, "out of memory"); return 0; }
        p->count = o->vertex_count, p->first_offset_vertex = m->vertex_count;
        for (uint32_t k = 0; k < o->vertex_count; k++) p->vertices[k] = o->first_vertex + o->vertex_count - 1 - k;   /* descending, as the CD */
        MeshVertex *wheel = malloc(o->vertex_count * sizeof *wheel);
        if (wheel == NULL) { exchange_fail(b->err, "out of memory"); return 0; }
        for (uint32_t k = 0; k < o->vertex_count; k++) wheel[k] = m->vertices[p->vertices[k]];
        const int ok = car_steer_offsets(wheel, o->vertex_count, degrees, w == 0 ? -1 : 1, m->vertices + m->vertex_count, b->err);
        free(wheel);
        if (!ok) return 0;
        m->vertex_count += o->vertex_count;
    }
    return 1;
}

static int build(Build *b, const ShpiDescEntry *donor_bank, size_t donor_count)
{
    MeshDesc *m = b->out;
    memcpy(m->name, b->donor->name, sizeof m->name);
    m->unknown_08 = b->donor->unknown_08, m->unknown_64 = b->donor->unknown_64;
    if (!classify(b) || !vertices(b) || !ci_bank_start(b->bank, b->level, donor_bank, donor_count, b->err)) return 0;
    if (b->level == 0 && !tyre_refs(b, donor_bank, donor_count)) return 0;
    if (!material_refs(b) || !ci_grow((void **)&m->uvs, 4, sizeof *m->uvs, b->err) || !polygons(b) || !groups(b)) return 0;
    if (b->level != 0) { m->real_vertices = m->vertex_count; return 1; }
    return points(b) && lights(b) && parts(b);
}

int ci_obj_mesh(const CarImportIn *in, int level, const MeshDesc *donor, const ShpiDescEntry *donor_bank, size_t donor_count,
                MeshDesc *out, CarImportBank *bank, ExchangeError *err)
{
    const ObjModel *obj = level == 0 ? in->high : in->low;
    memset(out, 0, sizeof *out);
    memset(bank, 0, sizeof *bank);
    if (obj->from_exporter) {
        exchange_fail(err, "this OBJ is mesh_export's (its first line is '# ORIP mesh'): import it in raw mode, with its .tsv beside it");
        return 0;
    }
    if (obj->face_count == 0) { exchange_fail(err, "the OBJ has no face"); return 0; }
    Build b = { in, obj, donor, level, calloc(obj->object_count, sizeof(Kind)), { 0 }, { 0 }, { 0 },
                malloc(obj->material_count * sizeof(int32_t) + 1), calloc(obj->face_count, sizeof(uint32_t)), NULL, donor_groups(donor),
                out, bank, err };
    int ok = b.kind != NULL && b.material_ref != NULL && b.face_polygon != NULL && b.donor_group != NULL;
    if (!ok) exchange_fail(err, "out of memory");
    for (uint32_t i = 0; ok && i < obj->material_count; i++) b.material_ref[i] = -1;
    ok = ok && build(&b, donor_bank, donor_count);
    free(b.kind), free(b.material_ref), free(b.face_polygon), free(b.polygon_group), free(b.donor_group);
    if (!ok) mesh_desc_free(out), car_import_bank_free(bank);
    return ok;
}
