/* The Model window's car (editor/model_scene.c): it draws exactly the texels and palette the Preview window shows (M4: the race's painted textures, the race's 6 bits
 * a channel), and in the Preview's night view (M3): by day the file's colours, at night the palette in the track's light and the lamp texels the masks name lit in
 * their fixed colours, only with the switches that light them. A synthetic car (a box with a textured side, mesh_build/shpi_build/cfm_build: no game data) and a
 * mask with head and brake lights; with the CD's P911 in original/ (skipped without) the same from its own masks, and TNFS_MODEL_SHOTS=DIR writes the pictures. */
#include "model_scene.h"
#include "formats/car_mask_codec.h"
#include "formats/cfm_build.h"
#include "formats/mesh_build.h"
#include "formats/png.h"
#include "formats/shpi_build.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { W = 320, H = 200, SIDE = 8 };

static char base[256];

/* the box of test_car_scene.c: 6 two-sided quads, the +x one textured with "side" (8 x 8) */
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
    t[1].unknown_1[0] = 0x80 | 0x40 << 8;
    t[2].unknown_1[0] = 0x20 << 16 | 0xC0 << 8;
    memcpy(g[0].name, "surface", 7), g[0].polygons = surface, g[0].count = 6;
    MeshDesc d = { 0 };
    d.vertices = v, d.vertex_count = 8, d.uvs = uv, d.uv_count = 4, d.polygons = p, d.polygon_count = 6, d.textures = t, d.texture_count = 3, d.groups = g, d.group_count = 1;
    uint8_t *orip = NULL;
    size_t orip_len = 0;
    ExchangeError err = { { 0 } };
    if (!mesh_build(&d, &orip, &orip_len, &err)) return NULL;
    static uint8_t pal[768], side[SIDE * SIDE];
    for (int i = 0; i < 256; i++) pal[3 * i] = (uint8_t)(i | 3), pal[3 * i + 1] = (uint8_t)(255 - i), pal[3 * i + 2] = (uint8_t)(i / 2 + 1);   /* low bits the race drops */
    for (int i = 0; i < SIDE * SIDE; i++) side[i] = (uint8_t)(40 + i);
    const ShpiDescEntry e[2] = { { .name = { '!', 'P', 'A', 'L' }, .type = 0x24, .width = 256, .height = 1, .data = pal, .block_size = SHPI_BLOCK_AUTO },
                                 { .name = { 's', 'i', 'd', 'e' }, .type = 0x7b, .width = SIDE, .height = SIDE, .data = side, .block_size = SHPI_BLOCK_AUTO, .dir_index = 1 } };
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

static int write_file(const char *path, const uint8_t *data, size_t n)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) return 0;
    const int ok = fwrite(data, 1, n, f) == n;
    return fclose(f) == 0 && ok;
}

/* side_mask.png: the top two rows headlights, the bottom two brake lights, the rest paint */
static int make_mask(const char *dir)
{
    uint8_t mask[SIDE * SIDE], *png = NULL;
    size_t n = 0;
    for (int i = 0; i < SIDE * SIDE; i++) mask[i] = i < 2 * SIDE ? CAR_MASK_HEADLIGHT : i >= (SIDE - 2) * SIDE ? CAR_MASK_BRAKE : CAR_MASK_PAINT;
    if (!car_mask_write(mask, SIDE, SIDE, &png, &n)) return 0;
    char path[400];
    snprintf(path, sizeof path, "%s/side_mask.png", dir);
    const int ok = write_file(path, png, n);
    png_free(png);
    return ok;
}

static uint8_t rgba[W * H * 4];

typedef struct {
    int drawn, lamps;
    long light;   /* the sum of the channels of the pixels drawn */
    uint32_t hash;
} Picture;

static Picture picture(ModelScene *m, const Orbit *o, const CarNightState *night, const char *shot)
{
    Picture p = { 0, 0, 0, 2166136261u };
    CHECK(model_scene_render(m, o, W, H, night, rgba));
    for (size_t i = 0; i < sizeof rgba; i++) p.hash = (p.hash ^ rgba[i]) * 16777619u;
    for (int i = 0; i < W * H; i++)
        if (rgba[4 * i + 3] != 0) p.drawn++, p.light += rgba[4 * i] + rgba[4 * i + 1] + rgba[4 * i + 2];
    p.lamps = model_scene_lamp_pixels(m);
    const char *dir = getenv("TNFS_MODEL_SHOTS");
    if (dir != NULL && shot != NULL) {
        uint8_t *png = NULL;
        size_t n = 0;
        char path[400];
        snprintf(path, sizeof path, "%s/%s.png", dir, shot);
        if (png_write_rgba(rgba, W, H, &png, &n) == PNG_OK) write_file(path, png, n);
        png_free(png);
    }
    return p;
}

/* M4: the scene's texels are the Preview's, texture for texture, and its day colours the Preview's palette (6 bits a channel, as the race takes it) */
static void check_same_as_preview(const ModelScene *m, const Orbit *o, ModelScene *mm)
{
    const CarPreview *pv = model_scene_preview(m);
    CHECK(pv != NULL);
    if (pv == NULL) return;
    int compared = 0;
    for (int i = 0; i < pv->count; i++) {
        const PreviewTex *t = &pv->tex[i];
        if (t->info.level != 0 || t->index == NULL) continue;
        int w = 0, h = 0;
        const uint8_t *texels = model_scene_texels(m, t->info.name, &w, &h);
        CHECK(texels != NULL && w == t->info.width && h == t->info.height);
        if (texels != NULL && w == t->info.width && h == t->info.height) {
            CHECK(memcmp(texels, t->index, (size_t)w * (size_t)h) == 0);
            compared++;
        }
    }
    CHECK(compared > 0);
    CHECK(model_scene_render(mm, o, W, H, NULL, rgba));
    int off_palette = 0;
    for (int i = 0; i < W * H; i++) {
        if (rgba[4 * i + 3] == 0) continue;
        int found = 0;
        for (int e = 0; e < 256 && !found; e++) found = memcmp(rgba + 4 * i, pv->palette[0] + 3 * e, 3) == 0;
        off_palette += !found;
    }
    CHECK_EQ_INT(off_palette, 0);   /* every pixel a colour of the Preview's palette: no 8-bit colour of the file the race does not show */
}

static void test_a_synthetic_car(void)
{
    char dir[400], masks[400], path[500];
    snprintf(dir, sizeof dir, "%s/car", base);
    snprintf(masks, sizeof masks, "%s/masks", base);
    mkdir(dir, 0777), mkdir(masks, 0777);
    size_t len = 0;
    uint8_t *cfm = make_cfm(&len);
    CHECK(cfm != NULL);
    if (cfm == NULL) return;
    snprintf(path, sizeof path, "%s/BOX.CFM", dir);
    CHECK(write_file(path, cfm, len));
    free(cfm);
    CHECK(make_mask(masks));
    PreviewIn in;
    memset(&in, 0, sizeof in);
    in.cfm = path, in.stem = "BOX", in.masks_dir = masks, in.added = 1;
    char why[200] = "";
    ModelScene *m = model_scene_open_painted(&in, why, sizeof why);
    CHECK(m != NULL);
    if (m == NULL) {
        fprintf(stderr, "model_scene_open_painted: %s\n", why);
        return;
    }
    Orbit o;
    orbit_init(&o, model_scene_radius(m));
    orbit_set(&o, 60 * 16, 30 * 16);   /* the textured side and two flat faces */
    check_same_as_preview(m, &o, m);
    CarNightState s;
    memset(&s, 0, sizeof s);
    const Picture day = picture(m, &o, &s, "box_day");
    s.reverse = 1;
    const Picture day_lamps = picture(m, &o, &s, NULL);   /* the box has no reversing light: by day nothing glows */
    s.reverse = 0, s.lights = 1;
    const Picture day_switches = picture(m, &o, &s, NULL);   /* head and position lights glow only at night */
    s.brake = 1;
    const Picture day_brake = picture(m, &o, &s, "box_day_brake");   /* the brake lights light by day too, as in the race (car_night.c) */
    s.brake = 0;
    const Picture day_brake_off = picture(m, &o, &s, NULL);  /* and go out again: every switch change remakes the lamps */
    s.brake = 1;
    s.night = 1, s.lit = 1, s.light[0] = s.light[1] = s.light[2] = 0.4f;
    const Picture night = picture(m, &o, &s, "box_night");
    s.brake = 0;
    const Picture night_lights = picture(m, &o, &s, NULL);
    s.lights = 0;
    const Picture night_dark = picture(m, &o, &s, "box_night_dark");
    fprintf(stderr, "box: day %d px light %ld, night %d px light %ld lamps %d, lights only %d, dark %d\n", day.drawn, day.light, night.drawn, night.light, night.lamps,
            night_lights.lamps, night_dark.lamps);
    CHECK(day.drawn > 1000);
    CHECK_EQ_INT(day.lamps, 0);
    CHECK_EQ_INT(day_lamps.lamps, 0);
    CHECK_EQ_INT(day_switches.lamps, 0);
    CHECK(day_brake.lamps > 0);                        /* braking by day: the brake texels only */
    CHECK_EQ_INT(day_brake_off.lamps, 0);
    CHECK_EQ_INT(day_brake_off.hash, day_switches.hash);
    CHECK_EQ_INT(night.drawn, day.drawn);              /* the same car, the same pixels */
    CHECK(night.lamps > 50);                           /* head and brake lights */
    CHECK(day_brake.lamps < night.lamps);
    CHECK_EQ_INT(night_lights.lamps, night.lamps);    /* with the lights on an unlit brake light shows the position light's colour, as in the race */
    CHECK(night_lights.hash != night.hash);           /* braking it shows its own */
    CHECK_EQ_INT(night_dark.lamps, 0);
    CHECK(night_dark.light < day.light / 2);           /* the car in the night's light */
    CHECK(night.light > night_dark.light);
    model_scene_close(m);
}

/* the CD's 911 with the game's own masks of it (skipped without original/): the front at night shows its headlights, the rear its brake lights */
static void test_the_cds_911(void)
{
    char path[512], masks[512];
    snprintf(path, sizeof path, "%s/original/SIMDATA/CARFAMS/P911.CFM", TNFS_SOURCE_DIR);
    snprintf(masks, sizeof masks, "%s/data/car_masks/P911", TNFS_SOURCE_DIR);
    if (access(path, R_OK) != 0) return;
    PreviewIn in;
    memset(&in, 0, sizeof in);
    in.cfm = path, in.stem = "P911", in.masks_dir = masks;
    char why[200] = "";
    ModelScene *m = model_scene_open_painted(&in, why, sizeof why);
    CHECK(m != NULL);
    if (m == NULL) {
        fprintf(stderr, "P911: %s\n", why);
        return;
    }
    Orbit o;
    orbit_init(&o, model_scene_radius(m));
    check_same_as_preview(m, &o, m);
    CarNightState s;
    memset(&s, 0, sizeof s);
    s.night = 1, s.lights = 1, s.brake = 1, s.lit = 1, s.light[0] = s.light[1] = s.light[2] = 0.4f;
    orbit_view(&o, ORBIT_FRONT);
    const Picture front = picture(m, &o, &s, "p911_night_front");
    orbit_view(&o, ORBIT_REAR);
    const Picture rear = picture(m, &o, &s, "p911_night_rear");
    orbit_home(&o);
    picture(m, &o, &s, "p911_night_home");
    memset(&s, 0, sizeof s);
    const Picture day = picture(m, &o, &s, "p911_day_home");
    fprintf(stderr, "P911 night: front %d lamp px, rear %d; day %d\n", front.lamps, rear.lamps, day.lamps);
    CHECK(front.lamps > 20);
    CHECK_EQ_INT(day.lamps, 0);
    model_scene_close(m);
}

int main(void)
{
    snprintf(base, sizeof base, "%s/model_scene_XXXXXX", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp");
    if (mkdtemp(base) == NULL) return 2;
    test_a_synthetic_car();
    test_the_cds_911();
    return TEST_REPORT();
}
