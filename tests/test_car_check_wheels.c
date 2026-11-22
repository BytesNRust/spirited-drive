/* car_check_wheels: a car built by hand with good wheels has no finding, each way to break them is named, and the 39 cars of the CD (every
 * one the game draws) have no error and no warning. */
#include "formats/car_check.h"
#include "formats/cfm_build.h"
#include "formats/chunk.h"
#include "formats/mesh.h"
#include "test.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

enum { BODY = 8, WHEEL = 4, VERTS = BODY + 2 * WHEEL };

typedef struct {
    MeshDesc d;
    MeshVertex v[VERTS];
    uint32_t left[WHEEL], right[WHEEL];
    MeshDescPart part[2];
    MeshTexture tex[2];
    MeshDescPolygon poly[4];
} Car;

/* a body box 1000 wide, 400 high, 2000 long, and a wheel of 4 vertices at each side of the front axle */
static void car_init(Car *c)
{
    memset(c, 0, sizeof *c);
    for (int i = 0; i < BODY; i++)
        c->v[i] = (MeshVertex){ i & 1 ? 500 : -500, i & 2 ? 400 : 0, i & 4 ? 1000 : -1000 };
    for (int s = 0; s < 2; s++)
        for (int k = 0; k < WHEEL; k++) {
            const int x = s ? 500 : -500;
            c->v[BODY + s * WHEEL + k] = (MeshVertex){ x, k & 1 ? 150 : 50, k & 2 ? 750 : 650 };
            (s ? c->right : c->left)[k] = (uint32_t)(BODY + s * WHEEL + k);
        }
    c->d.vertices = c->v;
    c->d.vertex_count = c->d.real_vertices = VERTS;
    memcpy(c->part[0].name, "left_tur", MESH_PART_NAME_SIZE);   /* the names are 8 bytes, with no NUL */
    memcpy(c->part[1].name, "right_tu", MESH_PART_NAME_SIZE);
    c->part[0].vertices = c->left, c->part[0].count = WHEEL;
    c->part[1].vertices = c->right, c->part[1].count = WHEEL;
    c->d.parts = c->part, c->d.part_count = 2;
    memcpy(c->tex[0].label, "frntires", 8);   /* the two tyre labels, each on a quad at each side of the car */
    memcpy(c->tex[1].label, "rertires", 8);
    c->d.textures = c->tex, c->d.texture_count = 2;
    for (int i = 0; i < 4; i++) {
        c->poly[i].flags = 4, c->poly[i].texture = (uint8_t)(i / 2);
        for (int k = 0; k < 4; k++) c->poly[i].vertex[k] = (i & 1 ? c->right : c->left)[k];
    }
    c->d.polygons = c->poly, c->d.polygon_count = 4;
}

static size_t findings(const Car *c, CarCheckList *l, const char *want)
{
    car_check_free(l);
    CHECK(car_check_wheels(&c->d, l));
    if (want != NULL) {
        int found = 0;
        for (size_t i = 0; i < l->count; i++) found += strstr(l->item[i].text, want) != NULL;
        CHECK(found > 0);
    }
    return l->count;
}

static void test_a_good_car_and_each_break(void)
{
    Car c;
    CarCheckList l = { 0 };
    car_init(&c);
    CHECK(findings(&c, &l, NULL) == 0);

    car_init(&c);   /* a traffic car: no steered part, nothing to say */
    c.d.part_count = 0;
    CHECK(findings(&c, &l, NULL) == 0);

    car_init(&c);   /* only one front wheel turns */
    c.d.part_count = 1;
    CHECK(findings(&c, &l, "right_tu") == 1 && l.item[0].severity == CAR_CHECK_ERROR);

    car_init(&c);   /* a part that moves nothing */
    c.part[1].count = 0;
    CHECK(findings(&c, &l, "moves no vertex") >= 1 && car_check_count(&l, CAR_CHECK_ERROR) >= 1);

    car_init(&c);   /* a part naming a vertex the model has not */
    c.right[2] = VERTS + 5;
    CHECK(findings(&c, &l, "outside the model") >= 1 && car_check_count(&l, CAR_CHECK_ERROR) >= 1);

    car_init(&c);   /* the wheels swapped: the left one on the car's right */
    for (int k = 0; k < WHEEL; k++) c.v[c.left[k]].x = 500, c.v[c.right[k]].x = -500;
    CHECK(findings(&c, &l, "swapped") >= 1 && car_check_count(&l, CAR_CHECK_ERROR) >= 1);

    car_init(&c);   /* both wheels above the middle of the car */
    for (int k = 0; k < WHEEL; k++) c.v[c.left[k]].y += 300, c.v[c.right[k]].y += 300;
    CHECK(findings(&c, &l, "middle") == 1 && l.item[0].severity == CAR_CHECK_WARNING);

    car_init(&c);   /* one wheel a hand ahead of the other: the axle is not square */
    for (int k = 0; k < WHEEL; k++) c.v[c.right[k]].z += 200;
    CHECK(findings(&c, &l, "axle") == 1 && l.item[0].severity == CAR_CHECK_WARNING);
    CHECK(strstr(l.item[0].text, "right_tu is 200 ahead of left_tur along the car") != NULL && strstr(l.item[0].text, "move right_tu 200 back") != NULL);

    car_init(&c);   /* the pair not mirrored about the middle */
    for (int k = 0; k < WHEEL; k++) c.v[c.right[k]].x = 200;
    CHECK(findings(&c, &l, "mirror") == 1);

    car_init(&c);   /* a tyre label on one quad only */
    c.d.polygon_count = 3;
    CHECK(findings(&c, &l, "rertires") == 1 && l.item[0].severity == CAR_CHECK_WARNING);

    car_init(&c);   /* both of a label's quads at the same side */
    for (int k = 0; k < 4; k++) c.poly[1].vertex[k] = c.left[k];
    CHECK(findings(&c, &l, "one at each side") == 1);

    car_init(&c);   /* a quad that is not flat across x: a wheel seen from the side is a plane */
    c.v[c.left[0]].x = -380;
    CHECK(findings(&c, &l, "flat") == 2);   /* the front and rear quads share these vertices */

    car_init(&c);   /* a triangle where the frame goes */
    c.poly[0].flags = 3;
    CHECK(findings(&c, &l, "quad") >= 1);
    car_check_free(&l);
}

/* the CD's cars: the steered parts and the tyre quads of all of them pass */
static void test_the_cds_cars_have_no_finding(void)
{
    char dir[512];
    snprintf(dir, sizeof dir, "%s/original/SIMDATA/CARFAMS", TNFS_SOURCE_DIR);
    DIR *h = opendir(dir);
    if (h == NULL) {
        fprintf(stderr, "car_check_wheels: no CD in original/, the CD's cars skipped\n");
        return;
    }
    int cars = 0;
    struct dirent *de;
    while ((de = readdir(h)) != NULL) {
        const size_t n = strlen(de->d_name);
        if (n < 5 || strcasecmp(de->d_name + n - 4, ".CFM") != 0) continue;
        char path[1024];
        snprintf(path, sizeof path, "%s/%s", dir, de->d_name);
        FILE *f = fopen(path, "rb");
        if (f == NULL) { CHECK(0); continue; }
        fseek(f, 0, SEEK_END);
        const long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        uint8_t *cfm = size > 0 ? malloc((size_t)size) : NULL;
        if (cfm == NULL || fread(cfm, 1, (size_t)size, f) != (size_t)size) { fclose(f); free(cfm); CHECK(0); continue; }
        fclose(f);
        const uint8_t *child[CFM_CHILDREN];
        size_t len[CFM_CHILDREN];
        ExchangeError err = { { 0 } };
        Mesh m;
        MeshDesc d;
        if (!cfm_describe(cfm, (size_t)size, child, len, &err)) { CHECK(0); free(cfm); continue; }
        Chunk ck = { CHUNK_KIND_MESH, child[0], child[0][4] | child[0][5] << 8 | child[0][6] << 16 | (size_t)child[0][7] << 24, 0 };
        if (mesh_open(&m, &ck) != MESH_OK || !mesh_describe(&m, &d, &err)) { CHECK(0); free(cfm); continue; }
        CarCheckList l = { 0 };
        CHECK(car_check_wheels(&d, &l));
        if (l.count > 0) fprintf(stderr, "%s: %s\n", de->d_name, l.item[0].text);
        CHECK(l.count == 0);
        car_check_free(&l);
        mesh_desc_free(&d);
        free(cfm);
        cars++;
    }
    closedir(h);
    CHECK(cars == 39);
}

int main(void)
{
    test_a_good_car_and_each_break();
    test_the_cds_cars_have_no_finding();
    return TEST_REPORT();
}
