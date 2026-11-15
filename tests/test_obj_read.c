/* dep59 tools B2, Task 2: an OBJ and its MTL read for the car importer (formats/obj_read.h). A cube of 6 faces in two materials;
 * the forms of a corner (v, v/vt, v//vn, v/vt/vn, negative); an object with a vertex and no face (a point); what it refuses, with
 * the line; the Viper's OBJ as mesh_export writes it (skipped without the CD). */
#include "formats/chunk.h"
#include "formats/mesh_exchange.h"
#include "formats/obj_read.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_text(const char *obj, const char *mtl, ObjModel *m, ExchangeError *err)
{
    return obj_read((const uint8_t *)obj, strlen(obj), (const uint8_t *)mtl, mtl != NULL ? strlen(mtl) : 0, m, err);
}

static const char CUBE[] =
    "# a cube\nmtllib cube.mtl\no Body\n"
    "v -1 -1 -1\nv 1 -1 -1\nv 1 1 -1\nv -1 1 -1\nv -1 -1 1\nv 1 -1 1\nv 1 1 1\nv -1 1 1\n"
    "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nvn 0 0 1\ns off\n"
    "usemtl red\nf 1/1 2/2 3/3 4/4\nf 5/1/1 8/4/1 7/3/1 6/2/1\nf 1//1 5//1 6//1 2//1\n"
    "usemtl paint\nf 4 3 7 8\nf -8 -4 -1 -5\nf 2/2 6/2 7/3 3/3\n";
static const char CUBE_MTL[] = "newmtl red\nKd 1 0 0\nmap_Kd textures/red side.png\n\nnewmtl paint\nKd 1 1 1\n";

static void test_cube(void)
{
    ObjModel m;
    ExchangeError err = { { 0 } };
    CHECK(read_text(CUBE, CUBE_MTL, &m, &err));
    if (err.message[0]) fprintf(stderr, "obj_read: cube: %s\n", err.message);
    CHECK_EQ_INT((int)m.vertex_count, 8);
    CHECK_EQ_INT((int)m.uv_count, 4);
    CHECK_EQ_INT((int)m.face_count, 6);
    CHECK_EQ_INT((int)m.material_count, 2);
    CHECK_EQ_INT(m.from_exporter, 0);
    if (m.face_count == 6 && m.material_count == 2) {
        CHECK(strcmp(m.materials[0].name, "red") == 0 && strcmp(m.materials[0].texture, "textures/red side.png") == 0);
        CHECK(strcmp(m.materials[1].name, "paint") == 0 && m.materials[1].texture[0] == '\0');
        for (int f = 0; f < 6; f++) {
            CHECK_EQ_INT(m.faces[f].material, f < 3 ? 0 : 1);
            CHECK_EQ_INT((int)m.faces[f].corners, 4);
        }
        const ObjFace *f1 = &m.faces[1], *f2 = &m.faces[2], *f4 = &m.faces[4];
        CHECK_EQ_INT((int)m.corner_vertex[f1->first + 1], 7);      /* 8/4/1: zero-based */
        CHECK_EQ_INT(m.corner_uv[f1->first + 1], 3);
        CHECK_EQ_INT(m.corner_uv[f2->first], -1);                  /* 1//1 */
        CHECK_EQ_INT((int)m.corner_vertex[f4->first], 0);          /* -8 of 8 */
        CHECK_EQ_INT((int)m.corner_vertex[f4->first + 2], 7);      /* -1 */
        CHECK_EQ_INT((int)m.faces[5].line, 25);
        CHECK(m.vertices[6].x == 1 && m.vertices[6].y == 1 && m.vertices[6].z == 1);
        CHECK(m.uvs[2].u == 1 && m.uvs[2].v == 1);
    }
    CHECK_EQ_INT((int)m.object_count, 2);                          /* "" and Body */
    if (m.object_count == 2) {
        CHECK(strcmp(m.objects[1].name, "Body") == 0);
        CHECK_EQ_INT((int)m.objects[1].first_vertex, 0);
        CHECK_EQ_INT((int)m.objects[1].vertex_count, 8);
        CHECK_EQ_INT((int)m.objects[1].face_count, 6);
        CHECK_EQ_INT((int)m.faces[0].object, 1);
    }
    obj_free(&m);
}

/* objects: a point with no face; g as well as o; the vertices of each are those written after its name */
static void test_objects(void)
{
    static const char obj[] = "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\no smok\nv 0 5 -10\ng wheel_fl\nv 1 1 1\nv 2 1 1\nv 2 2 1\nf -1 -2 -3\n";
    ObjModel m;
    ExchangeError err = { { 0 } };
    CHECK(read_text(obj, NULL, &m, &err));
    CHECK_EQ_INT((int)m.object_count, 3);
    if (m.object_count == 3) {
        CHECK(strcmp(m.objects[0].name, "") == 0 && m.objects[0].face_count == 1 && m.objects[0].vertex_count == 3);
        CHECK(strcmp(m.objects[1].name, "smok") == 0 && m.objects[1].face_count == 0);
        CHECK(m.objects[1].first_vertex == 3 && m.objects[1].vertex_count == 1);
        CHECK(strcmp(m.objects[2].name, "wheel_fl") == 0 && m.objects[2].face_count == 1 && m.objects[2].first_vertex == 4);
    }
    CHECK(m.face_count == 2 && m.faces[1].object == 2 && m.faces[1].material == -1);
    obj_free(&m);
}

/* `obj` refused, the message holding `word` */
static void refused(const char *obj, const char *mtl, const char *word, const char *what)
{
    ObjModel m;
    ExchangeError err = { { 0 } };
    const int ok = read_text(obj, mtl, &m, &err);
    if (ok || strstr(err.message, word) == NULL) fprintf(stderr, "obj_read: %s: %s (%s)\n", what, ok ? "read" : err.message, word);
    CHECK_EQ_INT(ok, 0);
    CHECK(strstr(err.message, word) != NULL);
    if (ok) obj_free(&m);
}

static void test_refusals(void)
{
    refused("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 4\n", NULL, "line 4", "a vertex index past the vertices");
    refused("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 0\n", NULL, "line 4", "a vertex index of 0");
    refused("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 -4\n", NULL, "line 4", "a negative index before the first vertex");
    refused("v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nf 1/1 2/2 3/1\n", NULL, "line 5", "a uv index past the uvs");
    refused("v 0 0 0\nv 1 0 0\n\nf 1 2\n", NULL, "line 4", "a face of 2 corners");
    refused("v 0 0 0\nv 1 0 0\nl 1 2\n", NULL, "line 3", "a line element");
    refused("v 0 0 0\np 1\n", NULL, "line 2", "a point element");
    refused("v 0 zero 0\n", NULL, "line 1", "a coordinate that is not a number");
    refused("v 0 0\n", NULL, "line 1", "a vertex of 2 coordinates");
    refused("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 x 3\n", NULL, "line 4", "a corner that is not a number");
    refused("v 0 0 0\n", "newmtl\n", "line 1", "an MTL material with no name");
}

/* the Viper's high mesh as mesh_export writes it */
static void test_exporter(void)
{
    char path[512];
    snprintf(path, sizeof path, "%s/original/SIMDATA/CARFAMS/DVIPER.CFM", TNFS_SOURCE_DIR);
    FILE *f = fopen(path, "rb");
    if (f == NULL) { fprintf(stderr, "obj_read: no CD in original/, the exporter's OBJ is skipped\n"); return; }
    static uint8_t cfm[200000];
    const size_t n = fread(cfm, 1, sizeof cfm, f);
    fclose(f);
    Chunk root, c;
    FileSet files;
    fileset_init(&files);
    ExchangeError err = { { 0 } };
    CHECK(chunk_open(&root, cfm, n) == CHUNK_OK && chunk_child(&root, CFM_CHILD_MESH_HIGH, &c) == CHUNK_OK);
    CHECK(mesh_export(c.data, c.size, "m", NULL, 0, &files, &err));
    const SetFile *obj = fileset_find(&files, "m.obj"), *mtl = fileset_find(&files, "m.mtl");
    ObjModel m;
    CHECK(obj != NULL && mtl != NULL && obj_read(obj->data, obj->size, mtl->data, mtl->size, &m, &err));
    if (obj != NULL && mtl != NULL && err.message[0] == '\0') {
        CHECK_EQ_INT(m.from_exporter, 1);
        CHECK_EQ_INT((int)m.vertex_count, 125);
        CHECK_EQ_INT((int)m.material_count, 22);
        obj_free(&m);
    } else {
        fprintf(stderr, "obj_read: the Viper's OBJ: %s\n", err.message);
    }
    fileset_free(&files);
}

int main(void)
{
    test_cube();
    test_objects();
    test_refusals();
    test_exporter();
    return TEST_REPORT();
}
