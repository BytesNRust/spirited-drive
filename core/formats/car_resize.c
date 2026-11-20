/* dep71: a car's bitmap resized and its uvs following (formats/car_resize.h). */
#include "formats/car_resize.h"
#include "formats/cfm_build.h"
#include "formats/chunk.h"
#include "formats/mesh_exchange.h"
#include "formats/shpi_build.h"

#include <stdlib.h>
#include <string.h>

enum { BANK_CAP = 256 };

int32_t car_resize_scale(int32_t v, uint16_t was, uint16_t now)
{
    if (was == 0) return v;
    const int64_t num = 2 * (int64_t)v * now, den = 2 * (int64_t)was;   /* v * now / was, halves away from zero */
    return (int32_t)(num >= 0 ? (num + was) / den : -((-num + was) / den));
}

void car_resize_pixels(const uint8_t *src, uint16_t sw, uint16_t sh, uint8_t *dst, uint16_t w, uint16_t h)
{
    for (uint32_t y = 0; y < h; y++) {
        const uint32_t sy = y * sh / h;
        for (uint32_t x = 0; x < w; x++) dst[y * w + x] = src[sy * sw + x * sw / w];
    }
}

/* does reference `t` show bitmap `name` */
static int shows(const MeshDesc *m, uint8_t t, const char name[4])
{
    if (t >= m->texture_count) return 0;
    char shown[4];
    mesh_blender_shown(m->textures[t].label, m->textures[t].name, shown);
    return memcmp(shown, name, 4) == 0;
}

/* per uv record: 1 a face of the bitmap uses it, 2 a face of another does (bits) */
static uint8_t *uv_users(const MeshDesc *m, const char name[4])
{
    uint8_t *use = calloc(m->uv_count ? m->uv_count : 1, 1);
    for (uint32_t i = 0; use != NULL && i < m->polygon_count; i++) {
        const MeshDescPolygon *p = &m->polygons[i];
        if (!p->has_uv) continue;
        const uint8_t bit = shows(m, p->texture, name) ? 1 : 2;
        for (int c = 0; c < (p->flags & 7) && c < 4; c++)
            if (p->uv[c] < m->uv_count) use[p->uv[c]] |= bit;
    }
    return use;
}

int car_resize_uvs(MeshDesc *mesh, const char name[4], uint16_t was_w, uint16_t was_h, uint16_t w, uint16_t h, CarResizeStats *stats,
                   ExchangeError *err)
{
    stats->faces = stats->uvs_scaled = stats->uvs_split = 0;
    uint8_t *use = uv_users(mesh, name);
    uint32_t *to = malloc((mesh->uv_count ? mesh->uv_count : 1) * sizeof *to);   /* each record's for the bitmap's faces */
    uint32_t extra = 0;
    for (uint32_t i = 0; use != NULL && i < mesh->uv_count; i++) extra += use[i] == 3;
    MeshUv *uvs = use != NULL && to != NULL ? malloc((mesh->uv_count + extra ? mesh->uv_count + extra : 1) * sizeof *uvs) : NULL;
    if (uvs == NULL) {
        free(use), free(to), free(uvs);
        exchange_fail(err, "out of memory");
        return 0;
    }
    memcpy(uvs, mesh->uvs, mesh->uv_count * sizeof *uvs);
    uint32_t n = mesh->uv_count;
    for (uint32_t i = 0; i < mesh->uv_count; i++) {
        to[i] = i;
        if (!(use[i] & 1)) continue;
        if (use[i] & 2) to[i] = n++, stats->uvs_split++;
        uvs[to[i]] = (MeshUv){ car_resize_scale(mesh->uvs[i].u, was_w, w), car_resize_scale(mesh->uvs[i].v, was_h, h) };
        stats->uvs_scaled++;
    }
    for (uint32_t i = 0; i < mesh->polygon_count; i++) {
        MeshDescPolygon *p = &mesh->polygons[i];
        if (!p->has_uv || !shows(mesh, p->texture, name)) continue;
        stats->faces++;
        for (int c = 0; c < (p->flags & 7) && c < 4; c++)
            if (p->uv[c] < mesh->uv_count) p->uv[c] = to[p->uv[c]];
    }
    free(mesh->uvs);
    mesh->uvs = uvs, mesh->uv_count = n;
    free(use), free(to);
    return 1;
}

/* the level's bank with `name` at w x h (malloc'd SHPI); `pixels` taken, or the old scaled into `*made` (the caller frees) */
static int resized_bank(const uint8_t *shpi, size_t len, const char name[4], uint16_t w, uint16_t h, const uint8_t *pixels, uint8_t **made,
                        uint8_t **out, size_t *out_len, CarResizeStats *stats, ExchangeError *err)
{
    ShpiDescEntry *e = malloc(BANK_CAP * sizeof *e);
    size_t n = 0;
    if (e == NULL) { exchange_fail(err, "out of memory"); return 0; }
    const size_t declared = len >= 8 ? (size_t)shpi[4] | (size_t)shpi[5] << 8 | (size_t)shpi[6] << 16 | (size_t)shpi[7] << 24 : 0;
    if (declared >= 8 && declared <= len) len = declared;   /* not the CFM's padding to 4 after it (it would grow by it each time) */
    int ok = shpi_describe(shpi, len, e, BANK_CAP, &n, err);
    ShpiDescEntry *hit = NULL;
    for (size_t i = 0; ok && i < n; i++)
        if (memcmp(e[i].name, name, 4) == 0) hit = &e[i];
    if (ok && (hit == NULL || hit->type != 0x7b))
        ok = 0, exchange_fail(err, hit == NULL ? "the bank has no bitmap '%.4s'" : "'%.4s' is not a bitmap", name);
    if (ok) {
        stats->was_w = hit->width, stats->was_h = hit->height;
        if (pixels == NULL && (*made = malloc((size_t)w * h)) == NULL) ok = 0, exchange_fail(err, "out of memory");
        else if (pixels == NULL) car_resize_pixels(hit->data, hit->width, hit->height, *made, w, h);
    }
    if (ok) {
        hit->width = w, hit->height = h, hit->data = pixels != NULL ? pixels : *made;
        hit->block_size = SHPI_BLOCK_AUTO;
        ShpiHead head;   /* the bank keeps its own tag */
        ok = shpi_describe_head(shpi, len, &head) && shpi_build_head(&head, e, n, out, out_len, err);
    }
    free(e);
    return ok;
}

static int resized_mesh(const uint8_t *orip, size_t len, const char name[4], uint16_t w, uint16_t h, uint8_t **out, size_t *out_len,
                        CarResizeStats *stats, ExchangeError *err)
{
    Chunk c;
    Mesh m;
    MeshDesc d;
    if (chunk_open(&c, orip, len) != CHUNK_OK || mesh_open(&m, &c) != MESH_OK) { exchange_fail(err, "the mesh does not open"); return 0; }
    if (!mesh_describe(&m, &d, err)) return 0;
    const int ok = car_resize_uvs(&d, name, stats->was_w, stats->was_h, w, h, stats, err) && mesh_build(&d, out, out_len, err);
    mesh_desc_free(&d);
    return ok;
}

int car_resize(const uint8_t *cfm, size_t len, int level, const char name[4], uint16_t w, uint16_t h, const uint8_t *pixels,
               uint8_t **out, size_t *out_len, CarResizeStats *stats, ExchangeError *err)
{
    *out = NULL, *out_len = 0;
    memset(stats, 0, sizeof *stats);
    if (level != 0 && level != 1) { exchange_fail(err, "no level %d (0 the high model, 1 the far one)", level); return 0; }
    if (w == 0 || h == 0 || w > SHPI_MAX_SIDE || h > SHPI_MAX_SIDE) {
        exchange_fail(err, "%ux%u: a side is 1 to %d", w, h, SHPI_MAX_SIDE);
        return 0;
    }
    const uint8_t *child[CFM_CHILDREN];
    size_t clen[CFM_CHILDREN];
    if (!cfm_describe(cfm, len, child, clen, err)) return 0;
    uint8_t *made = NULL, *bank = NULL, *mesh = NULL;
    size_t bank_len = 0, mesh_len = 0;
    int ok = resized_bank(child[2 * level + 1], clen[2 * level + 1], name, w, h, pixels, &made, &bank, &bank_len, stats, err) &&
             resized_mesh(child[2 * level], clen[2 * level], name, w, h, &mesh, &mesh_len, stats, err);
    if (ok) {
        const uint8_t *parts[CFM_CHILDREN];
        size_t parts_len[CFM_CHILDREN];
        for (int k = 0; k < CFM_CHILDREN; k++) parts[k] = child[k], parts_len[k] = clen[k];
        parts[2 * level] = mesh, parts_len[2 * level] = mesh_len, parts[2 * level + 1] = bank, parts_len[2 * level + 1] = bank_len;
        ok = cfm_build(parts, parts_len, out, out_len, err);
    }
    free(made), free(bank), free(mesh);
    return ok;
}
