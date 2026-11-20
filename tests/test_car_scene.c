/* The Studio's 3D view of a car (core/raster/orbit.h, car_scene.h): the orbit camera's moves and limits, a synthetic car (a box with a textured
 * side, built with mesh_build, shpi_build and cfm_build: no CD data) opened and drawn from the four fixed views and an orbit, each picture's CRC-32
 * pinned, and a CFM that is not a car refused. With the CD's P911 in original/ (skipped without) its four views are pinned too. */
#include "formats/cfm_build.h"
#include "formats/mesh_build.h"
#include "formats/shpi_build.h"
#include "raster/car_scene.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

enum { W = 160, H = 100, U = 0x10000 };

static uint32_t crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

static void test_the_orbit_keeps_to_its_limits(void)
{
    Orbit o;
    orbit_init(&o, 300);
    CHECK_EQ_INT(o.dist, 900);
    orbit_drag(&o, 0, -100000);
    CHECK_EQ_INT(o.pitch, ORBIT_PITCH_MIN);   /* a little below the road at most */
    orbit_drag(&o, 0, 100000);
    CHECK_EQ_INT(o.pitch, ORBIT_PITCH_MAX);   /* straight above at most */
    orbit_drag(&o, 10000, 0);
    CHECK(o.yaw >= 0 && o.yaw < ORBIT_TURN);  /* the yaw wraps */
    orbit_drag(&o, -100000, 0);
    CHECK(o.yaw >= 0 && o.yaw < ORBIT_TURN);
    for (int i = 0; i < 100; i++) orbit_zoom(&o, 1);
    CHECK_EQ_INT(o.dist, o.dist_min);
    for (int i = 0; i < 200; i++) orbit_zoom(&o, -1);
    CHECK_EQ_INT(o.dist, o.dist_max);
    orbit_view(&o, ORBIT_TOP);
    CHECK_EQ_INT(o.pitch, ORBIT_PITCH_MAX);
    orbit_view(&o, ORBIT_SIDE);
    CHECK_EQ_INT(o.pitch, 0);
    CHECK_EQ_INT(o.yaw, 90 * 16);
    orbit_home(&o);
    CHECK_EQ_INT(o.dist, o.dist_home);
    int32_t m[9];
    orbit_view(&o, ORBIT_REAR);
    orbit_matrix(&o, m);
    CHECK_EQ_INT(m[0], 0x10000);   /* yaw 0, pitch 0: the identity */
    CHECK_EQ_INT(m[4], 0x10000);
    CHECK_EQ_INT(m[8], 0x10000);
}

/* ---- a synthetic car: a box, 6 two-sided quads, one with a texture ---- */
static uint8_t *make_cfm(size_t *len)
{
    static MeshVertex v[8];
    static MeshUv uv[4] = { { 0, 0 }, { 7, 0 }, { 7, 7 }, { 0, 7 } };
    static MeshDescPolygon p[6];
    static MeshTexture t[3];
    static uint32_t surface[6] = { 0, 1, 2, 3, 4, 5 };
    static MeshDescGroup g[1];
    const int sx[8] = { -1, 1, 1, -1, -1, 1, 1, -1 }, sy[8] = { -1, -1, 1, 1, -1, -1, 1, 1 }, sz[8] = { -1, -1, -1, -1, 1, 1, 1, 1 };
    for (int i = 0; i < 8; i++) v[i] = (MeshVertex){ sx[i] * 2 * 300, sy[i] * 300, sz[i] * 400 };
    const uint32_t face[6][4] = { { 0, 1, 2, 3 }, { 5, 4, 7, 6 }, { 4, 0, 3, 7 }, { 1, 5, 6, 2 }, { 3, 2, 6, 7 }, { 4, 5, 1, 0 } };
    for (int i = 0; i < 6; i++) {
        p[i] = (MeshDescPolygon){ .flags = 4 | (i == 3 ? 0x80 : 0), .unknown_1 = (uint8_t)(0x01 | (i == 3 ? 0x10 : 0)), .texture = (uint8_t)(i == 3 ? 0 : 1 + i % 2), .has_uv = i == 3 };
        for (int k = 0; k < 4; k++) p[i].vertex[k] = face[i][k], p[i].uv[k] = (uint32_t)k;
    }
    memcpy(t[0].label, "side", 4), memcpy(t[0].name, "side", 4);
    t[1].unknown_1[0] = 0x80 | 0x40 << 8;   /* flat colours, nearest the bank palette's */
    t[2].unknown_1[0] = 0x20 << 16 | 0xC0 << 8;
    memcpy(g[0].name, "surface", 7), g[0].polygons = surface, g[0].count = 6;
    MeshDesc d = { 0 };
    d.vertices = v, d.vertex_count = 8, d.uvs = uv, d.uv_count = 4, d.polygons = p, d.polygon_count = 6, d.textures = t, d.texture_count = 3, d.groups = g, d.group_count = 1;
    uint8_t *orip = NULL;
    size_t orip_len = 0;
    ExchangeError err = { { 0 } };
    if (!mesh_build(&d, &orip, &orip_len, &err)) return NULL;
    static uint8_t pal[768], side[64];
    for (int i = 0; i < 256; i++) pal[3 * i] = (uint8_t)i, pal[3 * i + 1] = (uint8_t)(255 - i), pal[3 * i + 2] = (uint8_t)(i / 2);
    for (int i = 0; i < 64; i++) side[i] = (uint8_t)(40 + i);
    const ShpiDescEntry e[2] = { { .name = "!PAL", .type = 0x24, .width = 256, .height = 1, .data = pal, .block_size = SHPI_BLOCK_AUTO },
                                 { .name = "side", .type = 0x7b, .width = 8, .height = 8, .data = side, .block_size = SHPI_BLOCK_AUTO, .dir_index = 1 } };
    uint8_t *shpi = NULL;
    size_t shpi_len = 0;
    if (!shpi_build(e, 2, &shpi, &shpi_len, &err)) return free(orip), NULL;
    const uint8_t *const child[CFM_CHILDREN] = { orip, shpi, orip, shpi };
    const size_t clen[CFM_CHILDREN] = { orip_len, shpi_len, orip_len, shpi_len };
    uint8_t *cfm = NULL;
    const int ok = cfm_build(child, clen, &cfm, len, &err);
    free(orip), free(shpi);
    return ok ? cfm : NULL;
}

static uint32_t picture(CarScene *s, const Orbit *o, int *drawn)
{
    static uint8_t pix[W * H];
    memset(pix, CAR_SCENE_CLEAR, sizeof pix);
    car_scene_render(s, o, pix, W, H);
    *drawn = 0;
    for (int i = 0; i < W * H; i++) *drawn += pix[i] != CAR_SCENE_CLEAR;
    return crc32(pix, sizeof pix);
}

static const uint32_t GOLDEN[5] = { 0x1b8678eau, 0x9af83cd1u, 0x3e42a15bu, 0x1a57e690u, 0x2dc643f8u };   /* front, side, rear, top, orbit */

static void test_a_synthetic_car(void)
{
    size_t len = 0;
    uint8_t *cfm = make_cfm(&len);
    CHECK(cfm != NULL);
    if (cfm == NULL) return;
    CarScene s;
    CHECK(car_scene_open(&s, cfm, len));
    CHECK(s.radius > 600 && s.radius < 800);   /* the box's corner: sqrt(600^2 + 300^2 + 400^2) = 781 */
    Orbit o;
    orbit_init(&o, s.radius);
    uint32_t crc[5];
    int drawn[5];
    const OrbitView views[4] = { ORBIT_FRONT, ORBIT_SIDE, ORBIT_REAR, ORBIT_TOP };
    for (int i = 0; i < 4; i++) {
        orbit_view(&o, views[i]);
        crc[i] = picture(&s, &o, &drawn[i]);
    }
    orbit_home(&o);
    crc[4] = picture(&s, &o, &drawn[4]);
    for (int i = 0; i < 5; i++) {
        fprintf(stderr, "view %d crc %08x drawn %d\n", i, crc[i], drawn[i]);
        CHECK(drawn[i] > 500);                 /* the car is in the picture */
        CHECK_EQ_INT(crc[i], GOLDEN[i]);
        for (int j = 0; j < i; j++) CHECK(crc[i] != crc[j]);   /* every view is its own */
    }
    uint8_t rgba[4 * 4], pix[4] = { 0, 1, CAR_SCENE_CLEAR, 7 };
    car_scene_rgba(&s, pix, 4, rgba);
    CHECK_EQ_INT(rgba[3], 255);
    CHECK_EQ_INT(rgba[4 + 0], 1);                 /* the palette's entry 1: red 1 */
    CHECK_EQ_INT(rgba[8 + 3], 0);                 /* the clear index is transparent */
    car_scene_close(&s);
    free(cfm);
}

/* the CD's 911 (skipped without original/): its four views pinned, so the high model, its bank, the wheels at rest and the sections' order are */
static const uint32_t GOLDEN_P911[4] = { 0x3d07a350u, 0xfeb3f860u, 0xd73844dbu, 0xbcd263f6u };

static void test_the_cds_911(void)
{
    char path[512];
    snprintf(path, sizeof path, "%s/original/SIMDATA/CARFAMS/P911.CFM", TNFS_SOURCE_DIR);
    FILE *f = fopen(path, "rb");
    if (f == NULL) return;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *cfm = malloc((size_t)n);
    CHECK(cfm != NULL && fread(cfm, 1, (size_t)n, f) == (size_t)n);
    fclose(f);
    CarScene s;
    CHECK(car_scene_open(&s, cfm, (size_t)n));
    Orbit o;
    orbit_init(&o, s.radius);
    const OrbitView views[4] = { ORBIT_FRONT, ORBIT_SIDE, ORBIT_REAR, ORBIT_TOP };
    for (int i = 0; i < 4; i++) {
        int drawn;
        orbit_view(&o, views[i]);
        const uint32_t crc = picture(&s, &o, &drawn);
        fprintf(stderr, "911 view %d crc %08x drawn %d\n", i, crc, drawn);
        CHECK(drawn > 1000);
        CHECK_EQ_INT(crc, GOLDEN_P911[i]);
    }
    car_scene_close(&s);
    free(cfm);
}

static void test_what_is_not_a_car_is_refused(void)
{
    CarScene s;
    uint8_t junk[64] = { 'w', 'w', 'w', 'w' };
    CHECK(!car_scene_open(&s, junk, sizeof junk));
    CHECK(!car_scene_open(&s, junk, 0));
}

int main(void)
{
    test_the_orbit_keeps_to_its_limits();
    test_a_synthetic_car();
    test_the_cds_911();
    test_what_is_not_a_car_is_refused();
    return TEST_REPORT();
}
