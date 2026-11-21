/* dep59 tools B2, Task 5: a car's meshes, banks and body size made from an OBJ and a donor (formats/car_import.h), the Viper the
 * donor. An OBJ the test writes (a body, a seat inside, four wheels of 8 sides; no point, no light) gives the groups, the steered
 * wheels, the points and lights made and noted; the CFM passes car_piece_check with no finding; the same input, the same bytes.
 * Raw mode gives the Viper back from its own export, takes an export with a quad added, and refuses an export with no lists.tsv.
 * The refusals of the plan's Review Focus. Skipped without the CD. */
#include "formats/car_import.h"
#include "formats/car_piece.h"
#include "formats/car_resize.h"
#include "formats/car_texture_add.h"
#include "formats/cfm_build.h"
#include "formats/chunk.h"
#include "formats/mesh.h"
#include "formats/mesh_exchange.h"
#include "formats/obj_read.h"
#include "formats/pdn.h"
#include "formats/png.h"
#include "test.h"
#include "blender_like.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *data; size_t len, cap; } Buf;

static void put(Buf *b, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    char line[512];
    const int n = vsnprintf(line, sizeof line, format, ap);
    va_end(ap);
    if (b->len + (size_t)n + 1 > b->cap) b->cap = (b->len + (size_t)n + 1) * 2, b->data = realloc(b->data, b->cap);
    memcpy(b->data + b->len, line, (size_t)n + 1);
    b->len += (size_t)n;
}

/* a box, Blender's frame, faces counter-clockwise from outside, each corner its uv 1..4 */
static void box(Buf *b, int *v, double x0, double x1, double y0, double y1, double z0, double z1)
{
    for (int k = 0; k < 8; k++) put(b, "v %g %g %g\n", k & 1 ? x1 : x0, k & 2 ? y1 : y0, k & 4 ? z1 : z0);
    static const int F[6][4] = { { 0, 4, 6, 2 }, { 1, 3, 7, 5 }, { 0, 1, 5, 4 }, { 2, 6, 7, 3 }, { 0, 2, 3, 1 }, { 4, 5, 7, 6 } };
    for (int f = 0; f < 6; f++) put(b, "f %d/1 %d/2 %d/3 %d/4\n", *v + F[f][0], *v + F[f][1], *v + F[f][2], *v + F[f][3]);
    *v += 8;
}

/* a wheel: 8 sides about an axis along x, radius 35, its bottom at y 0; 16 vertices, 8 quads and two octagons */
static void wheel(Buf *b, int *v, const char *name, double x0, double x1, double z)
{
    put(b, "o %s\n", name);
    for (int ring = 0; ring < 2; ring++)
        for (int k = 0; k < 8; k++) {
            const double a = 3.14159265358979 * (2 * k + 1) / 8;
            put(b, "v %g %g %g\n", ring ? x1 : x0, 35 + 35 * sin(a), z + 35 * cos(a));
        }
    for (int k = 0; k < 8; k++) put(b, "f %d %d %d %d\n", *v + k, *v + (k + 1) % 8, *v + 8 + (k + 1) % 8, *v + 8 + k);
    put(b, "f %d %d %d %d %d %d %d %d\n", *v + 7, *v + 6, *v + 5, *v + 4, *v + 3, *v + 2, *v + 1, *v);
    put(b, "f %d %d %d %d %d %d %d %d\n", *v + 8, *v + 9, *v + 10, *v + 11, *v + 12, *v + 13, *v + 14, *v + 15);
    *v += 16;
}

/* the test's car: `extra` adds a second wheel_fl, `no_fr` leaves wheel_fr out, `twins` gives two materials of one first 4 */
static char *car_obj(int extra, int no_fr, int twins)
{
    Buf b = { NULL, 0, 0 };
    int v = 1;
    put(&b, "mtllib car.mtl\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\no body\nusemtl paint\n");
    box(&b, &v, -100, 100, 20, 130, -260, 260);
    put(&b, "o inside_seat\nusemtl %s\n", twins ? "paintwork" : "seat");
    box(&b, &v, -30, 30, 40, 70, -20, 40);
    wheel(&b, &v, "wheel_fl", -120, -90, -170);
    if (!no_fr) wheel(&b, &v, "Wheel_FR", 90, 120, -170);
    wheel(&b, &v, "wheel_rl", -120, -90, 170);
    wheel(&b, &v, "wheel_rr", 90, 120, 170);
    if (extra) wheel(&b, &v, "wheel_fl.001", -120, -90, -100);
    return b.data;
}

static const char CAR_MTL[] = "newmtl paint\nmap_Kd paint.png\nnewmtl seat\nmap_Kd seat.png\nnewmtl paintwork\nmap_Kd seat.png\n";

static uint8_t *read_cd(const char *file, size_t *size)
{
    char path[512];
    snprintf(path, sizeof path, "%s/original/%s", TNFS_SOURCE_DIR, file);
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

static void add_png(FileSet *set, const char *name, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t alpha)
{
    uint8_t *rgba = malloc((size_t)w * h * 4), *png = NULL;
    for (int k = 0; k < w * h; k++) rgba[4 * k] = r, rgba[4 * k + 1] = g, rgba[4 * k + 2] = b, rgba[4 * k + 3] = k == 0 ? alpha : 255;
    size_t len = 0;
    CHECK(png_write_rgba(rgba, (uint32_t)w, (uint32_t)h, &png, &len) == PNG_OK && fileset_add_owned(set, name, png, len));
    free(rgba);
}

static int made_notes;
static void count_note(void *ctx, const char *text)
{
    (void)ctx;
    made_notes += strncmp(text, "made ", 5) == 0;
}

static CarImportIn input(const uint8_t *donor, size_t donor_len, const ObjModel *high, const FileSet *pngs)
{
    CarImportIn in;
    memset(&in, 0, sizeof in);
    in.donor_cfm = donor, in.donor_cfm_len = donor_len, in.high = high, in.high_pngs = pngs, in.scale = 1;
    in.note = count_note;
    return in;
}

/* the donor's mesh and bank of level 0 */
typedef struct { MeshDesc mesh; ShpiDescEntry bank[64]; size_t bank_n; } Donor;

static int donor_of(const uint8_t *cfm, size_t size, Donor *d)
{
    const uint8_t *child[CFM_CHILDREN];
    size_t len[CFM_CHILDREN];
    ExchangeError err = { { 0 } };
    Chunk c;
    Mesh m;
    return cfm_describe(cfm, size, child, len, &err) && chunk_open(&c, child[0], len[0]) == CHUNK_OK && mesh_open(&m, &c) == MESH_OK &&
           mesh_describe(&m, &d->mesh, &err) && shpi_describe(child[1], len[1], d->bank, 64, &d->bank_n, &err);
}

static int group_index(const MeshDesc *d, const char *name)
{
    for (uint32_t g = 0; g < d->group_count; g++)
        if (strncmp(d->groups[g].name, name, MESH_NAME_SIZE) == 0) return (int)g;
    return -1;
}

static void check_mesh(const MeshDesc *m, uint32_t obj_vertices)
{
    CHECK(group_index(m, "inside") >= 0 && group_index(m, "surface") >= 0 && group_index(m, "outside") >= 0);
    uint8_t *seen = calloc(m->polygon_count, 1);
    for (uint32_t g = 0; g < m->group_count; g++)
        for (uint32_t k = 0; k < m->groups[g].count; k++)
            if (m->groups[g].polygons[k] < m->polygon_count) seen[m->groups[g].polygons[k]]++;
    int once = 1;
    for (uint32_t i = 0; i < m->polygon_count; i++) once &= seen[i] == 1;
    CHECK(once);
    free(seen);
    if (group_index(m, "inside") >= 0) CHECK_EQ_INT((int)m->groups[group_index(m, "inside")].count, 6);
    CHECK_EQ_INT((int)m->part_count, 2);
    for (uint32_t p = 0; p < m->part_count && p < 2; p++) {
        const MeshDescPart *part = &m->parts[p];
        CHECK(strncmp(part->name, p == 0 ? "left_tur" : "right_tu", 8) == 0);
        CHECK_EQ_INT((int)part->count, 16);
        for (uint32_t k = 0; k < part->count; k++) CHECK_EQ_INT(m->vertices[part->first_offset_vertex + k].y, 0);
    }
    CHECK_EQ_INT((int)m->real_vertices, (int)obj_vertices + 1);   /* smok, made */
    CHECK_EQ_INT((int)m->vertex_count, (int)m->real_vertices + 32);
    CHECK_EQ_INT((int)m->point_count, 9);
    CHECK_EQ_INT((int)m->label_count, 4);
}

/* the race draws a polygon with the transparent mapper when its flag byte's bits 3..6 say 1 (object3d.c draw_polygon): the seat,
 * whose texture has a clear texel, so; the body, all opaque, not */
static void check_modes(const MeshDesc *m)
{
    const int inside = group_index(m, "inside"), surface = group_index(m, "surface");
    if (inside < 0 || surface < 0) return;
    for (uint32_t k = 0; k < m->groups[inside].count; k++) CHECK_EQ_INT((m->polygons[m->groups[inside].polygons[k]].flags >> 3) & 0xF, 1);
    for (uint32_t k = 0; k < m->groups[surface].count; k++) CHECK_EQ_INT((m->polygons[m->groups[surface].polygons[k]].flags >> 3) & 0xF, 0);
}

/* the CFM as a piece with the donor's PDN (its body sizes the new ones) and PBS */
static void check_piece(const uint8_t *cfm, size_t len, const int32_t half[3])
{
    size_t n = 0;
    uint8_t *pdn = read_cd("SIMDATA/CARFAMS/DVIPER.PDN", &n), rec[PDN_SIZE], *pbs = read_cd("SIMDATA/CARSPECS/DVIPER.PBS", &n);
    CHECK(pdn != NULL && pbs != NULL && pdn_read(rec, pdn, n ? n : PDN_SIZE));
    for (int k = 0; k < 3; k++)
        for (int i = 0; i < 4; i++) rec[4 * k + i] = (uint8_t)((uint32_t)half[k] >> (8 * i));
    FileSet set;
    fileset_init(&set);
    CHECK(fileset_add(&set, "SIMDATA/CARFAMS/TESTCAR.CFM", cfm, len) && fileset_add(&set, "SIMDATA/CARFAMS/TESTCAR.PDN", rec, PDN_SIZE) &&
          fileset_add(&set, "SIMDATA/CARSPECS/TESTCAR.PBS", pbs, n));
    add_png(&set, "card.png", 309, 166, 40, 40, 40, 255);   /* dep59 step 3: a piece brings its card (car new writes one) */
    static const char GOOD[] = "id=test-car\nname=Test Car\nversion=1\ntype=car\nstem=TESTCAR\nshort_name=TEST\nsound_from=viper\n";
    ModInfo card;
    char why[200];
    CHECK(mod_ini_parse((const uint8_t *)GOOD, strlen(GOOD), &card, why, sizeof why));
    Report r;
    report_init(&r);
    car_piece_check(&card, &set, &r);
    if (r.count > 0) report_print(&r, VALIDATE_COMPATIBLE, stderr);
    CHECK_EQ_INT((int)r.count, 0);
    report_free(&r);
    fileset_free(&set);
    free(pdn), free(pbs);
}

static void test_obj(const uint8_t *donor, size_t donor_len, const FileSet *pngs)
{
    char *text = car_obj(0, 0, 0);
    ObjModel obj;
    ExchangeError err = { { 0 } };
    CHECK(obj_read((const uint8_t *)text, strlen(text), (const uint8_t *)CAR_MTL, strlen(CAR_MTL), &obj, &err));
    CarImportIn in = input(donor, donor_len, &obj, pngs);
    Donor d;
    CHECK(donor_of(donor, donor_len, &d));
    MeshDesc m;
    CarImportBank bank;
    made_notes = 0;
    CHECK(car_import_mesh(&in, 0, &d.mesh, d.bank, d.bank_n, &m, &bank, &err));
    if (err.message[0]) fprintf(stderr, "car_import: %s\n", err.message);
    if (err.message[0] == '\0') {
        check_mesh(&m, obj.vertex_count);
        CHECK_EQ_INT(made_notes, 13);
        check_modes(&m);
        mesh_desc_free(&m);
        car_import_bank_free(&bank);
    }
    uint8_t *cfm = NULL, *again = NULL;
    size_t len = 0, again_len = 0;
    int32_t half[3], half2[3];
    CHECK(car_import(&in, &cfm, &len, half, &err) && car_import(&in, &again, &again_len, half2, &err));
    if (cfm != NULL && again != NULL) {
        CHECK(len == again_len && memcmp(cfm, again, len) == 0);   /* the same input, the same bytes */
        check_piece(cfm, len, half);
    } else {
        fprintf(stderr, "car_import: %s\n", err.message);
    }
    free(cfm), free(again);
    mesh_desc_free(&d.mesh);
    obj_free(&obj);
    free(text);
}

/* car_import of the test's car with the variation refused, `word` in the message */
static void refused_obj(const uint8_t *donor, size_t n, const FileSet *pngs, int extra, int no_fr, const char *word)
{
    char *text = car_obj(extra, no_fr, 0);
    ObjModel obj;
    ExchangeError err = { { 0 } };
    CHECK(obj_read((const uint8_t *)text, strlen(text), (const uint8_t *)CAR_MTL, strlen(CAR_MTL), &obj, &err));
    CarImportIn in = input(donor, n, &obj, pngs);
    uint8_t *cfm = NULL;
    size_t len = 0;
    int32_t half[3];
    const int ok = car_import(&in, &cfm, &len, half, &err);
    if (ok || strstr(err.message, word) == NULL) fprintf(stderr, "car_import: wanted a refusal naming %s: %s\n", word, ok ? "made" : err.message);
    CHECK(!ok && strstr(err.message, word) != NULL && cfm == NULL);
    free(cfm);
    obj_free(&obj);
    free(text);
}

/* two materials whose first 4 characters are one ("paint", "paintwork"): two entries, neither the donor's nor '!' first */
static void test_twins(const uint8_t *donor, size_t n, const FileSet *pngs)
{
    char *text = car_obj(0, 0, 1);
    ObjModel obj;
    ExchangeError err = { { 0 } };
    CHECK(obj_read((const uint8_t *)text, strlen(text), (const uint8_t *)CAR_MTL, strlen(CAR_MTL), &obj, &err));
    CarImportIn in = input(donor, n, &obj, pngs);
    Donor d;
    CHECK(donor_of(donor, n, &d));
    MeshDesc m;
    CarImportBank bank;
    CHECK(car_import_mesh(&in, 0, &d.mesh, d.bank, d.bank_n, &m, &bank, &err));
    int paint = 0;
    for (size_t i = 0; err.message[0] == '\0' && i < bank.count; i++) {
        for (size_t j = 0; j < i; j++) CHECK(memcmp(bank.entry[i].name, bank.entry[j].name, 4) != 0);
        paint += memcmp(bank.entry[i].name, "pain", 4) == 0;
        CHECK(i < 2 || bank.entry[i].name[0] != '!');
    }
    CHECK_EQ_INT(paint, 1);
    if (err.message[0] == '\0') mesh_desc_free(&m), car_import_bank_free(&bank);
    mesh_desc_free(&d.mesh);
    obj_free(&obj);
    free(text);
}

/* the exporter's OBJ is refused without raw, explaining */
static void test_exporter_obj(const uint8_t *donor, size_t n, const FileSet *exported)
{
    const SetFile *o = fileset_find(exported, "mesh.obj"), *t = fileset_find(exported, "mesh.mtl");
    ObjModel obj;
    ExchangeError err = { { 0 } };
    CHECK(o != NULL && t != NULL && obj_read(o->data, o->size, t->data, t->size, &obj, &err));
    CarImportIn in = input(donor, n, &obj, NULL);
    uint8_t *cfm = NULL;
    size_t len = 0;
    int32_t half[3];
    CHECK(!car_import(&in, &cfm, &len, half, &err) && strstr(err.message, "raw") != NULL);
    obj_free(&obj);
}

/* raw: `files` (an export of the Viper's high mesh, perhaps edited) made the high mesh; 0 when refused, the reason in `err` */
static int raw_mesh(const uint8_t *donor, size_t n, const FileSet *files, MeshDesc *m, ExchangeError *err)
{
    const SetFile *o = fileset_find(files, "mesh.obj"), *t = fileset_find(files, "mesh.mtl");
    ObjModel obj;
    if (o == NULL || t == NULL || !obj_read(o->data, o->size, t->data, t->size, &obj, err)) return 0;
    CarImportIn in = input(donor, n, &obj, NULL);
    in.raw = 1, in.high_tables = files;
    Donor d;
    CarImportBank bank;
    int ok = donor_of(donor, n, &d);
    ok = ok && car_import_mesh(&in, 0, &d.mesh, d.bank, d.bank_n, m, &bank, err);
    if (ok) car_import_bank_free(&bank);
    mesh_desc_free(&d.mesh);
    obj_free(&obj);
    return ok;
}

/* raw with `obj` (an OBJ's text) as the high mesh and `files` the tables beside it */
static int raw_mesh_from(const uint8_t *donor, size_t n, const char *obj, const FileSet *files, MeshDesc *m, ExchangeError *err)
{
    ObjModel model;
    if (!obj_read((const uint8_t *)obj, strlen(obj), NULL, 0, &model, err)) return 0;
    CarImportIn in = input(donor, n, &model, NULL);
    in.raw = 1, in.high_tables = files;
    Donor d;
    CarImportBank bank;
    int ok = donor_of(donor, n, &d);
    ok = ok && car_import_mesh(&in, 0, &d.mesh, d.bank, d.bank_n, m, &bank, err);
    if (ok) car_import_bank_free(&bank);
    mesh_desc_free(&d.mesh);
    obj_free(&model);
    return ok;
}

static int same_mesh(const MeshDesc *a, const MeshDesc *b)
{
    int same = a->vertex_count == b->vertex_count && a->uv_count == b->uv_count && a->polygon_count == b->polygon_count &&
               a->texture_count == b->texture_count && a->group_count == b->group_count && a->part_count == b->part_count &&
               a->point_count == b->point_count && a->label_count == b->label_count && a->real_vertices == b->real_vertices;
    same = same && memcmp(a->vertices, b->vertices, a->vertex_count * sizeof *a->vertices) == 0 &&
           memcmp(a->uvs, b->uvs, a->uv_count * sizeof *a->uvs) == 0 &&
           memcmp(a->textures, b->textures, a->texture_count * sizeof *a->textures) == 0;
    for (uint32_t i = 0; same && i < a->polygon_count; i++) same = memcmp(&a->polygons[i], &b->polygons[i], sizeof a->polygons[i]) == 0;
    for (uint32_t i = 0; same && i < a->group_count; i++)
        same = strncmp(a->groups[i].name, b->groups[i].name, MESH_NAME_SIZE) == 0 && a->groups[i].count == b->groups[i].count &&
               memcmp(a->groups[i].polygons, b->groups[i].polygons, a->groups[i].count * 4) == 0;
    for (uint32_t i = 0; same && i < a->part_count; i++)
        same = strncmp(a->parts[i].name, b->parts[i].name, 8) == 0 && a->parts[i].count == b->parts[i].count &&
               a->parts[i].first_offset_vertex == b->parts[i].first_offset_vertex &&
               memcmp(a->parts[i].vertices, b->parts[i].vertices, a->parts[i].count * 4) == 0;
    for (uint32_t i = 0; same && i < a->point_count; i++)
        same = strncmp(a->points[i].name, b->points[i].name, 8) == 0 && a->points[i].value == b->points[i].value;
    for (uint32_t i = 0; same && i < a->label_count; i++)
        same = strncmp(a->labels[i].name, b->labels[i].name, 8) == 0 && a->labels[i].polygon == b->labels[i].polygon;
    return same;
}

/* `files` with `name` replaced by its text plus `more` */
static void append(FileSet *files, const char *name, const char *more)
{
    const SetFile *f = fileset_find(files, name);
    if (f == NULL) { CHECK(0); return; }
    const size_t n = f->size + strlen(more);
    uint8_t *text = malloc(n);
    memcpy(text, f->data, f->size), memcpy(text + f->size, more, strlen(more));
    FileSet next;
    fileset_init(&next);
    for (size_t i = 0; i < files->count; i++)
        if (strcmp(files->file[i].name, name) != 0) fileset_add(&next, files->file[i].name, files->file[i].data, files->file[i].size);
    fileset_add_owned(&next, name, text, n);
    fileset_free(files);
    *files = next;
}

/* the surface group's row of lists.tsv with polygon `p` added */
static void add_to_surface(FileSet *files, uint32_t p)
{
    const SetFile *f = fileset_find(files, "mesh.lists.tsv");
    if (f == NULL) { CHECK(0); return; }
    char *text = malloc(f->size + 16), *row = NULL;
    memcpy(text, f->data, f->size), text[f->size] = '\0';
    for (char *at = strstr(text, "\ngroup\t"); at != NULL && row == NULL; at = strstr(at + 1, "\ngroup\t"))
        if (strncmp(strchr(strchr(at + 1, '\t') + 1, '\t') + 1, "surface\t", 8) == 0) row = at + 1;
    CHECK(row != NULL);
    if (row != NULL) {
        char *end = strchr(row, '\n');
        char tail[16];
        snprintf(tail, sizeof tail, " %u", p);
        memmove(end + strlen(tail), end, strlen(end) + 1), memcpy(end, tail, strlen(tail));
        FileSet next;
        fileset_init(&next);
        for (size_t i = 0; i < files->count; i++)
            if (strcmp(files->file[i].name, "mesh.lists.tsv") != 0) fileset_add(&next, files->file[i].name, files->file[i].data, files->file[i].size);
        fileset_add(&next, "mesh.lists.tsv", (uint8_t *)text, strlen(text));
        fileset_free(files);
        *files = next;
    }
    free(text);
}

static void test_raw(const uint8_t *donor, size_t n)
{
    Donor d;
    CHECK(donor_of(donor, n, &d));
    const uint8_t *child[CFM_CHILDREN];
    size_t len[CFM_CHILDREN];
    ExchangeError err = { { 0 } };
    FileSet files;
    fileset_init(&files);
    CHECK(cfm_describe(donor, n, child, len, &err) && mesh_export(child[0], len[0], "mesh", NULL, 0, &files, &err));
    test_exporter_obj(donor, n, &files);
    MeshDesc m;
    CHECK(raw_mesh(donor, n, &files, &m, &err));
    if (err.message[0]) fprintf(stderr, "car_import: raw: %s\n", err.message);
    else {
        CHECK(same_mesh(&m, &d.mesh));
        mesh_desc_free(&m);
    }
    /* a quad more, on the first 4 vertices with the first 4 uvs, in surface: its row copies polygon 0's */
    char row[200];
    const uint32_t p = d.mesh.polygon_count;
    const MeshDescPolygon *p0 = &d.mesh.polygons[0];
    snprintf(row, sizeof row, "%u\t%u\t%u\t%u\t%u\t0\t0\n", p, (p0->flags & ~7u) | 4u, p0->unknown_1, p0->texture, p0->unknown_3);
    append(&files, "mesh.obj", "f 1/1 2/2 3/3 4/4\n");
    append(&files, "mesh.polygons.tsv", row);
    add_to_surface(&files, p);
    CHECK(raw_mesh(donor, n, &files, &m, &err));
    if (err.message[0]) fprintf(stderr, "car_import: raw with a quad: %s\n", err.message);
    else {
        CHECK_EQ_INT((int)m.polygon_count, (int)p + 1);
        const int s = group_index(&m, "surface");
        CHECK(s >= 0 && m.groups[s].count > 0 && m.groups[s].polygons[m.groups[s].count - 1] == p);
        mesh_desc_free(&m);
    }
    /* fix/raw-polygons-bounds: a row of 5 corners with a face of 5 is refused (the polygon holds 4), not written past its corners */
    FileSet five;
    fileset_init(&five);
    for (size_t i = 0; i < files.count; i++) fileset_add(&five, files.file[i].name, files.file[i].data, files.file[i].size);
    snprintf(row, sizeof row, "%u\t%u\t%u\t%u\t%u\t0\t0\n", p + 1, (p0->flags & ~7u) | 5u, p0->unknown_1, p0->texture, p0->unknown_3);
    append(&five, "mesh.obj", "f 1/1 2/2 3/3 4/4 5/5\n");
    append(&five, "mesh.polygons.tsv", row);
    CHECK(!raw_mesh(donor, n, &five, &m, &err) && strstr(err.message, "5 corners") != NULL);
    if (strstr(err.message, "5 corners") == NULL) fprintf(stderr, "car_import: five corners: %s\n", err.message);
    fileset_free(&five);
    /* a material that looks like ours but is not t<0..255>_: refused, named; one that is not ours keeps its row's texture */
    static const char *const BAD[] = { "t5x", "t300_body", "t99999999999_x" };
    for (size_t b = 0; b < sizeof BAD / sizeof BAD[0]; b++) {
        FileSet named;
        fileset_init(&named);
        for (size_t i = 0; i < files.count; i++) fileset_add(&named, files.file[i].name, files.file[i].data, files.file[i].size);
        char use[64];
        snprintf(use, sizeof use, "usemtl %s\nf 1/1 2/2 3/3 4/4\n", BAD[b]);
        append(&named, "mesh.obj", use);
        snprintf(row, sizeof row, "%u\t%u\t%u\t%u\t%u\t0\t0\n", p + 1, (p0->flags & ~7u) | 4u, p0->unknown_1, p0->texture, p0->unknown_3);
        append(&named, "mesh.polygons.tsv", row);
        CHECK(!raw_mesh(donor, n, &named, &m, &err) && strstr(err.message, BAD[b]) != NULL);
        if (strstr(err.message, BAD[b]) == NULL) fprintf(stderr, "car_import: material %s: %s\n", BAD[b], err.message);
        fileset_free(&named);
    }
    /* no lists.tsv: refused, saying so */
    FileSet bare;
    fileset_init(&bare);
    for (size_t i = 0; i < files.count; i++)
        if (strcmp(files.file[i].name, "mesh.lists.tsv") != 0) fileset_add(&bare, files.file[i].name, files.file[i].data, files.file[i].size);
    CHECK(!raw_mesh(donor, n, &bare, &m, &err) && strstr(err.message, "lists.tsv") != NULL);
    fileset_free(&bare);
    fileset_free(&files);
    mesh_desc_free(&d.mesh);
}

/* dep71: the Viper's high mesh through Blender (tests/blender_like.h; also with its faces by material) and back with raw comes back the Viper's; a vertex moved a metre
 * up comes back 130 units up; without the tables' mesh.obj it is refused, saying so */
static void test_blender(const uint8_t *donor, size_t n)
{
    Donor d;
    CHECK(donor_of(donor, n, &d));
    const uint8_t *child[CFM_CHILDREN];
    size_t len[CFM_CHILDREN];
    ExchangeError err = { { 0 } };
    FileSet files;
    fileset_init(&files);
    CHECK(cfm_describe(donor, n, child, len, &err));
    const MeshTextureBank banks[] = { { child[1], len[1], "../c1" } };
    CHECK(mesh_export(child[0], len[0], "mesh", banks, 1, &files, &err));
    const SetFile *f = fileset_find(&files, "mesh.blender.obj");
    CHECK(f != NULL);
    if (f != NULL) {
        char *text = malloc(f->size + 1);
        memcpy(text, f->data, f->size), text[f->size] = '\0';
        char *blender = blender_like(text);
        MeshDesc m;
        CHECK(raw_mesh_from(donor, n, blender, &files, &m, &err));
        if (err.message[0]) fprintf(stderr, "car_import: Blender: %s\n", err.message);
        else {
            CHECK(same_mesh(&m, &d.mesh));
            mesh_desc_free(&m);
        }
        /* the faces in material blocks, as Blender's exporter may write them: the same mesh */
        char *grouped = by_material(text), *sorted = blender_like(grouped);
        CHECK(strcmp(grouped, text) != 0);   /* the Viper's materials do interleave */
        CHECK(raw_mesh_from(donor, n, sorted, &files, &m, &err));
        if (err.message[0]) fprintf(stderr, "car_import: Blender by material: %s\n", err.message);
        else {
            CHECK(same_mesh(&m, &d.mesh));
            mesh_desc_free(&m);
        }
        free(grouped), free(sorted);
        /* vertex 1 a metre up */
        char *v = strstr(blender, "\nv "), *end = v != NULL ? strchr(v + 1, '\n') : NULL;
        double x, y, z;
        CHECK(end != NULL && sscanf(v + 1, "v %lf %lf %lf", &x, &y, &z) == 3);
        if (end != NULL) {
            char line[96];
            const size_t cap = strlen(blender) + sizeof line;
            char *moved = malloc(cap);
            snprintf(line, sizeof line, "\nv %.6f %.6f %.6f", x, y + 1, z);
            snprintf(moved, cap, "%.*s%s%s", (int)(v - blender), blender, line, end);
            CHECK(raw_mesh_from(donor, n, moved, &files, &m, &err));
            if (err.message[0] == '\0') {
                CHECK(m.vertices[0].y == d.mesh.vertices[0].y + 130 && m.vertices[0].x == d.mesh.vertices[0].x);
                mesh_desc_free(&m);
            }
            free(moved);
        }
        FileSet bare;
        fileset_init(&bare);
        for (size_t i = 0; i < files.count; i++)
            if (strcmp(files.file[i].name, "mesh.obj") != 0) fileset_add(&bare, files.file[i].name, files.file[i].data, files.file[i].size);
        CHECK(!raw_mesh_from(donor, n, blender, &bare, &m, &err) && strstr(err.message, "mesh.obj") != NULL);
        fileset_free(&bare);
        free(blender), free(text);
    }
    fileset_free(&files);
    mesh_desc_free(&d.mesh);
}

/* raw import of `donor` with its own meshes as exported (the low one too when `low`): the CFM made, malloc'd; NULL when it failed */
static uint8_t *raw_again(const uint8_t *donor, size_t n, int low, size_t *out_len)
{
    const uint8_t *child[CFM_CHILDREN];
    size_t len[CFM_CHILDREN];
    ExchangeError err = { { 0 } };
    FileSet hi, lo;
    fileset_init(&hi), fileset_init(&lo);
    ObjModel high, far;
    memset(&high, 0, sizeof high), memset(&far, 0, sizeof far);
    uint8_t *cfm = NULL;
    *out_len = 0;
    int ok = cfm_describe(donor, n, child, len, &err) && mesh_export(child[0], len[0], "mesh", NULL, 0, &hi, &err) &&
             mesh_export(child[2], len[2], "mesh", NULL, 0, &lo, &err);
    const SetFile *ho = ok ? fileset_find(&hi, "mesh.obj") : NULL, *lw = ok ? fileset_find(&lo, "mesh.obj") : NULL;
    ok = ok && ho != NULL && lw != NULL && obj_read(ho->data, ho->size, NULL, 0, &high, &err) && obj_read(lw->data, lw->size, NULL, 0, &far, &err);
    if (ok) {
        CarImportIn in = input(donor, n, &high, NULL);
        in.raw = 1, in.high_tables = &hi;
        if (low) in.low = &far, in.low_tables = &lo;
        int32_t half[3];
        ok = car_import(&in, &cfm, out_len, half, &err);
    }
    if (!ok) fprintf(stderr, "car_import: raw again: %s\n", err.message);
    obj_free(&high), obj_free(&far);
    fileset_free(&hi), fileset_free(&lo);
    if (!ok) {
        free(cfm);
        return NULL;
    }
    return cfm;
}

static int same_child(const uint8_t *a, size_t an, const uint8_t *b, size_t bn, int k)
{
    const uint8_t *ca[CFM_CHILDREN], *cb[CFM_CHILDREN];
    size_t la[CFM_CHILDREN], lb[CFM_CHILDREN];
    ExchangeError err = { { 0 } };
    return cfm_describe(a, an, ca, la, &err) && cfm_describe(b, bn, cb, lb, &err) && la[k] == lb[k] && memcmp(ca[k], cb[k], la[k]) == 0;
}

/* fix/raw-import-banks: raw keeps the donor's banks byte for byte (their order, block sizes, leads and tails), so an unchanged export of a
 * car the importer made comes back byte-identical; without a low OBJ the donor's far model and its bank stay as they were */
static void test_raw_keeps_the_banks(const uint8_t *viper, size_t viper_len)
{
    size_t n1 = 0, n2 = 0, n3 = 0;
    uint8_t *made = raw_again(viper, viper_len, 1, &n1);
    CHECK(made != NULL);
    if (made == NULL) return;
    CHECK(same_child(made, n1, viper, viper_len, 1));   /* the Viper's banks, as the CD has them */
    CHECK(same_child(made, n1, viper, viper_len, 3));
    uint8_t *again = raw_again(made, n1, 1, &n2);
    CHECK(again != NULL && n2 == n1 && memcmp(again, made, n1) == 0);
    /* without the low OBJ: the far model and its bank are the donor's, not the near ones */
    uint8_t *near_only = raw_again(viper, viper_len, 0, &n3);
    CHECK(near_only != NULL);
    if (near_only != NULL) {
        CHECK(same_child(near_only, n3, viper, viper_len, 2));
        CHECK(same_child(near_only, n3, viper, viper_len, 3));
        CHECK(same_child(near_only, n3, made, n1, 0) && same_child(near_only, n3, made, n1, 1));
    }
    free(made), free(again), free(near_only);
}

/* the Viper's high mesh through Blender (as test_blender) and back with raw onto a piece whose CFM is `own` (malloc'd CFM; NULL when it
 * failed) */
static uint8_t *blender_onto(const uint8_t *viper, size_t n, const uint8_t *own, size_t own_len, size_t *out_len)
{
    const uint8_t *child[CFM_CHILDREN];
    size_t len[CFM_CHILDREN];
    ExchangeError err = { { 0 } };
    FileSet files;
    fileset_init(&files);
    uint8_t *cfm = NULL;
    *out_len = 0;
    int ok = cfm_describe(viper, n, child, len, &err);
    const MeshTextureBank b = { ok ? child[1] : NULL, ok ? len[1] : 0, "../c1" };
    ok = ok && mesh_export(child[0], len[0], "mesh", &b, 1, &files, &err);
    const SetFile *f = ok ? fileset_find(&files, "mesh.blender.obj") : NULL;
    if (f != NULL) {
        char *text = malloc(f->size + 1);
        memcpy(text, f->data, f->size), text[f->size] = '\0';
        char *blender = blender_like(text);
        ObjModel model;
        ok = obj_read((const uint8_t *)blender, strlen(blender), NULL, 0, &model, &err);
        if (ok) {
            CarImportIn in = input(viper, n, &model, NULL);
            in.raw = 1, in.high_tables = &files, in.own_cfm = own, in.own_cfm_len = own_len;
            int32_t half[3];
            ok = car_import(&in, &cfm, out_len, half, &err);
            obj_free(&model);
        }
        free(blender), free(text);
    }
    if (!ok || f == NULL) fprintf(stderr, "car_import: Blender onto a piece: %s\n", err.message), free(cfm), cfm = NULL;
    fileset_free(&files);
    return cfm;
}

/* dep71 follow-up: a piece whose rsid car resize made twice its size, then the Viper's old export through Blender and back with raw:
 * the piece's banks stay (rsid at its new size), its uvs on rsid come back scaled (Blender's 0..1 read with the new size), the whole
 * CFM is the resized one; a piece whose banks name other entries (another car's) gets the donor's, as before */
static void test_raw_keeps_a_resize(const uint8_t *viper, size_t viper_len)
{
    const uint8_t *child[CFM_CHILDREN];
    size_t len[CFM_CHILDREN];
    ExchangeError err = { { 0 } };
    ShpiDescEntry e[64];
    size_t ne = 0, rn = 0, n1 = 0, n2 = 0;
    CHECK(cfm_describe(viper, viper_len, child, len, &err) && shpi_describe(child[1], len[1], e, 64, &ne, &err));
    uint16_t w = 0, h = 0;
    for (size_t i = 0; i < ne; i++)
        if (memcmp(e[i].name, "rsid", 4) == 0) w = e[i].width, h = e[i].height;
    uint8_t *resized = NULL;
    CarResizeStats st;
    CHECK(w > 0 && car_resize(viper, viper_len, 0, "rsid", (uint16_t)(2 * w), (uint16_t)(2 * h), NULL, &resized, &rn, &st, &err));
    if (resized == NULL) return;
    uint8_t *back = blender_onto(viper, viper_len, resized, rn, &n1);
    CHECK(back != NULL && n1 == rn && memcmp(back, resized, rn) == 0);
    if (back != NULL && (n1 != rn || memcmp(back, resized, rn) != 0))
        for (int k = 0; k < CFM_CHILDREN; k++) fprintf(stderr, "car_import: resize kept: child %d %s\n", k, same_child(back, n1, resized, rn, k) ? "same" : "differs");
    /* another car's piece: the donor's banks */
    size_t nn = 0;
    uint8_t *nsx = read_cd("SIMDATA/CARFAMS/ANSX.CFM", &nn);
    if (nsx != NULL) {
        uint8_t *other = blender_onto(viper, viper_len, nsx, nn, &n2);
        CHECK(other != NULL && same_child(other, n2, viper, viper_len, 1) && same_child(other, n2, viper, viper_len, 3) &&
              same_child(other, n2, viper, viper_len, 2));
        free(other);
    }
    free(nsx), free(back), free(resized);
}

/* `obj` with its first two faces under the material `mtl` (and the faces after them under the material they had): what an author
 * does picking a material for two faces in Blender (malloc'd) */
static char *two_faces_on(const char *obj, const char *mtl)
{
    const size_t cap = strlen(obj) + 2 * strlen(mtl) + 512;
    char *out = malloc(cap), *copy = strdup(obj), last[256] = "";
    size_t at = 0;
    int faces = 0;
    for (char *l = strtok(copy, "\n"); l != NULL; l = strtok(NULL, "\n")) {
        if (strncmp(l, "usemtl ", 7) == 0) {
            snprintf(last, sizeof last, "%s", l);
            if (faces == 1) continue;   /* between the two: they stay on `mtl` */
        }
        if (strncmp(l, "f ", 2) == 0 && faces == 0) at += (size_t)snprintf(out + at, cap - at, "usemtl %s\n", mtl);
        at += (size_t)snprintf(out + at, cap - at, "%s\n", l);
        if (strncmp(l, "f ", 2) == 0 && ++faces == 2 && last[0] != '\0') at += (size_t)snprintf(out + at, cap - at, "%s\n", last);
    }
    free(copy);
    return out;
}

/* the piece `own` (made from the Viper) exported at both levels as a rebuild of the kit re-extracts it, `mtl` put on the first two faces of the
 * high OBJ (or the low one when `low_edit`), both through Blender and back with raw onto the piece (malloc'd CFM; NULL with the reason
 * in `why`) */
static uint8_t *blender_edit(const uint8_t *viper, size_t n, const uint8_t *own, size_t own_len, const char *mtl, int low_edit,
                             size_t *out_len, char *why, size_t why_size)
{
    const uint8_t *child[CFM_CHILDREN];
    size_t len[CFM_CHILDREN];
    ExchangeError err = { { 0 } };
    FileSet hi, lo;
    fileset_init(&hi), fileset_init(&lo);
    ObjModel high, far;
    memset(&high, 0, sizeof high), memset(&far, 0, sizeof far);
    uint8_t *cfm = NULL;
    char *text[2] = { NULL, NULL };
    *out_len = 0, why[0] = '\0';
    int ok = cfm_describe(own, own_len, child, len, &err);
    const MeshTextureBank bh = { ok ? child[1] : NULL, ok ? len[1] : 0, "../c1" }, bl = { ok ? child[3] : NULL, ok ? len[3] : 0, "../c3" };
    ok = ok && mesh_export(child[0], len[0], "mesh", &bh, 1, &hi, &err) && mesh_export(child[2], len[2], "mesh", &bl, 1, &lo, &err);
    for (int k = 0; ok && k < 2; k++) {
        const SetFile *f = fileset_find(k == 0 ? &hi : &lo, "mesh.blender.obj");
        if (f == NULL) { ok = 0; break; }
        char *plain = malloc(f->size + 1);
        memcpy(plain, f->data, f->size), plain[f->size] = '\0';
        char *edited = k == low_edit ? two_faces_on(plain, mtl) : strdup(plain);
        text[k] = blender_like(edited);
        free(plain), free(edited);
    }
    ok = ok && obj_read((const uint8_t *)text[0], strlen(text[0]), NULL, 0, &high, &err) &&
         obj_read((const uint8_t *)text[1], strlen(text[1]), NULL, 0, &far, &err);
    if (ok) {
        CarImportIn in = input(viper, n, &high, NULL);
        in.raw = 1, in.high_tables = &hi, in.low = &far, in.low_tables = &lo, in.own_cfm = own, in.own_cfm_len = own_len;
        int32_t half[3];
        ok = car_import(&in, &cfm, out_len, half, &err);
    }
    if (!ok) snprintf(why, why_size, "%s", err.message), free(cfm), cfm = NULL;
    obj_free(&high), obj_free(&far);
    free(text[0]), free(text[1]);
    fileset_free(&hi), fileset_free(&lo);
    return cfm;
}

/* dep71 follow-up 2: the Viper with a bitmap of its own (badg, 64 x 32, near), two of its faces put on it in Blender: they come back on
 * it, their uvs in its texels; every other face on its texture as before; the piece's bank (the donor's entries and badg after them)
 * kept byte for byte. A far face put on it is refused, saying to add it to the far model (--low); with it added there, it is taken. */
static void test_raw_takes_an_added_bitmap(const uint8_t *viper, size_t viper_len)
{
    static uint8_t px[64 * 32];
    for (int i = 0; i < 64 * 32; i++) px[i] = (uint8_t)(i % 0xfa);
    uint8_t *own = NULL, *both = NULL;
    size_t on = 0, bn = 0, n1 = 0, n2 = 0;
    uint32_t ref = 0, far_ref = 0;
    ExchangeError err = { { 0 } };
    CHECK(car_texture_add(viper, viper_len, 0, "badg", "badge", px, 64, 32, &own, &on, &ref, &err));
    if (own == NULL) return;
    char mtl[32], why[256];
    snprintf(mtl, sizeof mtl, "t%u_badge", ref);
    uint8_t *back = blender_edit(viper, viper_len, own, on, mtl, 0, &n1, why, sizeof why);
    CHECK(back != NULL);
    if (back == NULL) printf("car_import: Blender onto badg: %s\n", why);
    Donor was, now;
    if (back != NULL && donor_of(own, on, &was) && donor_of(back, n1, &now)) {
        CHECK(same_child(back, n1, own, on, 1) && same_child(back, n1, own, on, 3) && same_child(back, n1, own, on, 2));
        CHECK(now.mesh.polygon_count == was.mesh.polygon_count && now.mesh.texture_count == was.mesh.texture_count);
        int on_badg = 0, moved_wrong = 0, uv_in = 1;
        for (uint32_t p = 0; p < now.mesh.polygon_count && p < was.mesh.polygon_count; p++) {
            const MeshDescPolygon *q = &now.mesh.polygons[p];
            if (q->texture == ref) {
                on_badg++;
                for (int c = 0; c < (q->flags & 7) && q->has_uv; c++) {
                    const MeshUv u = now.mesh.uvs[q->uv[c]];
                    uv_in &= u.u >= 0 && u.u <= 64 && u.v >= 0 && u.v <= 32;
                }
            } else {
                moved_wrong += q->texture != was.mesh.polygons[p].texture;
            }
        }
        CHECK_EQ_INT(on_badg, 2);
        CHECK_EQ_INT(moved_wrong, 0);
        CHECK(uv_in);   /* Blender's 0..1 in badg's texels, not rsid's */
        mesh_desc_free(&was.mesh), mesh_desc_free(&now.mesh);
    }
    /* a far face on it: refused while the far model has no such texture, naming --low; taken once it has */
    uint8_t *no = blender_edit(viper, viper_len, own, on, mtl, 1, &n2, why, sizeof why);
    CHECK(no == NULL && strstr(why, "--low") != NULL);
    if (no != NULL || strstr(why, "--low") == NULL) printf("car_import: a far face on badg: '%s'\n", why);
    free(no);
    CHECK(car_texture_add(own, on, 1, "badg", "badge", px, 32, 16, &both, &bn, &far_ref, &err));
    snprintf(mtl, sizeof mtl, "t%u_badge", far_ref);
    uint8_t *far_ok = both != NULL ? blender_edit(viper, viper_len, both, bn, mtl, 1, &n2, why, sizeof why) : NULL;
    CHECK(far_ok != NULL && same_child(far_ok, n2, both, bn, 3));
    if (far_ok == NULL) printf("car_import: a far face on badg added --low: %s\n", why);
    free(far_ok), free(both), free(back), free(own);
}

int main(void)
{
    size_t n = 0;
    uint8_t *donor = read_cd("SIMDATA/CARFAMS/DVIPER.CFM", &n);
    if (donor == NULL) {
        fprintf(stderr, "car_import: no CD in original/, skipped\n");
        return TEST_REPORT();
    }
    FileSet pngs;
    fileset_init(&pngs);
    add_png(&pngs, "paint.png", 16, 8, 200, 30, 30, 255);
    add_png(&pngs, "seat.png", 8, 8, 60, 60, 60, 0);   /* one clear texel */
    test_obj(donor, n, &pngs);
    refused_obj(donor, n, &pngs, 1, 0, "wheel_fl");
    refused_obj(donor, n, &pngs, 0, 1, "wheel_fr");
    test_twins(donor, n, &pngs);
    test_raw(donor, n);
    test_blender(donor, n);
    test_raw_keeps_the_banks(donor, n);
    test_raw_keeps_a_resize(donor, n);
    test_raw_takes_an_added_bitmap(donor, n);
    fileset_free(&pngs);
    free(donor);
    return TEST_REPORT();
}
