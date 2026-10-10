#include "viewer/model_view.h"

#include "game/sys/fixed_math.h"

#include <stdlib.h>
#include <string.h>

enum { MAX_POLYGONS = 512, MODEL_SHIFT = 12 };

static const float UNIT = 65536.0f;

typedef struct {
    RasterVertex v[4];
    int count;
    float depth;
    int textured;
    FshEntry texture;
} Face;

int model_load(ViewModel *model, const Chunk *pair)
{
    Chunk mesh_chunk, shpi;
    memset(model, 0, sizeof *model);
    if (chunk_child(pair, 0, &mesh_chunk) != CHUNK_OK || mesh_open(&model->mesh, &mesh_chunk) != MESH_OK) return 0;
    model->has_textures = chunk_child(pair, 1, &shpi) == CHUNK_OK && shpi.kind == CHUNK_KIND_BITMAP
        && chunk_bitmaps(&shpi, &model->textures) == FSH_OK;
    return 1;
}

static int find_texture(const ViewModel *model, uint8_t index, FshEntry *out)
{
    MeshTexture t;
    uint32_t entry;
    static const char NONE[4] = { 0, 0, 0, 0 };
    if (!model->has_textures || mesh_texture(&model->mesh, index, &t) != MESH_OK || memcmp(t.name, NONE, 4) == 0)
        return 0;
    return fsh_find(&model->textures, t.name, &entry) == FSH_OK && fsh_entry(&model->textures, entry, out) == FSH_OK
        && out->type == FSH_TYPE_BITMAP && (size_t)out->width * out->height <= out->payload_size;
}

/* 00491ec8: yaw matrix [c 0 -s; 0 1 0; s 0 c]. */
static Vec3 to_world(TrackPoint origin, int32_t s, int32_t c, const MeshVertex *v)
{
    const float x = (float)v->x, y = (float)v->y, z = (float)v->z;
    const float fs = s / 65536.0f, fc = c / 65536.0f, scale = (float)(1 << MODEL_SHIFT) / UNIT;
    return (Vec3){ origin.x / UNIT + (fc * x - fs * z) * scale, origin.y / UNIT + y * scale,
                   origin.z / UNIT + (fs * x + fc * z) * scale };
}

static int compare_depth(const void *a, const void *b)
{
    const float da = ((const Face *)a)->depth, db = ((const Face *)b)->depth;
    return (da < db) - (da > db);  /* far first */
}

static int build_face(const ViewModel *model, const Camera *cam, TrackPoint origin, int32_t s, int32_t c,
                      uint32_t index, Face *f)
{
    MeshPolygon p;
    uint32_t vi[4], ui[4];
    if (mesh_polygon(&model->mesh, index, &p) != MESH_OK || mesh_polygon_corners(&model->mesh, &p, vi, ui) != MESH_OK)
        return 0;
    f->count = p.flags & 7;
    if (f->count < 3 || f->count > 4) return 0;
    f->textured = find_texture(model, p.texture, &f->texture);
    const int has_uv = p.uv_slot != p.vertex_slot;
    f->depth = 0;
    for (int k = 0; k < f->count; k++) {
        MeshVertex mv;
        MeshUv uv = { 0, 0 };
        if (mesh_vertex(&model->mesh, vi[k], &mv) != MESH_OK) return 0;
        if (has_uv && mesh_uv(&model->mesh, ui[k], &uv) != MESH_OK) return 0;
        f->v[k] = camera_project(cam, to_world(origin, s, c, &mv), (float)uv.u, (float)uv.v);
        f->depth += f->v[k].z / f->count;
    }
    return 1;
}

void model_draw(const ViewModel *model, const RasterTarget *target, const Camera *cam, TrackPoint origin,
                uint32_t heading)
{
    const uint32_t count = model->mesh.polygon_count < MAX_POLYGONS ? model->mesh.polygon_count : MAX_POLYGONS;
    Face *faces = malloc(count * sizeof *faces);
    if (faces == NULL) return;
    const uint32_t a10 = heading >> 14;
    const int32_t s = fx_sin(a10), c = fx_cos(a10);
    uint32_t n = 0;
    for (uint32_t i = 0; i < count; i++)
        if (build_face(model, cam, origin, s, c, i, &faces[n])) n++;
    qsort(faces, n, sizeof *faces, compare_depth);
    for (uint32_t i = 0; i < n; i++)
        raster_polygon(target, faces[i].v, faces[i].count, faces[i].textured ? &faces[i].texture : NULL, 1, 0);
    free(faces);
}
