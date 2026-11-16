/* dep71: see mesh_blender.h. */
#include "formats/mesh_blender.h"

#include "formats/mesh_exchange.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char TOPOLOGY[] = "adding or removing geometry is not a Blender round trip: import it with `car import` without --raw";

/* the original face each Blender face is (dep71, tnfs-35's decision: Blender may write the faces in another order, by material):
 * from[o] the Blender face of original face o, turn[o] how far its corners are rotated (Blender's corner (k + turn) % n is the
 * original's corner k) */
typedef struct {
    uint32_t *from, *turn;
} Match;

/* how far face `bf`'s corners are turned from `of`'s, the same vertices the same way round; -1 for none */
static int turn_of(const ObjModel *b, const ObjFace *bf, const ObjModel *o, const ObjFace *of)
{
    const uint32_t n = of->corners;
    if (bf->corners != n) return -1;
    for (uint32_t r = 0; r < n; r++) {
        uint32_t k = 0;
        while (k < n && b->corner_vertex[bf->first + (k + r) % n] == o->corner_vertex[of->first + k]) k++;
        if (k == n) return (int)r;
    }
    return -1;
}

/* the same vertices in any order */
static int same_set(const ObjModel *b, const ObjFace *bf, const ObjModel *o, const ObjFace *of)
{
    if (bf->corners != of->corners) return 0;
    for (uint32_t k = 0; k < bf->corners; k++) {
        uint32_t j = 0;
        while (j < of->corners && o->corner_vertex[of->first + j] != b->corner_vertex[bf->first + k]) j++;
        if (j == of->corners) return 0;
    }
    return 1;
}

/* the original face Blender face `f` is: the first unused one with its vertices turned; 0 with the reason (no face on them, a second
 * face on them, or them the other way round) */
static int match_one(const ObjModel *b, uint32_t f, const ObjModel *o, uint8_t *used, Match *m, ExchangeError *err)
{
    const ObjFace *bf = &b->faces[f];
    int set = 0, taken = 0;
    for (uint32_t of = 0; of < o->face_count; of++) {
        const int r = turn_of(b, bf, o, &o->faces[of]);
        if (r >= 0 && !used[of]) {
            used[of] = 1, m->from[of] = f, m->turn[of] = (uint32_t)r;
            return 1;
        }
        taken |= r >= 0;
        set |= same_set(b, bf, o, &o->faces[of]);
    }
    if (taken) return exchange_fail(err, "line %u: a second face on the vertices of one face of the mesh: %s", bf->line, TOPOLOGY), 0;
    if (set) return exchange_fail(err, "line %u: a face of the mesh with its corners the other way round (its normal flipped in Blender?): "
                                       "flip it back", bf->line), 0;
    return exchange_fail(err, "line %u: its %u vertices are no face of the mesh: %s", bf->line, bf->corners, TOPOLOGY), 0;
}

/* the same topology as the original: the vertex count and the faces, each on an original face's vertices the same way round, in any
 * order and turned (match_one); `m` malloc'd */
static int same_topology(const ObjModel *b, const ObjModel *o, Match *m, ExchangeError *err)
{
    if (b->vertex_count != o->vertex_count)
        return exchange_fail(err, "the OBJ has %u vertices and the mesh %u: %s", b->vertex_count, o->vertex_count, TOPOLOGY), 0;
    if (b->face_count != o->face_count)
        return exchange_fail(err, "the OBJ has %u faces and the mesh %u: %s", b->face_count, o->face_count, TOPOLOGY), 0;
    uint8_t *used = calloc(o->face_count + 1, 1);
    m->from = malloc((o->face_count + 1) * sizeof *m->from), m->turn = malloc((o->face_count + 1) * sizeof *m->turn);
    int ok = used != NULL && m->from != NULL && m->turn != NULL;
    if (!ok) exchange_fail(err, "out of memory");
    for (uint32_t f = 0; ok && f < b->face_count; f++) ok = match_one(b, f, o, used, m, err);
    free(used);
    if (!ok) free(m->from), free(m->turn);
    return ok;
}

/* a coordinate in mesh units or texels that fits the mesh's 32-bit words */
static int fits(double v) { return v >= -2147483647.0 && v <= 2147483647.0; }

/* the texture reference a face's material names (mesh_material_texture: -1 not one, -2 malformed) */
static int texture_of(const ObjModel *m, const ObjFace *face)
{
    return face->material >= 0 ? mesh_material_texture(m->materials[face->material].name) : -1;
}

static void *copy_of(const void *from, size_t n, size_t item)
{
    void *to = malloc((n ? n : 1) * item);
    if (to != NULL && n > 0) memcpy(to, from, n * item);
    return to;
}

/* the original's model with room for one more uv a corner (a split row each, at most) */
static int start(const ObjModel *b, const ObjModel *o, const Match *m, ObjModel *out, ExchangeError *err)
{
    memset(out, 0, sizeof *out);
    out->vertices = copy_of(b->vertices, b->vertex_count, sizeof *b->vertices);
    out->uvs = malloc(((size_t)o->uv_count + o->corner_count + 1) * sizeof *out->uvs);
    out->corner_vertex = copy_of(o->corner_vertex, o->corner_count, sizeof *o->corner_vertex);   /* the matched faces' (match_one) */
    out->corner_uv = copy_of(o->corner_uv, o->corner_count, sizeof *o->corner_uv);
    out->faces = copy_of(o->faces, o->face_count, sizeof *o->faces);
    out->materials = copy_of(b->materials, b->material_count, sizeof *b->materials);
    out->objects = copy_of(o->objects, o->object_count, sizeof *o->objects);
    if (!out->vertices || !out->uvs || !out->corner_vertex || !out->corner_uv || !out->faces || !out->materials || !out->objects) {
        obj_free(out);
        return exchange_fail(err, "out of memory"), 0;
    }
    if (o->uv_count > 0) memcpy(out->uvs, o->uvs, o->uv_count * sizeof *o->uvs);
    out->vertex_count = b->vertex_count, out->uv_count = o->uv_count, out->corner_count = o->corner_count;
    out->face_count = o->face_count, out->material_count = b->material_count, out->object_count = o->object_count;
    out->from_exporter = 1;
    for (uint32_t f = 0; f < out->face_count; f++) out->faces[f].material = b->faces[m->from[f]].material;
    for (uint32_t v = 0; v < out->vertex_count; v++) {
        out->vertices[v] = (ObjVec3){ b->vertices[v].x * MESH_BLENDER_UNITS, b->vertices[v].y * MESH_BLENDER_UNITS, -b->vertices[v].z * MESH_BLENDER_UNITS };
        if (!fits(out->vertices[v].x) || !fits(out->vertices[v].y) || !fits(out->vertices[v].z)) {
            obj_free(out);
            return exchange_fail(err, "vertex %u is %g m from the middle: past what the mesh's units hold", v + 1,
                                 fabs(b->vertices[v].x) + fabs(b->vertices[v].y) + fabs(b->vertices[v].z)), 0;
        }
    }
    return 1;
}

/* corner `c` of face `f` takes `uv` (texels) on its original row, or on a new one when a corner before it gave that row another */
static void put_uv(ObjModel *out, uint8_t *taken, uint32_t c, ObjVec2 uv)
{
    const int32_t row = out->corner_uv[c];
    const int same = out->uvs[row].u == uv.u && out->uvs[row].v == uv.v;
    if (!taken[row] || same) {
        out->uvs[row] = uv, taken[row] = 1;
        return;
    }
    out->corner_uv[c] = (int32_t)out->uv_count;
    out->uvs[out->uv_count++] = uv;
}

int mesh_blender_back(const ObjModel *blender, const ObjModel *original, const MeshBlenderSize *sizes, uint32_t size_count, ObjModel *out,
                      ExchangeError *err)
{
    Match m;
    if (!same_topology(blender, original, &m, err)) return 0;
    if (!start(blender, original, &m, out, err)) return free(m.from), free(m.turn), 0;
    uint8_t *taken = calloc(original->uv_count + 1, 1);
    int ok = taken != NULL;
    if (!ok) exchange_fail(err, "out of memory");
    for (uint32_t f = 0; ok && f < original->face_count; f++) {
        const ObjFace *bf = &blender->faces[m.from[f]], *of = &original->faces[f];
        int t = texture_of(blender, bf);
        if (t == -2) {
            exchange_fail(err, "line %u: the material %s is not one of the mesh's (t<0..255>_<name>)", bf->line, blender->materials[bf->material].name);
            ok = 0;
            break;
        }
        if (t < 0) t = texture_of(original, of);
        if (t >= 0 && (uint32_t)t >= size_count) {
            exchange_fail(err, "line %u: face %u's material names t%d, a texture the mesh does not have (it has %u)", bf->line, f, t, size_count);
            ok = 0;
            break;
        }
        const MeshBlenderSize size = t >= 0 ? sizes[t] : (MeshBlenderSize){ 1, 1 };
        for (uint32_t k = 0; ok && k < of->corners; k++) {
            const uint32_t c = of->first + k;
            const int32_t bt = blender->corner_uv[bf->first + (k + m.turn[f]) % of->corners];
            if (original->corner_uv[c] < 0 || bt < 0) continue;   /* no uv before: none now; Blender's none: the original's */
            const ObjVec2 u = blender->uvs[bt];
            const double tu = u.u * size.width, tv = (1.0 - u.v) * size.height;
            if (size.width == 0 || size.height == 0) {
                exchange_fail(err, "line %u: face %u's texture t%d has no bitmap in the car's bank (another --donor?): its uvs cannot be turned "
                                   "back into texels", bf->line, f, t);
                ok = 0;
            } else if (!fits(tu) || !fits(tv)) {
                exchange_fail(err, "line %u: face %u's uv %g %g is past what the mesh's texels hold", bf->line, f, u.u, u.v);
                ok = 0;
            } else put_uv(out, taken, c, (ObjVec2){ (double)lround(tu), (double)lround(tv) });
        }
    }
    free(taken), free(m.from), free(m.turn);
    if (!ok) obj_free(out);
    return ok;
}
