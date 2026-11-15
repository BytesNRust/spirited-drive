/* dep59 tools B2, Task 1: what an importer must know of the car model, measured on the CD's 39 CFMs and the race cars' PDNs
 * (docs/modding/car-model.md, "Importing a model"). Each fact is checked with the count of files it holds in; what has no known
 * value yet is printed. Skipped without the CD. */
#include "formats/cfm_build.h"
#include "formats/chunk.h"
#include "formats/mesh.h"
#include "formats/mesh_build.h"
#include "formats/pdn.h"
#include "formats/shpi_build.h"
#include "test.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

enum { MAX_ENTRIES = 64, SIZE_SHIFT = 9 };

static const char *const RACE[] = { "TSUPRA", "LDIABL", "P911", "CZR1", "F512TR", "DVIPER", "ANSX", "MRX7" };
enum { RACE_CARS = sizeof RACE / sizeof RACE[0] };

typedef struct {
    int meshes, inward_meshes;            /* R1: meshes whose unreversed polygons mostly wind inward by the right hand */
    long inward, outward, rev_inward, rev_outward;
    int axes_cars;                        /* R1: race cars with the left wheel at -x, the wheels below the box's middle, the front at +z */
    int refs, uv_in, uv_edge;             /* R2: texture references used with uvs; inside 0..w-1 x 0..h-1; touching w or h */
    int tyre_cars;                        /* R3: cars whose frntires and rertires are 2 quads each, no uvs, in outside */
    int point_cars;                       /* R5: cars whose 8 light points are corners of the wheel polygons and smok a used vertex */
    int body_sizes;                       /* R7: the race PDNs' half sizes within 2 % of the model's */
    int pal_22, pal_24, xxx_22, xxx_24, xxx_7b;   /* R8 */
} Tally;

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

static uint8_t *read_cd(const char *file, size_t *size)
{
    char path[512];
    snprintf(path, sizeof path, "%s/original/SIMDATA/CARFAMS/%s", TNFS_SOURCE_DIR, file);
    return read_all(path, size);
}

static int is_race(const char *stem)
{
    for (int i = 0; i < RACE_CARS; i++)
        if (strcmp(stem, RACE[i]) == 0) return 1;
    return 0;
}

static const ShpiDescEntry *entry_named(const ShpiDescEntry *e, size_t n, const char name[4])
{
    for (size_t i = 0; i < n; i++)
        if (memcmp(e[i].name, name, 4) == 0) return &e[i];
    return NULL;
}

/* R8: the types of !PAL and !xxx in a bank */
static void tally_bank(const ShpiDescEntry *e, size_t n, Tally *t)
{
    const ShpiDescEntry *pal = entry_named(e, n, "!PAL"), *own = entry_named(e, n, "!xxx");
    if (pal != NULL) t->pal_22 += pal->type == 0x22, t->pal_24 += pal->type == 0x24;
    if (own != NULL) t->xxx_22 += own->type == 0x22, t->xxx_24 += own->type == 0x24, t->xxx_7b += own->type == 0x7b;
}

static void box(const MeshDesc *d, int32_t lo[3], int32_t hi[3])
{
    const uint32_t n = d->real_vertices && d->real_vertices < d->vertex_count ? d->real_vertices : d->vertex_count;
    for (int k = 0; k < 3; k++) lo[k] = INT32_MAX, hi[k] = INT32_MIN;
    for (uint32_t i = 0; i < n; i++) {
        const int32_t v[3] = { d->vertices[i].x, d->vertices[i].y, d->vertices[i].z };
        for (int k = 0; k < 3; k++) lo[k] = v[k] < lo[k] ? v[k] : lo[k], hi[k] = v[k] > hi[k] ? v[k] : hi[k];
    }
}

/* R1: (v1 - v0) x (v2 - v0) against the way from the box's middle to the polygon's middle */
static void tally_winding(const MeshDesc *d, Tally *t)
{
    int32_t lo[3], hi[3];
    box(d, lo, hi);
    long in = 0, out = 0;
    for (uint32_t i = 0; i < d->polygon_count; i++) {
        const MeshDescPolygon *p = &d->polygons[i];
        const int n = p->flags & 7;
        if (n < 3) continue;
        const MeshVertex *a = &d->vertices[p->vertex[0]], *b = &d->vertices[p->vertex[1]], *c = &d->vertices[p->vertex[2]];
        const double u[3] = { b->x - a->x, b->y - a->y, b->z - a->z }, v[3] = { c->x - a->x, c->y - a->y, c->z - a->z };
        const double nx = u[1] * v[2] - u[2] * v[1], ny = u[2] * v[0] - u[0] * v[2], nz = u[0] * v[1] - u[1] * v[0];
        double mid[3] = { 0, 0, 0 };
        for (int k = 0; k < n; k++)
            mid[0] += d->vertices[p->vertex[k]].x, mid[1] += d->vertices[p->vertex[k]].y, mid[2] += d->vertices[p->vertex[k]].z;
        const double dot = nx * (mid[0] / n - (lo[0] + hi[0]) / 2.0) + ny * (mid[1] / n - (lo[1] + hi[1]) / 2.0) +
                           nz * (mid[2] / n - (lo[2] + hi[2]) / 2.0);
        const int reversed = (p->unknown_1 & 0x02) != 0;
        if (dot < 0) reversed ? t->rev_inward++ : (in++, t->inward++);
        else if (dot > 0) reversed ? t->rev_outward++ : (out++, t->outward++);
    }
    t->meshes++;
    t->inward_meshes += in > out;
}

/* R2: the largest u and v of each texture reference's polygons against its entry's width and height */
static void tally_uvs(const char *stem, const MeshDesc *d, const ShpiDescEntry *e, size_t n, Tally *t)
{
    for (uint32_t r = 0; r < d->texture_count; r++) {
        const ShpiDescEntry *x = entry_named(e, n, d->textures[r].name);
        int32_t mu = -1, mv = -1, least = 0;
        for (uint32_t i = 0; i < d->polygon_count; i++) {
            const MeshDescPolygon *p = &d->polygons[i];
            for (int k = 0; p->texture == r && p->has_uv && k < (p->flags & 7); k++) {
                const MeshUv u = d->uvs[p->uv[k]];
                mu = u.u > mu ? u.u : mu, mv = u.v > mv ? u.v : mv;
                least = u.u < least ? u.u : u.v < least ? u.v : least;
            }
        }
        if (x == NULL || x->type != 0x7b || mu < 0) continue;
        t->refs++;
        if (least >= 0 && mu < x->width && mv < x->height) t->uv_in++;
        else if (least >= 0 && mu <= x->width && mv <= x->height) t->uv_edge++;
        else fprintf(stderr, "car_import_layout: %s '%.4s' %u x %u: uvs to %d, %d\n", stem, x->name, x->width, x->height, mu, mv);
    }
}

static int part_side(const MeshDesc *d, const char *prefix, double *x, double *y, double *z)
{
    for (uint32_t i = 0; i < d->part_count; i++) {
        const MeshDescPart *p = &d->parts[i];
        if (strncmp(p->name, prefix, strlen(prefix)) != 0 || p->count == 0) continue;
        *x = *y = *z = 0;
        for (uint32_t k = 0; k < p->count; k++)
            *x += d->vertices[p->vertices[k]].x, *y += d->vertices[p->vertices[k]].y, *z += d->vertices[p->vertices[k]].z;
        *x /= p->count, *y /= p->count, *z /= p->count;
        return 1;
    }
    return 0;
}

/* R1: the left wheel at -x, the wheels below the box's middle, the front wheels and the front texture at +z */
static int axes_hold(const MeshDesc *d)
{
    double lx, ly, lz, rx, ry, rz;
    int32_t lo[3], hi[3];
    if (!part_side(d, "left_tu", &lx, &ly, &lz) || !part_side(d, "right_tu", &rx, &ry, &rz)) return 0;
    box(d, lo, hi);
    double front = 0;
    int corners = 0;
    for (uint32_t i = 0; i < d->polygon_count; i++) {
        const MeshDescPolygon *p = &d->polygons[i];
        if (p->texture >= d->texture_count || memcmp(d->textures[p->texture].name, "frnt", 4) != 0) continue;
        for (int k = 0; k < (p->flags & 7); k++, corners++) front += d->vertices[p->vertex[k]].z;
    }
    return lx < 0 && rx > 0 && ly < (lo[1] + hi[1]) / 2.0 && ry < (lo[1] + hi[1]) / 2.0 && lz > 0 && rz > 0 && corners > 0 && front > 0;
}

static int group_of(const MeshDesc *d, uint32_t polygon, const char *name)
{
    for (uint32_t g = 0; g < d->group_count; g++) {
        if (strncmp(d->groups[g].name, name, MESH_NAME_SIZE) != 0) continue;
        for (uint32_t k = 0; k < d->groups[g].count; k++)
            if (d->groups[g].polygons[k] == polygon) return 1;
    }
    return 0;
}

static int texture_labelled(const MeshDesc *d, uint8_t texture, const char *label)
{
    return texture < d->texture_count && strncmp(d->textures[texture].label, label, 8) == 0;
}

/* R3: frntires and rertires each on 2 quads with no uvs, in outside */
static int tyres_hold(const MeshDesc *d)
{
    static const char *const LABELS[2] = { "frntires", "rertires" };
    for (int l = 0; l < 2; l++) {
        int quads = 0;
        for (uint32_t i = 0; i < d->polygon_count; i++) {
            const MeshDescPolygon *p = &d->polygons[i];
            if (!texture_labelled(d, p->texture, LABELS[l])) continue;
            if ((p->flags & 7) != 4 || p->has_uv || !group_of(d, i, "outside")) return 0;
            quads++;
        }
        if (quads != 2) return 0;
    }
    return 1;
}

/* R5: the 8 light points are corners of the wheel polygons; smok is a vertex some polygon uses */
static int points_hold(const MeshDesc *d)
{
    int lights = 0, smoke = 0;
    for (uint32_t c = 0; c < d->point_count; c++) {
        const char *n = d->points[c].name;
        const int light = strlen(n) == 3 && strchr("fFrR", n[0]) && strchr("lLrR", n[1]) && strchr("01", n[2]);
        const int is_smoke = strncmp(n, "smok", 4) == 0;
        for (uint32_t i = 0; (light || is_smoke) && i < d->polygon_count; i++) {
            const MeshDescPolygon *p = &d->polygons[i];
            const int wheel = texture_labelled(d, p->texture, "frntires") || texture_labelled(d, p->texture, "rertires");
            int on = 0;
            for (int k = 0; k < (p->flags & 7); k++) on |= p->vertex[k] == d->points[c].value;
            if (on && light && wheel) { lights++; break; }
            if (on && is_smoke) { smoke = 1; break; }
        }
    }
    return lights == 8 && smoke;
}

/* R7: the PDN's half sizes (words 0, 4, 8) against the model's, as car_piece_check measures them */
static void tally_body(const char *stem, const MeshDesc *d, Tally *t)
{
    char file[32];
    snprintf(file, sizeof file, "%s.PDN", stem);
    size_t n = 0;
    uint8_t *raw = read_cd(file, &n), rec[PDN_SIZE];
    if (raw == NULL || !pdn_read(rec, raw, n)) { free(raw); return; }
    int32_t lo[3], hi[3];
    box(d, lo, hi);
    fprintf(stderr, "car_import_layout: R7 %-6s", stem);
    for (int k = 0; k < 3; k++) {
        const long half = (((long)hi[k] - lo[k]) << SIZE_SHIFT) / 2, body = pdn_i32(rec, 4 * k);
        fprintf(stderr, "  %c %ld / %ld = %.1f %%", "xyz"[k], body, half, half ? 100.0 * body / half : 0);
        t->body_sizes += labs(body - half) * 50 <= half;
    }
    fprintf(stderr, "\n");
    free(raw);
}

static void measure(const char *file, const uint8_t *cfm, size_t size, Tally *t)
{
    char stem[16];
    snprintf(stem, sizeof stem, "%.*s", (int)(strlen(file) - 4), file);
    const uint8_t *child[CFM_CHILDREN];
    size_t len[CFM_CHILDREN];
    ExchangeError err = { { 0 } };
    if (!cfm_describe(cfm, size, child, len, &err)) { CHECK(0); return; }
    for (int level = 0; level < 2; level++) {
        ShpiDescEntry e[MAX_ENTRIES];
        size_t n = 0;
        Chunk c = { CHUNK_KIND_MESH, child[2 * level], len[2 * level], 0 };
        Mesh m;
        MeshDesc d;
        CHECK(shpi_describe(child[2 * level + 1], len[2 * level + 1], e, MAX_ENTRIES, &n, &err));
        tally_bank(e, n, t);
        if (level == 1 && strcmp(stem, "LDIABLO") == 0) continue;   /* its low mesh's texture records are 19 bytes (car-model.md) */
        c.size = child[2 * level][4] | child[2 * level][5] << 8 | child[2 * level][6] << 16 | (size_t)child[2 * level][7] << 24;
        if (mesh_open(&m, &c) != MESH_OK || !mesh_describe(&m, &d, &err)) { CHECK(0); continue; }
        tally_winding(&d, t);
        tally_uvs(stem, &d, e, n, t);
        if (level == 0 && (is_race(stem) || strcmp(stem, "TRAFFC") == 0 || strcmp(stem, "WARRIOR") == 0)) {
            t->axes_cars += is_race(stem) && axes_hold(&d);
            t->tyre_cars += tyres_hold(&d);
            t->point_cars += points_hold(&d);
            if (is_race(stem)) tally_body(stem, &d, t);
        }
        mesh_desc_free(&d);
    }
}

int main(void)
{
    char dir[512];
    snprintf(dir, sizeof dir, "%s/original/SIMDATA/CARFAMS", TNFS_SOURCE_DIR);
    DIR *h = opendir(dir);
    if (h == NULL) {
        fprintf(stderr, "car_import_layout: no CD in original/, skipped\n");
        return TEST_REPORT();
    }
    Tally t;
    memset(&t, 0, sizeof t);
    int files = 0;
    struct dirent *de;
    while ((de = readdir(h)) != NULL) {
        const size_t n = strlen(de->d_name);
        if (n < 5 || strcasecmp(de->d_name + n - 4, ".CFM") != 0) continue;
        size_t size = 0;
        uint8_t *cfm = read_cd(de->d_name, &size);
        files++;
        if (cfm != NULL) measure(de->d_name, cfm, size, &t);
        free(cfm);
    }
    closedir(h);
    fprintf(stderr, "car_import_layout: R1 unreversed polygons %ld inward, %ld outward; reversed (byte 1 bit 0x02) %ld inward, %ld outward\n",
            t.inward, t.outward, t.rev_inward, t.rev_outward);
    CHECK_EQ_INT(files, 39);
    CHECK_EQ_INT(t.meshes, 77);
    CHECK_EQ_INT(t.inward_meshes, 77);
    CHECK_EQ_INT(t.axes_cars, RACE_CARS);
    CHECK_EQ_INT(t.refs, 343);
    CHECK_EQ_INT(t.uv_in, 182);
    CHECK_EQ_INT(t.uv_edge, 159);
    CHECK_EQ_INT(t.tyre_cars, RACE_CARS + 2);
    CHECK_EQ_INT(t.point_cars, RACE_CARS + 2);
    CHECK_EQ_INT(t.body_sizes, 3 * RACE_CARS);
    CHECK_EQ_INT(t.pal_22, 21);
    CHECK_EQ_INT(t.pal_24, 57);
    CHECK_EQ_INT(t.xxx_22, 12);
    CHECK_EQ_INT(t.xxx_24, 3);
    CHECK_EQ_INT(t.xxx_7b, 25);
    return TEST_REPORT();
}
