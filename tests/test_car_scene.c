/* The Studio's 3D view of a car (core/raster/orbit.h, car_scene.h): the orbit camera's moves and limits, a synthetic car (a box with a textured
 * side, built with mesh_build, shpi_build and cfm_build: no CD data) opened and drawn from the four fixed views and an orbit, each picture's CRC-32
 * pinned, and a CFM that is not a car refused. With the CD's P911 in original/ (skipped without) its four views are pinned too. */
#include "formats/cfm_build.h"
#include "formats/fixed_math.h"
#include "raster/car_pose.h"
#include "formats/mesh_build.h"
#include "formats/shpi_build.h"
#include "raster/car_scene.h"
#include "test.h"

#include <math.h>
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
    CHECK_EQ_INT(o.pitch, 18 * 16);
    orbit_drag(&o, 0, 72 * 16 / ORBIT_DRAG);
    CHECK_EQ_INT(o.pitch, ORBIT_PITCH_TOP);   /* straight above */
    orbit_drag(&o, 0, 90 * 16 / ORBIT_DRAG);
    CHECK_EQ_INT(o.pitch, 180 * 16);           /* on over the top: no limit, the car upside down */
    orbit_drag(&o, 0, 210 * 16 / ORBIT_DRAG);
    CHECK_EQ_INT(o.pitch, 30 * 16);            /* a whole turn and more: the pitch wraps */
    orbit_drag(&o, 0, -(45 * 16) / ORBIT_DRAG);
    CHECK_EQ_INT(o.pitch, ORBIT_TURN - 15 * 16);   /* below the road, as far as the drag goes */
    for (int i = 0; i < 50; i++) orbit_drag(&o, 0, -100000);
    CHECK(o.pitch >= 0 && o.pitch < ORBIT_TURN);
    orbit_set(&o, 0, 0);
    orbit_drag(&o, 10, 0);
    const int32_t upright = o.yaw;
    orbit_set(&o, 0, 180 * 16);
    orbit_drag(&o, 10, 0);
    CHECK_EQ_INT(o.yaw, ORBIT_TURN - upright);   /* upside down a drag turns the other way, so the car still follows the pointer */
    orbit_set(&o, -90 * 16, 370 * 16);
    CHECK_EQ_INT(o.yaw, 270 * 16);
    CHECK_EQ_INT(o.pitch, 10 * 16);              /* a set orbit is wrapped too, not held */
    orbit_set_degrees(&o, 360e9 + 45.0, -100.5);
    CHECK_EQ_INT(o.yaw, 45 * 16);                /* degrees of any size */
    CHECK_EQ_INT(o.pitch, 260 * 16 - 8);
    orbit_set_degrees(&o, NAN, INFINITY);
    CHECK_EQ_INT(o.yaw, 0);                      /* not a number: level, from the rear */
    CHECK_EQ_INT(o.pitch, 0);
    orbit_drag(&o, 10000, 0);
    CHECK(o.yaw >= 0 && o.yaw < ORBIT_TURN);  /* the yaw wraps */
    orbit_drag(&o, -100000, 0);
    CHECK(o.yaw >= 0 && o.yaw < ORBIT_TURN);
    for (int i = 0; i < 100; i++) orbit_zoom(&o, 1);
    CHECK_EQ_INT(o.dist, o.dist_min);
    for (int i = 0; i < 200; i++) orbit_zoom(&o, -1);
    CHECK_EQ_INT(o.dist, o.dist_max);
    orbit_set_dist(&o, 1);
    CHECK_EQ_INT(o.dist, o.dist_min);   /* a distance asked for is held too: nearer, the near plane would cut the car */
    orbit_set_dist(&o, 1000);
    CHECK_EQ_INT(o.dist, 1000);
    orbit_set_dist(&o, INT32_MAX);
    CHECK_EQ_INT(o.dist, o.dist_max);
    orbit_view(&o, ORBIT_TOP);
    CHECK_EQ_INT(o.pitch, ORBIT_PITCH_TOP);
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

/* M1: the pitch tips the car about the eye's horizontal axis, whatever the yaw: from any side a positive pitch shows the roof (the car's up leans
 * toward the eye, as from the front, where the race's own tip already does), and the car's up stays upright on the screen (no sideways lean) */
static void test_the_pitch_is_the_eyes(void)
{
    Orbit o;
    orbit_init(&o, 300);
    const int32_t up[3] = { 0, 0x10000, 0 };
    int32_t m[9], front[3];
    car_body_matrix(0x800000, 30 * 0x1000000 / 360, 0, m);   /* the race's own tip, the car's front to the eye: the roof shows */
    mat3_apply(up, m, front);
    CHECK(front[2] != 0);
    for (int32_t yaw = 0; yaw < ORBIT_TURN; yaw += 30 * 16) {
        int32_t c[3];
        orbit_set(&o, yaw, 30 * 16);
        orbit_matrix(&o, m);
        mat3_apply(up, m, c);
        CHECK((c[2] < 0) == (front[2] < 0));   /* the same lean as from the front */
        CHECK(c[0] > -0x400 && c[0] < 0x400);  /* upright: no lean to the side */
        CHECK(c[1] > 0);
    }
}

/* ---- a synthetic car: a box, 6 two-sided quads, one with a texture ---- */
/* `wheels`: the race cars' tyre refs too (frntires, rertires naming no bitmap; tire1 -> tyr1, tire4 -> tyr4, each its own pixels) */
static uint8_t *make_cfm_with(size_t *len, int wheels)
{
    static MeshVertex v[8];
    static MeshUv uv[4] = { { 0, 0 }, { 7, 0 }, { 7, 7 }, { 0, 7 } };
    static MeshDescPolygon p[6];
    static MeshTexture t[9];
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
    memset(t + 3, 0, sizeof t[0] * 6);
    memcpy(t[3].label, "frntires", 8), memcpy(t[4].label, "rertires", 8);
    memcpy(t[5].label, "tire1", 5), memcpy(t[5].name, "tyr1", 4), memcpy(t[6].label, "tire4", 5), memcpy(t[6].name, "tyr4", 4);
    memcpy(t[7].label, "tire2", 5), memcpy(t[7].name, "tyr2", 4), memcpy(t[8].label, "tire3", 5), memcpy(t[8].name, "tyr3", 4);
    MeshDesc d = { 0 };
    d.vertices = v, d.vertex_count = 8, d.uvs = uv, d.uv_count = 4, d.polygons = p, d.polygon_count = 6, d.textures = t, d.texture_count = wheels ? 9 : 3, d.groups = g, d.group_count = 1;
    uint8_t *orip = NULL;
    size_t orip_len = 0;
    ExchangeError err = { { 0 } };
    if (!mesh_build(&d, &orip, &orip_len, &err)) return NULL;
    static uint8_t pal[768], side[64], tyr1[64], tyr2[64], tyr3[64], tyr4[64];
    for (int i = 0; i < 256; i++) pal[3 * i] = (uint8_t)i, pal[3 * i + 1] = (uint8_t)(255 - i), pal[3 * i + 2] = (uint8_t)(i / 2);
    for (int i = 0; i < 64; i++) side[i] = (uint8_t)(40 + i), tyr1[i] = 11, tyr2[i] = 22, tyr3[i] = 33, tyr4[i] = 44;
    const ShpiDescEntry e[6] = { { .name = { '!', 'P', 'A', 'L' }, .type = 0x24, .width = 256, .height = 1, .data = pal, .block_size = SHPI_BLOCK_AUTO },
                                 { .name = { 's', 'i', 'd', 'e' }, .type = 0x7b, .width = 8, .height = 8, .data = side, .block_size = SHPI_BLOCK_AUTO, .dir_index = 1 },
                                 { .name = { 't', 'y', 'r', '1' }, .type = 0x7b, .width = 8, .height = 8, .data = tyr1, .block_size = SHPI_BLOCK_AUTO, .dir_index = 2 },
                                 { .name = { 't', 'y', 'r', '4' }, .type = 0x7b, .width = 8, .height = 8, .data = tyr4, .block_size = SHPI_BLOCK_AUTO, .dir_index = 3 },
                                 { .name = { 't', 'y', 'r', '2' }, .type = 0x7b, .width = 8, .height = 8, .data = tyr2, .block_size = SHPI_BLOCK_AUTO, .dir_index = 4 },
                                 { .name = { 't', 'y', 'r', '3' }, .type = 0x7b, .width = 8, .height = 8, .data = tyr3, .block_size = SHPI_BLOCK_AUTO, .dir_index = 5 } };
    uint8_t *shpi = NULL;
    size_t shpi_len = 0;
    if (!shpi_build(e, wheels ? 6 : 2, &shpi, &shpi_len, &err)) return free(orip), NULL;
    const uint8_t *const child[CFM_CHILDREN] = { orip, shpi, orip, shpi };
    const size_t clen[CFM_CHILDREN] = { orip_len, shpi_len, orip_len, shpi_len };
    uint8_t *cfm = NULL;
    const int ok = cfm_build(child, clen, &cfm, len, &err);
    free(orip), free(shpi);
    return ok ? cfm : NULL;
}

static uint8_t *make_cfm(size_t *len)
{
    return make_cfm_with(len, 0);
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

static const uint32_t GOLDEN[5] = { 0xdd527df4u, 0x78d20c93u, 0x5ec55c77u, 0x61cf1fcbu, 0x3a35d6dfu };   /* front, side, rear, top, orbit (160 x 100: the scale of the 100-pixel side) */

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

/* M2: a picture of any shape has square pixels and the same scale as a square one of its smaller side: the square's picture is the middle of the wide one, and of the tall one */
static void test_the_picture_fills_any_shape(void)
{
    size_t len = 0;
    uint8_t *cfm = make_cfm(&len);
    CHECK(cfm != NULL);
    if (cfm == NULL) return;
    CarScene s;
    CHECK(car_scene_open(&s, cfm, len));
    Orbit o;
    orbit_init(&o, s.radius);
    enum { S = 96, WIDE = 3 * S };
    static uint8_t square[S * S], wide[WIDE * S], tall[S * WIDE];
    memset(square, CAR_SCENE_CLEAR, sizeof square), memset(wide, CAR_SCENE_CLEAR, sizeof wide), memset(tall, CAR_SCENE_CLEAR, sizeof tall);
    car_scene_render(&s, &o, square, S, S);
    car_scene_render(&s, &o, wide, WIDE, S);
    car_scene_render(&s, &o, tall, S, WIDE);
    int same_wide = 1, same_tall = 1, outside = 0, drawn = 0;
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            same_wide &= square[y * S + x] == wide[y * WIDE + S + x];
            same_tall &= square[y * S + x] == tall[(S + y) * S + x];
            drawn += square[y * S + x] != CAR_SCENE_CLEAR;
        }
    for (int y = 0; y < S; y++)
        for (int x = 0; x < WIDE; x++) outside += (x < S || x >= 2 * S) && wide[y * WIDE + x] != CAR_SCENE_CLEAR;
    CHECK(drawn > 500);
    CHECK(same_wide);
    CHECK(same_tall);
    CHECK_EQ_INT(outside, 0);   /* at home the whole car is in the square: nothing more at the sides */
    /* zoomed in all the way, from every side: no corner of the car comes to the near plane, so no polygon is left out */
    for (int i = 0; i < 100; i++) orbit_zoom(&o, 1);
    int32_t nearest = INT32_MAX;
    for (int32_t yaw = 0; yaw < ORBIT_TURN; yaw += 45 * 16)
        for (int32_t pitch = 0; pitch < ORBIT_TURN; pitch += 45 * 16) {
            o.yaw = yaw, o.pitch = pitch;
            car_scene_render(&s, &o, wide, WIDE, S);
            for (uint32_t v = 0; v < s.obj.vertex_count; v++)
                if (s.obj.camera[v][2] < nearest) nearest = s.obj.camera[v][2];
        }
    fprintf(stderr, "dist_min %d radius %d nearest corner %d\n", o.dist_min, s.radius, nearest);
    CHECK(nearest > CAR_SCENE_NEAR_Z);
    car_scene_close(&s);
    free(cfm);
}

/* M3: a layer of the same pixels: the side's texture swapped for one of a single value, every flat in another; it covers exactly what the picture does, and
 * the car's own texture is back after it */
static void test_a_layer_of_the_same_pixels(void)
{
    size_t len = 0;
    uint8_t *cfm = make_cfm(&len);
    CHECK(cfm != NULL);
    if (cfm == NULL) return;
    CarScene s;
    CHECK(car_scene_open(&s, cfm, len));
    Orbit o;
    orbit_init(&o, s.radius);
    orbit_set(&o, 60 * 16, 30 * 16);   /* the textured side and flat faces */
    static uint8_t pix[W * H], layer[W * H], again[W * H];
    memset(pix, CAR_SCENE_CLEAR, sizeof pix), memset(layer, CAR_SCENE_CLEAR, sizeof layer), memset(again, CAR_SCENE_CLEAR, sizeof again);
    car_scene_render(&s, &o, pix, W, H);
    static uint8_t fives[64];
    memset(fives, 5, sizeof fives);
    const FshEntry **swap = calloc(s.obj.texture_count, sizeof *swap);
    CHECK(swap != NULL);
    if (swap == NULL) return;
    FshEntry side;
    memset(&side, 0, sizeof side);
    int swapped = 0;
    for (uint32_t i = 0; i < s.obj.texture_count; i++)
        if (s.obj.textures[i].texture != NULL && memcmp(s.obj.textures[i].texture->name, "side", 4) == 0) {
            side = *s.obj.textures[i].texture, side.payload = fives, side.payload_size = sizeof fives;
            swap[i] = &side, swapped++;
        }
    CHECK_EQ_INT(swapped, 1);
    car_scene_render_layer(&s, &o, layer, W, H, swap, 9);
    int same_cover = 1, fives_seen = 0, nines = 0, other = 0;
    for (int i = 0; i < W * H; i++) {
        same_cover &= (pix[i] == CAR_SCENE_CLEAR) == (layer[i] == CAR_SCENE_CLEAR);
        fives_seen += layer[i] == 5, nines += layer[i] == 9;
        other += layer[i] != 5 && layer[i] != 9 && layer[i] != CAR_SCENE_CLEAR;
    }
    CHECK(same_cover);
    fprintf(stderr, "layer: %d of the side, %d flat\n", fives_seen, nines);
    CHECK(fives_seen > 100);   /* the textured side */
    CHECK(nines > 100);        /* the flat faces */
    CHECK_EQ_INT(other, 0);
    car_scene_render(&s, &o, again, W, H);
    CHECK(memcmp(pix, again, sizeof pix) == 0);   /* the car's own textures are back */
    free(swap);
    car_scene_close(&s);
    free(cfm);
}

/* the CD's 911 (skipped without original/): its four views pinned, so the high model, its bank, the wheels at rest and the sections' order are */
static const uint32_t GOLDEN_P911[4] = { 0xa980ab32u, 0xb06d745au, 0x96b1cebbu, 0x741a4d58u };   /* the side's wheels are tyr1 (at rest) since 2026-11-20, tyr4 before */

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
        CHECK(drawn > 800);
        CHECK_EQ_INT(crc, GOLDEN_P911[i]);
    }
    car_scene_close(&s);
    free(cfm);
}

/* the wheels at rest show the still frame, tyr1, as the race does for a car that stands (game/race/car_wheels.c: frame 0 below 0x1999): not tyr4, the blurred one */
static void test_the_wheels_at_rest_are_the_still_frame(void)
{
    size_t len = 0;
    uint8_t *cfm = make_cfm_with(&len, 1);
    CHECK(cfm != NULL);
    if (cfm == NULL) return;
    CarScene s;
    CHECK(car_scene_open(&s, cfm, len));
    const int32_t front = object3d_texture_named(&s.obj, "frntires"), rear = object3d_texture_named(&s.obj, "rertires");
    const int32_t still = object3d_texture_named(&s.obj, "tire1"), fast = object3d_texture_named(&s.obj, "tire4");
    CHECK(front >= 0 && rear >= 0 && still >= 0 && fast >= 0);
    if (front >= 0 && rear >= 0 && still >= 0 && fast >= 0) {
        CHECK(s.obj.textures[still].texture != NULL && s.obj.textures[still].texture != s.obj.textures[fast].texture);
        CHECK(s.obj.textures[front].texture == s.obj.textures[still].texture);
        CHECK(s.obj.textures[rear].texture == s.obj.textures[still].texture);
    }
    car_scene_close(&s);
    free(cfm);
}

/* Wheels: Still / Slow / Fast: car_scene_wheels(k) draws race frame k on both axles: 0 tire1, 1 tire2, 2 tire3, 3 tire4; each frame its own pixels */
static void test_each_wheel_frame_links_its_entry(void)
{
    size_t len = 0;
    uint8_t *cfm = make_cfm_with(&len, 1);
    CHECK(cfm != NULL);
    if (cfm == NULL) return;
    CarScene s;
    CHECK(car_scene_open(&s, cfm, len));
    static const char *const NAMES[4] = { "tire1", "tire2", "tire3", "tire4" };
    const int32_t front = object3d_texture_named(&s.obj, "frntires"), rear = object3d_texture_named(&s.obj, "rertires");
    for (int k = 0; k < 4; k++) {
        const int32_t frame = object3d_texture_named(&s.obj, NAMES[k]);
        CHECK(frame >= 0 && car_scene_wheels(&s, k));
        if (frame < 0 || front < 0 || rear < 0) continue;
        CHECK(s.obj.textures[front].texture == s.obj.textures[frame].texture && s.obj.textures[rear].texture == s.obj.textures[frame].texture);
        for (int j = 0; j < k; j++) CHECK(s.obj.textures[front].texture != s.obj.textures[object3d_texture_named(&s.obj, NAMES[j])].texture);   /* not another frame's */
    }
    const void *last = s.obj.textures[front].texture;
    CHECK(!car_scene_wheels(&s, 4) && !car_scene_wheels(&s, -1) && s.obj.textures[front].texture == last);   /* a frame the race does not have: nothing changes */
    car_scene_close(&s);
    free(cfm);
    uint8_t *plain = make_cfm_with(&len, 0);   /* a car with no tyre refs */
    CHECK(plain != NULL && car_scene_open(&s, plain, len) && !car_scene_wheels(&s, 1));
    car_scene_close(&s);
    free(plain);
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
    test_the_pitch_is_the_eyes();
    test_a_synthetic_car();
    test_the_picture_fills_any_shape();
    test_a_layer_of_the_same_pixels();
    test_the_cds_911();
    test_the_wheels_at_rest_are_the_still_frame();
    test_each_wheel_frame_links_its_entry();
    test_what_is_not_a_car_is_refused();
    return TEST_REPORT();
}
