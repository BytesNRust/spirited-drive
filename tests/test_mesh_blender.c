/* dep71: a mesh's Blender OBJ back to mesh_export's (formats/mesh_blender.h), with no CD: an OBJ as mesh_export writes it, the one it
 * writes for Blender, that one through tests/blender_like.h (Blender's exporter as far as the tests know it), and back: the same
 * vertices, uvs, corners and materials; a moved vertex and a moved uv come back moved, a uv two corners shared and Blender split
 * takes a new row, a face without uvs stays without; a face or a vertex more or less is refused, naming the import without --raw. */
#include "formats/mesh_blender.h"
#include "formats/obj_read.h"
#include "test.h"
#include "blender_like.h"

#include <math.h>
#include <string.h>

/* mesh_export's: 5 vertices (the last a point no face uses), 3 uvs, a quad of texture 0 (4 x 2) whose first and last corners share
 * uv 1, a triangle of texture 1 (no bitmap) without uvs, a triangle of texture 0 without uvs (the materials go 0, 1, 0) */
static const char ORIGINAL[] = "# ORIP mesh 'T': integer units as stored, no axis flip\nmtllib m.mtl\no T\n"
                               "v 0 0 0\nv 130 0 0\nv 130 260 0\nv 0 0 -130\nv 5 5 5\n"
                               "vt 0 0\nvt 4 2\nvt 4 0\n"
                               "usemtl t0_side\nf 1/1 2/2 3/3 4/1\nusemtl t1_none\nf 1 2 3\nusemtl t0_side\nf 2 3 4\n";
/* what mesh_export writes for Blender from it */
static const char BLENDER[] = "# Spirited Drive mesh 'T' for Blender\nmtllib m.mtl\no T\n"
                              "v 0.000000 0.000000 0.000000\nv 1.000000 0.000000 0.000000\nv 1.000000 2.000000 0.000000\n"
                              "v 0.000000 0.000000 1.000000\nv 0.038462 0.038462 -0.038462\n"
                              "vt 0.000000 1.000000\nvt 1.000000 0.000000\nvt 1.000000 1.000000\nvt 0.000000 1.000000\n"
                              "usemtl t0_side\nf 1/1 2/2 3/3 4/4\nusemtl t1_none\nf 1 2 3\nusemtl t0_side\nf 2 3 4\n";
static const MeshBlenderSize SIZES[] = { { 4, 2 }, { 1, 1 } };

static int read(const char *text, ObjModel *m)
{
    ExchangeError err = { { 0 } };
    const int ok = obj_read((const uint8_t *)text, strlen(text), NULL, 0, m, &err);
    if (!ok) fprintf(stderr, "obj_read: %s\n", err.message);
    return ok;
}

/* `blender` (already Blender's) back against ORIGINAL into `out`; 0 with the reason in `err` */
static int back(const char *blender, ObjModel *out, ExchangeError *err)
{
    ObjModel b, o;
    if (!read(blender, &b)) return 0;
    if (!read(ORIGINAL, &o)) return obj_free(&b), 0;
    const int ok = mesh_blender_back(&b, &o, SIZES, 2, out, err);
    obj_free(&b), obj_free(&o);
    return ok;
}

static int same_model(const ObjModel *a, const ObjModel *b)
{
    int same = a->vertex_count == b->vertex_count && a->uv_count == b->uv_count && a->face_count == b->face_count &&
               a->corner_count == b->corner_count && a->from_exporter == b->from_exporter;
    for (uint32_t i = 0; same && i < a->vertex_count; i++)
        same = lround(a->vertices[i].x) == lround(b->vertices[i].x) && lround(a->vertices[i].y) == lround(b->vertices[i].y) &&
               lround(a->vertices[i].z) == lround(b->vertices[i].z);
    for (uint32_t i = 0; same && i < a->uv_count; i++) same = lround(a->uvs[i].u) == lround(b->uvs[i].u) && lround(a->uvs[i].v) == lround(b->uvs[i].v);
    for (uint32_t i = 0; same && i < a->corner_count; i++) same = a->corner_vertex[i] == b->corner_vertex[i] && a->corner_uv[i] == b->corner_uv[i];
    for (uint32_t f = 0; same && f < a->face_count; f++)
        same = a->faces[f].corners == b->faces[f].corners && a->faces[f].first == b->faces[f].first &&
               strcmp(a->materials[a->faces[f].material].name, b->materials[b->faces[f].material].name) == 0;
    return same;
}

static char *edited(const char *text, const char *from, const char *to)
{
    const char *hit = strstr(text, from);
    CHECK(hit != NULL);
    if (hit == NULL) return strdup(text);
    const size_t cap = strlen(text) + strlen(to) + 1;
    char *out = malloc(cap);
    snprintf(out, cap, "%.*s%s%s", (int)(hit - text), text, to, hit + strlen(from));
    return out;
}

static void test_unchanged(void)
{
    char *b = blender_like(BLENDER);
    CHECK(strstr(b, "o T.001\n") != NULL && strstr(b, "vn ") != NULL);   /* the rewrite did rewrite */
    ObjModel out, o;
    ExchangeError err = { { 0 } };
    CHECK(back(b, &out, &err) && read(ORIGINAL, &o));
    if (err.message[0]) fprintf(stderr, "back: %s\n", err.message);
    CHECK(same_model(&out, &o));
    obj_free(&out), obj_free(&o);
    free(b);
}

/* the faces in material blocks (0, 0, 1: Blender's exporter may write them so), and the quad's corners rotated by one: the same mesh */
static void test_reordered(void)
{
    char *rotated = edited(BLENDER, "f 1/1 2/2 3/3 4/4\n", "f 2/2 3/3 4/4 1/1\n");
    char *grouped = by_material(rotated);
    CHECK(strstr(grouped, "usemtl t0_side\nf 2/2 3/3 4/4 1/1\nf 2 3 4\nusemtl t1_none\nf 1 2 3\n") != NULL);
    char *b = blender_like(grouped);
    ObjModel out, o;
    ExchangeError err = { { 0 } };
    CHECK(back(b, &out, &err) && read(ORIGINAL, &o));
    if (err.message[0]) fprintf(stderr, "back: %s\n", err.message);
    else {
        CHECK(same_model(&out, &o));
        obj_free(&out), obj_free(&o);
    }
    free(rotated), free(grouped), free(b);
}

static void test_moved(void)
{
    /* vertex 3 a metre up, the quad's third corner's uv to the bitmap's middle */
    char *e1 = edited(BLENDER, "v 1.000000 2.000000 0.000000", "v 1.000000 3.000000 0.000000");
    char *e2 = edited(e1, "vt 1.000000 1.000000\n", "vt 0.500000 0.500000\n");
    char *b = blender_like(e2);
    ObjModel out;
    ExchangeError err = { { 0 } };
    CHECK(back(b, &out, &err));
    if (err.message[0]) fprintf(stderr, "back: %s\n", err.message);
    else {
        CHECK(lround(out.vertices[2].y) == 390 && lround(out.vertices[2].x) == 130);
        CHECK_EQ_INT((int)out.uv_count, 3);   /* uv 2 had one corner: it moves */
        CHECK(lround(out.uvs[2].u) == 2 && lround(out.uvs[2].v) == 1);
        CHECK_EQ_INT(out.corner_uv[2], 2);
        obj_free(&out);
    }
    free(e1), free(e2), free(b);
}

static void test_split(void)
{
    /* the quad's last corner, which shared uv 0 with its first, moved: a uv row more; the first keeps uv 0 */
    char *e = edited(BLENDER, "vt 0.000000 1.000000\nusemtl", "vt 0.250000 1.000000\nusemtl");
    char *b = blender_like(e);
    ObjModel out;
    ExchangeError err = { { 0 } };
    CHECK(back(b, &out, &err));
    if (err.message[0]) fprintf(stderr, "back: %s\n", err.message);
    else {
        CHECK_EQ_INT((int)out.uv_count, 4);
        CHECK_EQ_INT(out.corner_uv[0], 0);
        CHECK_EQ_INT(out.corner_uv[3], 3);
        CHECK(lround(out.uvs[3].u) == 1 && lround(out.uvs[3].v) == 0);
        CHECK(lround(out.uvs[0].u) == 0 && lround(out.uvs[0].v) == 0);
        obj_free(&out);
    }
    free(e), free(b);
}

static void test_no_uv_stays(void)
{
    /* Blender gives every face of a mesh with uvs a vt: the triangle that had none still has none */
    char *e1 = edited(BLENDER, "usemtl t1_none\nf 1 2 3\n", "usemtl t1_none\nf 1/1 2/1 3/1\n");
    ObjModel out;
    ExchangeError err = { { 0 } };
    CHECK(back(e1, &out, &err));
    if (err.message[0]) fprintf(stderr, "back: %s\n", err.message);
    else {
        CHECK(out.corner_uv[4] < 0 && out.corner_uv[5] < 0 && out.corner_uv[6] < 0);
        obj_free(&out);
    }
    free(e1);
}

static void refused(const char *from, const char *to, const char *word)
{
    char *e = edited(BLENDER, from, to);
    ObjModel out;
    ExchangeError err = { { 0 } };
    CHECK(!back(e, &out, &err));
    CHECK(strstr(err.message, word) != NULL && strstr(err.message, "--raw") != NULL);
    if (strstr(err.message, word) == NULL) fprintf(stderr, "refused: %s\n", err.message);
    free(e);
}

/* refusals that say why: the faces in another order (Blender's Separate or Sort), a vertex past the mesh's units, a material naming
 * a texture the mesh does not have, a textured face whose bitmap is not in the bank (size 0 x 0) */
static void refused_more(void)
{
    ObjModel out, b, o;
    ExchangeError err = { { 0 } };
    char *flipped = edited(BLENDER, "f 1/1 2/2 3/3 4/4\n", "f 4/4 3/3 2/2 1/1\n");   /* its corners the other way round */
    CHECK(!back(flipped, &out, &err) && strstr(err.message, "other way") != NULL);
    char *stray = edited(BLENDER, "f 1 2 3\n", "f 1 2 5\n");                       /* vertices that are no face of the mesh */
    CHECK(!back(stray, &out, &err) && strstr(err.message, "no face") != NULL);
    char *twice = edited(BLENDER, "f 1 2 3\n", "f 2 3 4\n");                       /* two faces on one face's vertices */
    CHECK(!back(twice, &out, &err) && strstr(err.message, "second") != NULL);
    char *far = edited(BLENDER, "v 1.000000 2.000000 0.000000", "v 1.000000 20000000.000000 0.000000");
    CHECK(!back(far, &out, &err) && strstr(err.message, "vertex 3") != NULL);
    char *unknown = edited(BLENDER, "usemtl t0_side", "usemtl t7_side");
    CHECK(!back(unknown, &out, &err) && strstr(err.message, "t7") != NULL);
    static const MeshBlenderSize MISSING[] = { { 0, 0 }, { 1, 1 } };
    CHECK(read(BLENDER, &b) && read(ORIGINAL, &o));
    CHECK(!mesh_blender_back(&b, &o, MISSING, 2, &out, &err) && strstr(err.message, "bank") != NULL);
    obj_free(&b), obj_free(&o);
    free(flipped), free(stray), free(twice), free(far), free(unknown);
}

int main(void)
{
    test_unchanged();
    test_reordered();
    test_moved();
    test_split();
    test_no_uv_stays();
    refused("usemtl t1_none\nf 1 2 3\n", "", "faces");                                           /* a face less */
    refused("v 0.038462 0.038462 -0.038462\n", "", "vertices");                                  /* a vertex less */
    refused("f 1/1 2/2 3/3 4/4\n", "f 1/1 2/2 3/3\n", "second");   /* a quad made a triangle: the triangle 1 2 3's vertices twice */
    refused_more();
    return TEST_REPORT();
}
