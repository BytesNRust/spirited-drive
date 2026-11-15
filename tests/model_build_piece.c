/* dep59 tools B1: rewrites the CFM of a car piece made with `tnfs-studio car new` from the Viper, through the builders
 * (formats/mesh_build.h, shpi_build.h, cfm_build.h), for tests/model_build.sh. A test program, not a tnfs-studio command.
 *
 *   model_build_piece PIECE_DIR same|fin|texture|big
 *
 *   same     describe and build again, changing nothing (the bytes must come back)
 *   fin      a wing over the tail: 8 vertices and 6 quads in `surface`, textured as the roof
 *   texture  the wing with a new 48 x 20 bitmap `xfin` of the Viper's own colours, and a texture reference naming it
 *   big      every polygon of `surface` cut in 4, pass after pass, until the model has more than 300
 *
 * The PDN is not touched. The wing's corners turn as the donor's roof turns: the winding the race culls by is measured on the car. */
#include "formats/cfm_build.h"
#include "formats/chunk.h"
#include "formats/mesh.h"
#include "formats/mesh_build.h"
#include "formats/shpi_build.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_ENTRIES = 64, FIN_W = 48, FIN_H = 20, BIG_POLYGONS = 300, POLY_REVERSED_BIT = 0x2 /* byte 1: drawn when its winding is negative */ };

typedef struct { const char *dir; char stem[16]; char path[1024]; } Piece;

static void *grow(void *p, size_t n, size_t size)
{
    void *q = realloc(p, n * size);
    if (q == NULL) fprintf(stderr, "model_build_piece: out of memory\n"), exit(1);
    return q;
}

static uint8_t *read_all(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = n > 0 ? malloc((size_t)n) : NULL;
    if (d != NULL && fread(d, 1, (size_t)n, f) != (size_t)n) free(d), d = NULL;
    fclose(f);
    *size = (size_t)n;
    return d;
}

/* the piece's stem from its mod.ini, and the path of its CFM */
static int find_piece(Piece *p)
{
    char ini[1024], line[256];
    snprintf(ini, sizeof ini, "%s/mod.ini", p->dir);
    FILE *f = fopen(ini, "r");
    if (f == NULL) return fprintf(stderr, "model_build_piece: no %s\n", ini), 0;
    p->stem[0] = '\0';
    while (fgets(line, sizeof line, f) != NULL)
        if (strncmp(line, "stem=", 5) == 0) sscanf(line + 5, "%15[A-Za-z0-9_]", p->stem);
    fclose(f);
    if (p->stem[0] == '\0') return fprintf(stderr, "model_build_piece: %s has no stem=\n", ini), 0;
    snprintf(p->path, sizeof p->path, "%s/SIMDATA/CARFAMS/%s.CFM", p->dir, p->stem);
    return 1;
}

static int find_group(const MeshDesc *d, const char *name)
{
    for (uint32_t i = 0; i < d->group_count; i++)
        if (strncmp(d->groups[i].name, name, MESH_NAME_SIZE) == 0) return (int)i;
    return -1;
}

static int texture_named(const MeshDesc *d, const MeshDescPolygon *p, const char *name)
{
    return p->texture < d->texture_count && memcmp(d->textures[p->texture].name, name, 4) == 0;
}

static void normal(const MeshVertex *a, const MeshVertex *b, const MeshVertex *c, double n[3])
{
    const double u[3] = { b->x - a->x, b->y - a->y, b->z - a->z }, v[3] = { c->x - a->x, c->y - a->y, c->z - a->z };
    n[0] = u[1] * v[2] - u[2] * v[1], n[1] = u[2] * v[0] - u[0] * v[2], n[2] = u[0] * v[1] - u[1] * v[0];
}

/* the roof: the highest quad textured topv that is not drawn reversed; -1 when there is none */
static int roof(const MeshDesc *d)
{
    int best = -1;
    double best_y = -1e9;
    for (uint32_t i = 0; i < d->polygon_count; i++) {
        const MeshDescPolygon *p = &d->polygons[i];
        if ((p->flags & 7) != 4 || (p->unknown_1 & POLY_REVERSED_BIT) || !p->has_uv || !texture_named(d, p, "topv")) continue;
        double y = 0;
        for (int k = 0; k < 4; k++) y += d->vertices[p->vertex[k]].y;
        if (y > best_y) best_y = y, best = (int)i;
    }
    return best;
}

static uint32_t add_vertex(MeshDesc *d, MeshVertex v)
{
    d->vertices = grow(d->vertices, d->vertex_count + 1, sizeof *d->vertices);
    d->vertices[d->vertex_count] = v;
    return d->vertex_count++;
}

static uint32_t add_uv(MeshDesc *d, MeshUv u)
{
    d->uvs = grow(d->uvs, d->uv_count + 1, sizeof *d->uvs);
    d->uvs[d->uv_count] = u;
    return d->uv_count++;
}

static uint32_t add_polygon(MeshDesc *d, int group, MeshDescPolygon p)
{
    d->polygons = grow(d->polygons, d->polygon_count + 1, sizeof *d->polygons);
    d->polygons[d->polygon_count] = p;
    MeshDescGroup *g = &d->groups[group];
    g->polygons = grow(g->polygons, g->count + 1, sizeof *g->polygons);
    g->polygons[g->count++] = d->polygon_count;
    return d->polygon_count++;
}

/* the wing: a box over the tail, each face turned so that its outside winds as the roof's top does */
static int add_fin(MeshDesc *d, int top, uint8_t texture)
{
    const int surface = find_group(d, "surface");
    if (surface < 0) return fprintf(stderr, "model_build_piece: the donor has no surface group\n"), 0;
    const MeshDescPolygon *r = &d->polygons[top];
    double rn[3];
    normal(&d->vertices[r->vertex[0]], &d->vertices[r->vertex[1]], &d->vertices[r->vertex[2]], rn);
    const int sign = rn[1] > 0 ? 1 : -1;   /* the roof's outside is +y */
    const MeshDescPolygon base = *r;
    uint32_t v0 = d->vertex_count;
    for (int k = 0; k < 8; k++) add_vertex(d, (MeshVertex){ k & 1 ? 100 : -100, k & 2 ? 160 : 150, k & 4 ? -245 : -285 });
    static const int face[6][4] = { { 0, 2, 6, 4 }, { 1, 5, 7, 3 }, { 0, 4, 5, 1 }, { 2, 3, 7, 6 }, { 0, 1, 3, 2 }, { 4, 6, 7, 5 } };
    static const int out[6][3] = { { -1, 0, 0 }, { 1, 0, 0 }, { 0, -1, 0 }, { 0, 1, 0 }, { 0, 0, -1 }, { 0, 0, 1 } };
    for (int f = 0; f < 6; f++) {
        MeshDescPolygon q = base;
        q.texture = texture;
        double n[3];
        normal(&d->vertices[v0 + face[f][0]], &d->vertices[v0 + face[f][1]], &d->vertices[v0 + face[f][2]], n);
        const int flip = (n[0] * out[f][0] + n[1] * out[f][1] + n[2] * out[f][2] > 0 ? 1 : -1) != sign;
        for (int k = 0; k < 4; k++) {
            const int c = flip ? (4 - k) % 4 : k;
            q.vertex[k] = v0 + (uint32_t)face[f][c];
            q.uv[k] = add_uv(d, (MeshUv){ k == 1 || k == 2 ? FIN_W - 1 : 0, k >= 2 ? FIN_H - 1 : 0 });
        }
        add_polygon(d, surface, q);
    }
    return 1;
}

/* the index of the Viper's palette nearest an (r, g, b) of 0..255, among its own colours 0x00..0xF9 */
static uint8_t nearest(const uint8_t *pal, int r, int g, int b)
{
    int best = 0, best_d = 1 << 30;
    for (int i = 0; i < 0xfa; i++) {
        const int dr = pal[3 * i] * 4 - r, dg = pal[3 * i + 1] * 4 - g, db = pal[3 * i + 2] * 4 - b, dist = dr * dr + dg * dg + db * db;
        if (dist < best_d) best_d = dist, best = i;
    }
    return (uint8_t)best;
}

/* xfin: yellow and black stripes, in the palette's 6-bit (0x22) or 8-bit (0x24) colours */
static void paint_fin(const ShpiDescEntry *pal, uint8_t px[FIN_W * FIN_H])
{
    uint8_t rgb[SHPI_PALETTE_BYTES];
    for (int i = 0; i < SHPI_PALETTE_BYTES; i++) rgb[i] = pal->type == 0x24 ? (uint8_t)(pal->data[i] >> 2) : pal->data[i];
    const uint8_t yellow = nearest(rgb, 250, 220, 0), black = nearest(rgb, 0, 0, 0);
    for (int y = 0; y < FIN_H; y++)
        for (int x = 0; x < FIN_W; x++) px[y * FIN_W + x] = ((x + y) / 6) % 2 ? black : yellow;
}

/* the high bank with xfin added after its entries; `out` malloc'd */
static int bank_with_fin(const uint8_t *bank, size_t len, uint8_t px[FIN_W * FIN_H], uint8_t **out, size_t *out_len)
{
    ShpiDescEntry e[MAX_ENTRIES + 1];
    size_t n = 0;
    ExchangeError err = { { 0 } };
    if (!shpi_describe(bank, len, e, MAX_ENTRIES, &n, &err)) return fprintf(stderr, "model_build_piece: the bank: %s\n", err.message), 0;
    const ShpiDescEntry *pal = NULL;
    for (size_t i = 0; i < n; i++)
        if (memcmp(e[i].name, "!PAL", 4) == 0) pal = &e[i];
    if (pal == NULL) return fprintf(stderr, "model_build_piece: the bank has no !PAL\n"), 0;
    paint_fin(pal, px);
    ShpiDescEntry x;
    memset(&x, 0, sizeof x);
    memcpy(x.name, "xfin", 4);
    x.type = 0x7b, x.width = FIN_W, x.height = FIN_H, x.data = px, x.block_size = SHPI_BLOCK_AUTO, x.dir_index = (uint32_t)n;
    e[n] = x;
    if (!shpi_build(e, n + 1, out, out_len, &err)) return fprintf(stderr, "model_build_piece: the bank: %s\n", err.message), 0;
    return 1;
}

/* a reference naming xfin, the roof's otherwise; its index */
static int add_fin_texture(MeshDesc *d, uint8_t roof_texture)
{
    if (d->texture_count >= MESH_MAX_TEXTURES) return fprintf(stderr, "model_build_piece: no room for a texture reference\n"), -1;
    d->textures = grow(d->textures, d->texture_count + 1, sizeof *d->textures);
    MeshTexture t = d->textures[roof_texture];
    memcpy(t.name, "xfin", 4);
    d->textures[d->texture_count] = t;
    return (int)d->texture_count++;
}

static uint32_t midpoint_vertex(MeshDesc *d, uint32_t a, uint32_t b)
{
    const MeshVertex p = d->vertices[a], q = d->vertices[b];
    return add_vertex(d, (MeshVertex){ (p.x + q.x) / 2, (p.y + q.y) / 2, (p.z + q.z) / 2 });
}

static uint32_t midpoint_uv(MeshDesc *d, uint32_t a, uint32_t b)
{
    const MeshUv p = d->uvs[a], q = d->uvs[b];
    return add_uv(d, (MeshUv){ (p.u + q.u) / 2, (p.v + q.v) / 2 });
}

/* polygon `i` cut in 4 with its corners' order: it keeps its index (labels name it) and the other 3 go after the polygons */
static void cut(MeshDesc *d, int group, uint32_t i)
{
    const MeshDescPolygon p = d->polygons[i];
    const int n = p.flags & 7;
    uint32_t mv[5], mu[5] = { 0 };
    for (int k = 0; k < n; k++) {
        mv[k] = midpoint_vertex(d, p.vertex[k], p.vertex[(k + 1) % n]);
        if (p.has_uv) mu[k] = midpoint_uv(d, p.uv[k], p.uv[(k + 1) % n]);
    }
    if (n == 4) {
        mv[4] = midpoint_vertex(d, mv[0], mv[2]);
        if (p.has_uv) mu[4] = midpoint_uv(d, mu[0], mu[2]);
    }
    /* each piece by its corners: an original corner (0..3) or a midpoint (4 + k, the centre 8) */
    static const int quad[4][4] = { { 0, 4, 8, 7 }, { 4, 1, 5, 8 }, { 8, 5, 2, 6 }, { 7, 8, 6, 3 } };
    static const int tri[4][4] = { { 0, 4, 6, 0 }, { 4, 1, 5, 0 }, { 6, 5, 2, 0 }, { 4, 5, 6, 0 } };
    for (int s = 0; s < 4; s++) {
        MeshDescPolygon q = p;
        for (int k = 0; k < n; k++) {
            const int c = n == 4 ? quad[s][k] : tri[s][k];
            q.vertex[k] = c < 4 ? p.vertex[c] : mv[c - 4];
            q.uv[k] = !p.has_uv ? 0 : c < 4 ? p.uv[c] : mu[c - 4];
        }
        if (s == 0) d->polygons[i] = q;
        else add_polygon(d, group, q);
    }
}

static int make_big(MeshDesc *d)
{
    const int surface = find_group(d, "surface");
    if (surface < 0) return fprintf(stderr, "model_build_piece: the donor has no surface group\n"), 0;
    while (d->polygon_count <= BIG_POLYGONS) {
        const uint32_t count = d->groups[surface].count;
        for (uint32_t k = 0; k < count; k++) {
            const uint32_t i = d->groups[surface].polygons[k];
            const int n = d->polygons[i].flags & 7;
            if (n == 3 || n == 4) cut(d, surface, i);
        }
    }
    return 1;
}

static int write_all(const char *path, const uint8_t *b, size_t n)
{
    FILE *f = fopen(path, "wb");
    const int ok = f != NULL && fwrite(b, 1, n, f) == n;
    if (f != NULL && fclose(f) != 0) return 0;
    return ok;
}

/* child `i` becomes `b`, unless `b` is its bytes already: then it keeps the file's extent, padding and all */
static void put_child(const uint8_t *child[CFM_CHILDREN], size_t len[CFM_CHILDREN], int i, const uint8_t *b, size_t n)
{
    if (n <= len[i] && len[i] - n < 4 && memcmp(child[i], b, n) == 0) return;
    child[i] = b, len[i] = n;
}

/* the high mesh of `d` and the high bank `bank` (both built), the low ones as the file has them */
static int rebuild(const Piece *p, const uint8_t *cfm, size_t size, const MeshDesc *d, const uint8_t *bank, size_t bank_len)
{
    const uint8_t *child[CFM_CHILDREN];
    size_t len[CFM_CHILDREN], mesh_len = 0, out_len = 0;
    uint8_t *mesh = NULL, *out = NULL;
    ExchangeError err = { { 0 } };
    int ok = cfm_describe(cfm, size, child, len, &err) && mesh_build(d, &mesh, &mesh_len, &err);
    if (ok) {
        put_child(child, len, CFM_CHILD_MESH_HIGH, mesh, mesh_len);
        put_child(child, len, CFM_CHILD_BITMAPS_HIGH, bank, bank_len);
        ok = cfm_build(child, len, &out, &out_len, &err);
    }
    if (!ok) fprintf(stderr, "model_build_piece: %s: %s\n", p->path, err.message);
    else if (!write_all(p->path, out, out_len)) fprintf(stderr, "model_build_piece: cannot write %s\n", p->path), ok = 0;
    free(mesh);
    free(out);
    return ok;
}

/* the high bank described and built again as it is; `out` malloc'd */
static int same_bank(const uint8_t *bank, size_t len, uint8_t **out, size_t *out_len)
{
    ShpiDescEntry e[MAX_ENTRIES];
    size_t n = 0;
    ExchangeError err = { { 0 } };
    if (shpi_describe(bank, len, e, MAX_ENTRIES, &n, &err) && shpi_build(e, n, out, out_len, &err)) return 1;
    return fprintf(stderr, "model_build_piece: the bank: %s\n", err.message), 0;
}

/* the mode applied to the description and the bank; 0 after saying why */
static int apply(const char *mode, MeshDesc *d, const Chunk *bank, uint8_t fin_px[FIN_W * FIN_H], uint8_t **out, size_t *out_len)
{
    const int is_fin = strcmp(mode, "fin") == 0, is_texture = strcmp(mode, "texture") == 0;
    if (!is_fin && !is_texture && strcmp(mode, "same") != 0 && strcmp(mode, "big") != 0)
        return fprintf(stderr, "model_build_piece: no mode %s (same, fin, texture, big)\n", mode), 0;
    if (strcmp(mode, "big") == 0) return make_big(d) && same_bank(bank->data, bank->size, out, out_len);
    if (!is_fin && !is_texture) return same_bank(bank->data, bank->size, out, out_len);
    const int top = roof(d);
    if (top < 0) return fprintf(stderr, "model_build_piece: the donor has no roof quad\n"), 0;
    if (is_fin) return add_fin(d, top, d->polygons[top].texture) && same_bank(bank->data, bank->size, out, out_len);
    const int t = add_fin_texture(d, d->polygons[top].texture);
    return t >= 0 && add_fin(d, top, (uint8_t)t) && bank_with_fin(bank->data, bank->size, fin_px, out, out_len);
}

int main(int argc, char **argv)
{
    if (argc != 3) return fprintf(stderr, "usage: model_build_piece PIECE_DIR same|fin|texture|big\n"), 2;
    Piece p = { argv[1], "", "" };
    if (!find_piece(&p)) return 1;
    size_t size = 0, bank_len = 0;
    uint8_t *cfm = read_all(p.path, &size), *bank = NULL;
    static uint8_t fin_px[FIN_W * FIN_H];
    if (cfm == NULL) return fprintf(stderr, "model_build_piece: cannot read %s\n", p.path), 1;
    Chunk root, high, high_bank;
    Mesh m;
    MeshDesc d;
    ExchangeError err = { { 0 } };
    int ok = chunk_open(&root, cfm, size) == CHUNK_OK && chunk_child(&root, CFM_CHILD_MESH_HIGH, &high) == CHUNK_OK &&
             chunk_child(&root, CFM_CHILD_BITMAPS_HIGH, &high_bank) == CHUNK_OK && mesh_open(&m, &high) == MESH_OK;
    if (!ok) fprintf(stderr, "model_build_piece: %s is not a car model\n", p.path);
    if (ok && !mesh_describe(&m, &d, &err)) fprintf(stderr, "model_build_piece: the mesh: %s\n", err.message), ok = 0;
    if (ok) {
        ok = apply(argv[2], &d, &high_bank, fin_px, &bank, &bank_len) && rebuild(&p, cfm, size, &d, bank, bank_len);
        if (ok) printf("model_build_piece: %s %s: %u polygons, %u vertices, %u textures\n", p.stem, argv[2], d.polygon_count, d.vertex_count, d.texture_count);
        mesh_desc_free(&d);
    }
    free(bank);
    free(cfm);
    return ok ? 0 : 1;
}
