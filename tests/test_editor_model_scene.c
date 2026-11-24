/* The Model window's car (editor/model_scene.c): it draws exactly the texels and palette the Preview window shows (M4: the race's painted textures, the race's 6 bits
 * a channel), and in the Preview's night view (M3): by day the file's colours, at night the palette in the track's light and the lamp texels the masks name lit in
 * their fixed colours, only with the switches that light them. A synthetic car (a box with a textured side, mesh_build/shpi_build/cfm_build: no game data) and a
 * mask with head and brake lights; with the CD's P911 in original/ (skipped without) the same from its own masks, and TNFS_MODEL_SHOTS=DIR writes the pictures. */
#include "model_scene.h"
#include "card_backdrop.h"
#include "card_photo.h"
#include "formats/car_brake_lamp.h"
#include "formats/car_mask_codec.h"
#include "formats/car_night.h"
#include "formats/cfm_build.h"
#include "formats/mesh_build.h"
#include "formats/png.h"
#include "formats/shpi_build.h"
#include "raster/shine_class.h"
#include "test.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

enum { W = 320, H = 200, SIDE = 8 };

static char base[256];

/* the box of test_car_scene.c: 6 two-sided quads, the +x one textured with "side" (8 x 8); with `wheel` the other five with "tyr1", a texture with no
 * mask whose texels are all palette index 1 (as a wheel's are low indices: W1, the owner's 2026-11-20 test) */
static uint8_t *make_cfm_of(size_t *len, int wheel)
{
    static MeshVertex v[8];
    static MeshUv uv[4] = { { 0, 0 }, { 7, 0 }, { 7, 7 }, { 0, 7 } };
    static MeshDescPolygon p[6];
    static MeshTexture t[4];
    static uint32_t surface[6] = { 0, 1, 2, 3, 4, 5 };
    static MeshDescGroup g[1];
    const int sx[8] = { -1, 1, 1, -1, -1, 1, 1, -1 }, sy[8] = { -1, -1, 1, 1, -1, -1, 1, 1 }, sz[8] = { -1, -1, -1, -1, 1, 1, 1, 1 };
    for (int i = 0; i < 8; i++) v[i] = (MeshVertex){ sx[i] * 2 * 300, sy[i] * 300, sz[i] * 400 };
    const uint32_t face[6][4] = { { 0, 1, 2, 3 }, { 5, 4, 7, 6 }, { 4, 0, 3, 7 }, { 1, 5, 6, 2 }, { 3, 2, 6, 7 }, { 4, 5, 1, 0 } };
    for (int i = 0; i < 6; i++) {
        const int textured = i == 3 || wheel;
        p[i] = (MeshDescPolygon){ .flags = 4 | (textured ? 0x80 : 0), .unknown_1 = (uint8_t)(0x01 | (textured ? 0x10 : 0)),
                                  .texture = (uint8_t)(i == 3 ? 0 : wheel ? 3 : 1 + i % 2), .has_uv = textured };
        for (int k = 0; k < 4; k++) p[i].vertex[k] = face[i][k], p[i].uv[k] = (uint32_t)k;
    }
    memcpy(t[0].label, "side", 4), memcpy(t[0].name, "side", 4);
    memcpy(t[3].label, "tyr1", 4), memcpy(t[3].name, "tyr1", 4);
    t[1].unknown_1[0] = 0x80 | 0x40 << 8;
    t[2].unknown_1[0] = 0x20 << 16 | 0xC0 << 8;
    memcpy(g[0].name, "surface", 7), g[0].polygons = surface, g[0].count = 6;
    MeshDesc d = { 0 };
    d.vertices = v, d.vertex_count = 8, d.uvs = uv, d.uv_count = 4, d.polygons = p, d.polygon_count = 6, d.textures = t, d.texture_count = 4, d.groups = g, d.group_count = 1;
    uint8_t *orip = NULL;
    size_t orip_len = 0;
    ExchangeError err = { { 0 } };
    if (!mesh_build(&d, &orip, &orip_len, &err)) return NULL;
    static uint8_t pal[768], side[SIDE * SIDE], tyre[SIDE * SIDE];
    for (int i = 0; i < 256; i++) pal[3 * i] = (uint8_t)(i | 3), pal[3 * i + 1] = (uint8_t)(255 - i), pal[3 * i + 2] = (uint8_t)(i / 2 + 1);   /* low bits the race drops */
    pal[3 * 0xfe] = 172, pal[3 * 0xfe + 1] = 252, pal[3 * 0xfe + 2] = 64;   /* the brake lamp's entry in the CD's files: a phosphor green the race never shows */
    for (int i = 0; i < SIDE * SIDE; i++) side[i] = (uint8_t)(40 + i), tyre[i] = 1;
    for (int i = 3 * SIDE; i < 4 * SIDE; i++) side[i] = 0xfe;   /* a row of brake lamp in the paint: the race paints it, not the mask */
    const ShpiDescEntry e[3] = { { .name = { '!', 'P', 'A', 'L' }, .type = 0x24, .width = 256, .height = 1, .data = pal, .block_size = SHPI_BLOCK_AUTO },
                                 { .name = { 's', 'i', 'd', 'e' }, .type = 0x7b, .width = SIDE, .height = SIDE, .data = side, .block_size = SHPI_BLOCK_AUTO, .dir_index = 1 },
                                 { .name = { 't', 'y', 'r', '1' }, .type = 0x7b, .width = SIDE, .height = SIDE, .data = tyre, .block_size = SHPI_BLOCK_AUTO, .dir_index = 2 } };
    uint8_t *shpi = NULL;
    size_t shpi_len = 0;
    if (!shpi_build(e, 3, &shpi, &shpi_len, &err)) return free(orip), NULL;
    const uint8_t *const child[CFM_CHILDREN] = { orip, shpi, orip, shpi };
    const size_t clen[CFM_CHILDREN] = { orip_len, shpi_len, orip_len, shpi_len };
    uint8_t *cfm = NULL;
    const int ok = cfm_build(child, clen, &cfm, len, &err);
    free(orip), free(shpi);
    return ok ? cfm : NULL;
}

static uint8_t *make_cfm(size_t *len) { return make_cfm_of(len, 0); }

static int write_file(const char *path, const uint8_t *data, size_t n)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) return 0;
    const int ok = fwrite(data, 1, n, f) == n;
    return fclose(f) == 0 && ok;
}

/* side_mask.png: the top two rows headlights, the bottom two brake lights, the rest paint (or, `matte`, all matte: nothing shines) */
static int make_mask_of(const char *dir, int matte)
{
    uint8_t mask[SIDE * SIDE], *png = NULL;
    size_t n = 0;
    for (int i = 0; i < SIDE * SIDE; i++)
        mask[i] = matte ? CAR_MASK_MATTE : i < 2 * SIDE ? CAR_MASK_HEADLIGHT : i >= (SIDE - 2) * SIDE ? CAR_MASK_BRAKE : CAR_MASK_PAINT;
    if (!car_mask_write(mask, SIDE, SIDE, &png, &n)) return 0;
    char path[400];
    snprintf(path, sizeof path, "%s/side_mask.png", dir);
    const int ok = write_file(path, png, n);
    png_free(png);
    return ok;
}

static int make_mask(const char *dir) { return make_mask_of(dir, 0); }

/* an envmap PNG of one grey `level` (128 is the neutral: no lift, no shade) */
static int make_envmap(const char *path, uint8_t level)
{
    static uint8_t img[16 * 16 * 4];
    for (int i = 0; i < 16 * 16; i++) img[4 * i] = img[4 * i + 1] = img[4 * i + 2] = level, img[4 * i + 3] = 255;
    uint8_t *png = NULL;
    size_t n = 0;
    const int ok = png_write_rgba(img, 16, 16, &png, &n) == PNG_OK && write_file(path, png, n);
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

/* M5: the body shine as the race draws it: the masks' paint texels go through the envmap's tables, the kit's car_envmap.png replaces the game's
 * image, it follows the night's palette, and a closed car's class maps are dropped (another car's texels may take their addresses) */
static ModelScene *open_box(const char *masks, int matte, char *path, size_t path_cap, PreviewIn *in)
{
    char dir[400];
    snprintf(dir, sizeof dir, "%s/car", base);
    mkdir(dir, 0777), mkdir(masks, 0777);
    size_t len = 0;
    uint8_t *cfm = make_cfm(&len);
    CHECK(cfm != NULL);
    if (cfm == NULL) return NULL;
    snprintf(path, path_cap, "%s/BOX.CFM", dir);
    CHECK(write_file(path, cfm, len));
    free(cfm);
    CHECK(make_mask_of(masks, matte));
    memset(in, 0, sizeof *in);
    in->cfm = path, in->stem = "BOX", in->masks_dir = masks, in->added = 1;
    char why[200] = "";
    ModelScene *m = model_scene_open_painted(in, why, sizeof why);
    if (m == NULL) fprintf(stderr, "open_box: %s\n", why);
    return m;
}

static void test_the_body_shine(void)
{
    char masks[400], path[500], grey[500], bright[500];
    snprintf(masks, sizeof masks, "%s/shine_masks", base);
    snprintf(grey, sizeof grey, "%s/neutral_envmap.png", base);
    snprintf(bright, sizeof bright, "%s/bright_envmap.png", base);
    CHECK(make_envmap(grey, 128));
    CHECK(make_envmap(bright, 255));
    PreviewIn in;
    ModelScene *m = open_box(masks, 0, path, sizeof path, &in);
    CHECK(m != NULL);
    if (m == NULL) return;
    int sw = 0, sh = 0;
    const uint8_t *side = model_scene_texels(m, "side", &sw, &sh);
    CHECK(side != NULL);
    /* the masks' draft is made when the shine is first on, not before: a car looked at with the shine off never reads it */
    CHECK_EQ_INT(model_scene_shine_textures(m), 0);
    CHECK(shine_class_find(side) == NULL);
    CHECK_EQ_INT(model_scene_shine(m, 0, &in, NULL), 0);
    CHECK_EQ_INT(model_scene_shine_textures(m), 0);
    CHECK(shine_class_find(side) == NULL);
    Orbit o;
    orbit_init(&o, model_scene_radius(m));
    orbit_set(&o, 60 * 16, 30 * 16);
    CarNightState s;
    memset(&s, 0, sizeof s);
    const Picture plain = picture(m, &o, &s, "box_shine_off");
    CHECK(model_scene_shine(m, 1, &in, NULL));
    CHECK_EQ_INT(model_scene_shine_textures(m), 1);   /* the side, a body texture by its mask */
    CHECK(shine_class_find(side) != NULL);
    const Picture shine = picture(m, &o, &s, "box_shine_on");
    CHECK(model_scene_shine(m, 1, &in, grey));
    const Picture neutral = picture(m, &o, &s, NULL);
    CHECK(model_scene_shine(m, 1, &in, bright));
    const Picture lifted = picture(m, &o, &s, "box_shine_bright");
    CHECK_EQ_INT(model_scene_shine(m, 0, &in, NULL), 0);
    const Picture off_again = picture(m, &o, &s, NULL);
    fprintf(stderr, "shine: plain light %ld, procedural %ld, neutral %ld, bright %ld\n", plain.light, shine.light, neutral.light, lifted.light);
    CHECK_EQ_INT(shine.drawn, plain.drawn);
    CHECK(shine.hash != plain.hash);           /* the paint takes the envmap's light */
    CHECK_EQ_INT(neutral.hash, plain.hash);    /* the kit's own image is the one used: all neutral, no change */
    CHECK(lifted.light > plain.light);         /* all sky: lifted toward white */
    CHECK_EQ_INT(off_again.hash, plain.hash);
    s.night = 1, s.lit = 1, s.light[0] = s.light[1] = s.light[2] = 0.4f;
    CHECK(model_scene_shine(m, 0, &in, NULL) == 0);
    const Picture night = picture(m, &o, &s, NULL);
    CHECK(model_scene_shine(m, 1, &in, bright));
    CHECK_EQ_INT(model_scene_shine_textures(m), 1);   /* on again: the draft made once, not added to */
    const Picture night_shine = picture(m, &o, &s, "box_shine_night");
    CHECK(night_shine.hash != night.hash);     /* the tables follow the night's palette */
    CHECK(night_shine.light < lifted.light);
    CHECK(shine_class_find(side) != NULL);
    const uintptr_t side_at = (uintptr_t)side;   /* the address alone, as the class store keys it: never read after the close */
    model_scene_close(m);
    CHECK(shine_class_find((const uint8_t *)side_at) == NULL);   /* dropped with the car: a later file at that address finds nothing of it */
    /* another car after it: all matte, nothing of the first car's classes is left to find */
    char masks_b[400], path_b[500];
    snprintf(masks_b, sizeof masks_b, "%s/matte_masks", base);
    PreviewIn in_b;
    ModelScene *b = open_box(masks_b, 1, path_b, sizeof path_b, &in_b);
    CHECK(b != NULL);
    if (b == NULL) return;
    memset(&s, 0, sizeof s);
    const Picture b_plain = picture(b, &o, &s, NULL);
    model_scene_shine(b, 1, &in_b, bright);
    const Picture b_shine = picture(b, &o, &s, NULL);
    CHECK_EQ_INT(b_shine.hash, b_plain.hash);   /* all matte: no shine, and none of the closed car's */
    model_scene_close(b);
}

/* the CD's 911 with the game's own masks of it (skipped without original/): the front at night shows its headlights, the rear its brake lights */
/* The owner's police question (2026-11-23): the police car is not painted in the race, but its masks' shine (the green class) is drawn in the Model window as on the other cars: its body textures
 * (frnt, rear, side, bott of the high bank) are registered with the shine on, and a mask's texels of the body class make a different picture shine on than off. */
static void test_the_cds_police_shines(void)
{
    char path[512], masks[512];
    snprintf(path, sizeof path, "%s/original/SIMDATA/CARFAMS/COPMUST.CFM", TNFS_SOURCE_DIR);
    snprintf(masks, sizeof masks, "%s/data/car_masks/COPMUST", TNFS_SOURCE_DIR);
    if (access(path, R_OK) != 0) return;
    PreviewIn in;
    memset(&in, 0, sizeof in);
    in.cfm = path, in.stem = "COPMUST", in.masks_dir = masks;
    char why[200] = "";
    ModelScene *m = model_scene_open_painted(&in, why, sizeof why);
    CHECK(m != NULL);
    if (m == NULL) {
        fprintf(stderr, "COPMUST: %s\n", why);
        return;
    }
    CHECK(model_scene_shine(m, 1, &in, NULL));
    fprintf(stderr, "COPMUST shine textures: %d\n", model_scene_shine_textures(m));
    CHECK(model_scene_shine_textures(m) >= 3);   /* frnt, rear, side (bott too) */
    Orbit o;
    orbit_init(&o, model_scene_radius(m));
    orbit_set(&o, 60 * 16, 20 * 16);
    CarNightState s;
    memset(&s, 0, sizeof s);
    CHECK_EQ_INT(model_scene_shine(m, 0, &in, NULL), 0);
    const Picture plain = picture(m, &o, &s, "police_shine_off");
    CHECK(model_scene_shine(m, 1, &in, NULL));
    const Picture shine = picture(m, &o, &s, "police_shine_on");
    fprintf(stderr, "COPMUST shine: plain light %ld, shine %ld, hashes %s\n", plain.light, shine.light, shine.hash != plain.hash ? "differ" : "SAME");
    CHECK(shine.hash != plain.hash);   /* the green (body) texels reflect the sky on the unpainted car too */
    model_scene_close(m);
}

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

/* W1 (the owner's test, 2026-11-20): a texel of a texture with no mask (a wheel's) keeps its colour whatever lamps are lit. The box with its other five
 * faces in "tyr1" (all index 1) changes, between every lamps state and the dark night, in the pixels the plain box changes in (its lamps), give or
 * take the faces' edges */
static ModelScene *open_cfm(const char *name, int wheel, const char *masks)
{
    char path[500];
    const int np = snprintf(path, sizeof path, "%s/car/%s.CFM", base, name);
    CHECK(np > 0 && (size_t)np < sizeof path);
    size_t len = 0;
    uint8_t *cfm = make_cfm_of(&len, wheel);
    CHECK(cfm != NULL && write_file(path, cfm, len));
    free(cfm);
    PreviewIn in;
    memset(&in, 0, sizeof in);
    in.cfm = path, in.stem = "BOX", in.masks_dir = masks, in.added = 1;
    char why[200] = "";
    ModelScene *m = model_scene_open_painted(&in, why, sizeof why);
    if (m == NULL) fprintf(stderr, "open_cfm %s: %s\n", name, why);
    return m;
}

static int changed_pixels(ModelScene *m, const Orbit *o, const CarNightState *a, const CarNightState *b, int *lamps)
{
    static uint8_t first[W * H * 4];
    CHECK(model_scene_render(m, o, W, H, a, first));
    CHECK(model_scene_render(m, o, W, H, b, rgba));
    *lamps = model_scene_lamp_pixels(m);
    int changed = 0;
    for (int i = 0; i < W * H; i++) changed += memcmp(first + 4 * i, rgba + 4 * i, 4) != 0;
    return changed;
}

static void test_a_texture_with_no_mask_keeps_its_colours(void)
{
    char dir[400], masks[400];
    const int nd = snprintf(dir, sizeof dir, "%s/car", base), nm = snprintf(masks, sizeof masks, "%s/masks_w1", base);
    CHECK(nd > 0 && (size_t)nd < sizeof dir && nm > 0 && (size_t)nm < sizeof masks);
    mkdir(dir, 0777), mkdir(masks, 0777);
    CHECK(make_mask(masks));
    ModelScene *plain = open_cfm("PLAIN", 0, masks), *wheel = open_cfm("WHEEL", 1, masks);
    CHECK(plain != NULL && wheel != NULL);
    if (plain == NULL || wheel == NULL) {
        model_scene_close(plain), model_scene_close(wheel);
        return;
    }
    Orbit o;
    orbit_init(&o, model_scene_radius(plain));
    orbit_set(&o, 60 * 16, 30 * 16);
    CarNightState dark;
    memset(&dark, 0, sizeof dark);
    dark.night = 1, dark.lit = 1, dark.light[0] = dark.light[1] = dark.light[2] = 0.4f;
    const int states[5][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 }, { 1, 1, 1 }, { 0, 0, 1 } };   /* lights, brake, reverse: the owner's combinations */
    for (int k = 0; k < 5; k++) {
        CarNightState s = dark;
        s.lights = states[k][0], s.brake = states[k][1], s.reverse = states[k][2];
        int plain_lamps = 0, wheel_lamps = 0;
        const int plain_changed = changed_pixels(plain, &o, &dark, &s, &plain_lamps), wheel_changed = changed_pixels(wheel, &o, &dark, &s, &wheel_lamps);
        fprintf(stderr, "W1 state %d: plain %d changed %d lamps, wheel %d changed %d lamps\n", k, plain_changed, plain_lamps, wheel_changed, wheel_lamps);
        CHECK_EQ_INT(wheel_changed - wheel_lamps, plain_changed - plain_lamps);   /* besides the lamps only the side's brake lamp entry (W2), in both */
        CHECK(abs(wheel_lamps - plain_lamps) <= 1 + plain_lamps / 100);   /* and the lamps are the side's: the faces' edges are drawn a pixel apart */
    }
    model_scene_close(plain), model_scene_close(wheel);
}

/* W2 (the owner's test, 2026-11-20): the texels of entry 0xfe (the brake lamp, phosphor green in the files) show what the race paints there (formats/car_brake_lamp.h):
 * the lamp's unlit colour by day and at night, its lit one with the brake, never the file's green; in the Model and in the Preview alike */
static int count_colour(const uint8_t *px, int n, int r, int g, int b)
{
    int k = 0;
    for (int i = 0; i < n; i++) k += px[4 * i + 3] != 0 && px[4 * i] == r && px[4 * i + 1] == g && px[4 * i + 2] == b;
    return k;
}

static void test_the_brake_lamp_is_the_races(void)
{
    char masks[400];
    const int nm = snprintf(masks, sizeof masks, "%s/masks_w2", base);
    CHECK(nm > 0 && (size_t)nm < sizeof masks);
    mkdir(masks, 0777);
    CHECK(make_mask(masks));
    ModelScene *m = open_cfm("LAMP", 0, masks);
    CHECK(m != NULL);
    if (m == NULL) return;
    Orbit o;
    orbit_init(&o, model_scene_radius(m));
    orbit_set(&o, 60 * 16, 30 * 16);
    CarNightState s;
    memset(&s, 0, sizeof s);
    CHECK(model_scene_render(m, &o, W, H, &s, rgba));
    const int green_day = count_colour(rgba, W * H, 172, 252, 64);
    const int off_day = count_colour(rgba, W * H, 61, 12, 12);   /* (15,3,3) six bits, the standard row of an added car */
    s.brake = 1;
    CHECK(model_scene_render(m, &o, W, H, &s, rgba));
    const int lit_day = count_colour(rgba, W * H, 251, 20, 20);   /* (62,5,5) */
    memset(&s, 0, sizeof s);
    s.night = 1, s.lit = 1, s.light[0] = s.light[1] = s.light[2] = 0.4f;
    CHECK(model_scene_render(m, &o, W, H, &s, rgba));
    const int off_night = count_colour(rgba, W * H, 60, 12, 12);   /* untinted at night: written after the light */
    fprintf(stderr, "W2 box: green %d, off by day %d, lit by day %d, off at night %d\n", green_day, off_day, lit_day, off_night);
    CHECK_EQ_INT(green_day, 0);
    CHECK(off_day > 20);
    CHECK(lit_day >= off_day);
    CHECK_EQ_INT(off_night, off_day);
    const CarPreview *pv = model_scene_preview(m);   /* the Preview's own picture: the lamp unlit, not a pattern */
    int lamp_texels = 0, right = 0;
    for (int i = 0; pv != NULL && i < pv->count; i++)
        for (int k = 0; pv->tex[i].index != NULL && k < pv->tex[i].info.width * pv->tex[i].info.height; k++)
            if (pv->tex[i].index[k] == 0xfe) lamp_texels++, right += memcmp(pv->tex[i].rgba + 4 * k, (const uint8_t[]){ 61, 12, 12 }, 3) == 0;
    CHECK(lamp_texels > 0 && lamp_texels % SIDE == 0 && right == lamp_texels);   /* the row, in each bank */
    model_scene_close(m);
}

/* the same on every car of the CD (skipped without original/): each 0xfe texel, in the Preview's picture and at night with the lights off, is the race's unlit lamp of
 * that model; with the brake a red, never the file's colour */
static int check_cd_car(const char *dir, const char *file)
{
    char path[700], masks[700], stem[64];
    const int np = snprintf(path, sizeof path, "%s/%s", dir, file), ns = snprintf(stem, sizeof stem, "%.*s", (int)(strlen(file) - 4), file);
    const int nm = snprintf(masks, sizeof masks, "%s/data/car_masks/%s", TNFS_SOURCE_DIR, stem);
    CHECK(np > 0 && (size_t)np < sizeof path && ns > 0 && (size_t)ns < sizeof stem && nm > 0 && (size_t)nm < sizeof masks);
    PreviewIn in;
    memset(&in, 0, sizeof in);
    in.cfm = path, in.stem = stem, in.masks_dir = masks;
    char why[200] = "";
    ModelScene *m = model_scene_open_painted(&in, why, sizeof why);
    if (m == NULL) return 0;   /* not a car the Model opens (a far-only file): nothing to show */
    const CarPreview *pv = model_scene_preview(m);
    uint8_t off_day[768] = { 0 }, off6[3], lit6[3];
    car_preview_lamp_entry(pv, 0, off_day);
    car_brake_lamp_colour(car_brake_lamp_row(stem), 0, off6), car_brake_lamp_colour(car_brake_lamp_row(stem), 1, lit6);
    CarNightState dark, brake;
    memset(&dark, 0, sizeof dark);
    dark.night = 1, dark.lit = 1, dark.light[0] = dark.light[1] = dark.light[2] = 0.4f, dark.lamp_row = pv->lamp_row;
    memset(&brake, 0, sizeof brake);
    brake.brake = 1, brake.lamp_row = pv->lamp_row;
    int texels = 0, wrong = 0;
    for (int i = 0; i < pv->count; i++) {
        const PreviewTex *t = &pv->tex[i];
        const int n = t->info.width * t->info.height;
        if (t->index == NULL || memchr(t->index, 0xfe, (size_t)n) == NULL) continue;
        uint8_t *night = malloc((size_t)n * 4), *lit = malloc((size_t)n * 4);
        const int ok = night != NULL && lit != NULL && car_night_texture(t->index, t->mask, t->info.width, t->info.height, pv->palette[t->info.level ? 1 : 0], &dark, night) &&
                       car_night_texture(t->index, t->mask, t->info.width, t->info.height, pv->palette[t->info.level ? 1 : 0], &brake, lit);
        CHECK(ok);
        for (int k = 0; ok && k < n; k++) {
            if (t->index[k] != 0xfe) continue;
            texels++;
            const uint8_t *d = t->rgba + 4 * k, *g = night + 4 * k, *b = lit + 4 * k;
            const int day_ok = memcmp(d, off_day + 3 * 0xfe, 3) == 0;
            const int night_ok = g[0] == (uint8_t)(off6[0] << 2 | off6[0] >> 4) && g[1] == (uint8_t)(off6[1] << 2 | off6[1] >> 4);
            const int brake_ok = b[0] > b[1] && b[0] > b[2] && b[0] >= (uint8_t)(lit6[0] << 2) - 8;   /* the lit row, or the mask's brake light over it */
            wrong += !(day_ok && night_ok && brake_ok);
        }
        free(night), free(lit);
    }
    if (texels > 0) fprintf(stderr, "W2 %s: row %d, %d lamp texels, %d wrong\n", stem, pv->lamp_row, texels, wrong);
    CHECK_EQ_INT(wrong, 0);
    model_scene_close(m);
    return texels > 0;
}

static void test_the_cds_brake_lamps(void)
{
    char dir[600];
    const int nd = snprintf(dir, sizeof dir, "%s/original/SIMDATA/CARFAMS", TNFS_SOURCE_DIR);
    CHECK(nd > 0 && (size_t)nd < sizeof dir);
    DIR *d = opendir(dir);
    if (d == NULL) return;
    int cars = 0;   /* the cars opened that have brake lamp texels: every race car and most traffic (33 on the CD) */
    for (struct dirent *e; (e = readdir(d)) != NULL;) {
        const size_t len = strlen(e->d_name);
        if (len < 5 || len > 40 || strcasecmp(e->d_name + len - 4, ".CFM") != 0) continue;
        cars += check_cd_car(dir, e->d_name);
    }
    closedir(d);
    fprintf(stderr, "W2: %d CD cars checked\n", cars);
    CHECK(cars >= 30);
}

/* W6: «Use as card photo»: the Model's view at 309 x 166 over a plain backdrop, card.png RGBA with no transparency, written whole or not at all */
static void test_the_card_photo(void)
{
    char dir[400], masks[400], path[500], card[500], why[300] = "";
    snprintf(dir, sizeof dir, "%s/cardcar", base), snprintf(masks, sizeof masks, "%s/cardmasks", base);
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
    ModelScene *m = model_scene_open_painted(&in, why, sizeof why);
    CHECK(m != NULL);
    if (m == NULL) return;
    Orbit o;
    orbit_init(&o, model_scene_radius(m));
    orbit_set(&o, 60 * 16, 30 * 16);
    snprintf(card, sizeof card, "%s/card.png", dir);
    CHECK(!card_photo_exists(dir));
    CardBackdrop plain = card_backdrop_default();
    plain.top[0] = 20, plain.top[1] = 40, plain.top[2] = 60;
    static uint8_t backdrop[CARD_PHOTO_W * CARD_PHOTO_H * 4];
    CHECK(card_backdrop_fill(&plain, CARD_PHOTO_W, CARD_PHOTO_H, backdrop, why, sizeof why));
    const uint8_t *back = plain.top;
    CHECK(card_photo_write(m, &o, NULL, backdrop, dir, why, sizeof why));
    CHECK(card_photo_exists(dir));
    FILE *f = fopen(card, "rb");
    CHECK(f != NULL);
    if (f != NULL) {
        static uint8_t data[1 << 20];
        const size_t n = fread(data, 1, sizeof data, f);
        fclose(f);
        PngImage img;
        CHECK(png_read(data, n, &img) == PNG_OK);
        CHECK(img.width == CARD_PHOTO_W && img.height == CARD_PHOTO_H && !img.indexed);   /* 309 x 166, as PIECE.CARD wants */
        int car = 0, clear = 0;
        for (size_t i = 0; i < (size_t)CARD_PHOTO_W * CARD_PHOTO_H; ++i) {
            const uint8_t *px = img.pixels + 4 * i;
            if (px[3] != 255) clear++;   /* never transparent: the card has a backdrop */
            if (px[0] != back[0] || px[1] != back[1] || px[2] != back[2]) car++;
        }
        CHECK_EQ_INT(clear, 0);
        CHECK(car > 500 && car < CARD_PHOTO_W * CARD_PHOTO_H);   /* the car over the backdrop, which shows round it */
        CHECK(img.pixels[0] == back[0] && img.pixels[1] == back[1] && img.pixels[2] == back[2]);   /* the corner is the backdrop's */
        png_free(img.pixels);
    }
    char tmp[600];
    snprintf(tmp, sizeof tmp, "%s.tmp", card);
    CHECK(access(tmp, F_OK) != 0);   /* staged, then renamed */
    char nowhere[500];
    snprintf(nowhere, sizeof nowhere, "%s/no-such-folder", base);
    CHECK(!card_photo_write(m, &o, NULL, backdrop, nowhere, why, sizeof why) && why[0] != '\0');
    CHECK(!card_photo_write(NULL, &o, NULL, backdrop, dir, why, sizeof why));
    model_scene_close(m);
}

/* the car framed (B2): at the home camera the car is small in a short, wide picture (the scale is that of the smaller side); model_scene_frame puts it at the distance
 * where it fills 80 % of the picture's larger share. The share is measured on the picture drawn: the box of the pixels with any alpha */
enum { FRAME_W_MAX = 1280, FRAME_H_MAX = 800 };
static int drawn_share(ModelScene *m, const Orbit *o, int w, int h)
{
    static uint8_t big[FRAME_W_MAX * FRAME_H_MAX * 4];
    CHECK(model_scene_render(m, o, w, h, NULL, big));
    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (big[4 * ((size_t)y * w + x) + 3] != 0) {
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
    if (x1 < 0) return 0;
    const int by_w = 100 * (x1 - x0 + 1) / w, by_h = 100 * (y1 - y0 + 1) / h;
    return by_w > by_h ? by_w : by_h;
}

static void check_framed(ModelScene *m, const char *what)
{
    static const int SIZES[][2] = { { 640, 200 }, { 1230, 520 }, { 300, 600 }, { 500, 500 } };
    for (size_t i = 0; i < sizeof SIZES / sizeof SIZES[0]; i++) {
        const int w = SIZES[i][0], h = SIZES[i][1];
        Orbit o;
        orbit_init(&o, model_scene_radius(m));
        const int before = drawn_share(m, &o, w, h);
        CHECK(model_scene_frame(m, &o, w, h, 80));
        const int after = drawn_share(m, &o, w, h);
        if (after < 70 || after > 80 || (after < before && before <= 80)) fprintf(stderr, "frame %s %dx%d: %d%% before, %d%% after\n", what, w, h, before, after);
        CHECK(after >= 70 && after <= 80);   /* as large as 80 % lets it be (a car off the middle of the picture is cut by the edge sooner) */
        CHECK(after >= before || before > 80);
        CHECK(o.dist_home == o.dist && o.dist >= o.dist_min && o.dist <= o.dist_max);
        orbit_home(&o);
        CHECK(o.dist == o.dist_home);   /* Home comes back to the framed distance */
    }
    Orbit o;
    orbit_init(&o, model_scene_radius(m));
    CHECK(!model_scene_frame(m, &o, 640, 200, 0) && !model_scene_frame(m, &o, 640, 200, 101) && !model_scene_frame(m, &o, 1, 200, 80));
}

/* the Model view's picture has the panel's shape (model_view_frame_size): the car framed in a small picture of that shape is whole, away from every edge, in the larger one the panel draws, at the home camera,
 * Front and Side, in a wide panel and in a tall one */
static void check_edges_clear(ModelScene *m, const char *what)
{
    static const int SHAPES[][2] = { { 320, 100 }, { 150, 240 }, { 240, 180 } };   /* wide, tall, the game's 4:3 */
    static const OrbitView VIEWS[] = { ORBIT_FRONT, ORBIT_SIDE };
    for (size_t i = 0; i < sizeof SHAPES / sizeof SHAPES[0]; i++)
        for (size_t v = 0; v < 3; v++) {
            const int fw = SHAPES[i][0], fh = SHAPES[i][1], w = 2 * fw, h = 2 * fh;
            Orbit o;
            orbit_init(&o, model_scene_radius(m));
            if (v > 0) orbit_view(&o, VIEWS[v - 1]);
            CHECK(model_scene_frame(m, &o, fw, fh, 80));
            static uint8_t pic[FRAME_W_MAX * FRAME_H_MAX * 4];
            CHECK(model_scene_render(m, &o, w, h, NULL, pic));
            int drawn = 0, edge = 0;
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++) {
                    if (pic[4 * ((size_t)y * w + x) + 3] == 0) continue;
                    drawn++;
                    if (x == 0 || y == 0 || x == w - 1 || y == h - 1) edge++;
                }
            if (drawn == 0 || edge != 0) fprintf(stderr, "edges %s %dx%d view %zu: %d drawn, %d on the edge\n", what, w, h, v, drawn, edge);
            CHECK(drawn > 0 && edge == 0);
        }
}

static void test_the_car_is_framed(void)
{
    char masks[400];
    snprintf(masks, sizeof masks, "%s/masks", base);
    ModelScene *m = open_cfm("FRAMED", 0, masks);
    CHECK(m != NULL);
    if (m == NULL) return;
    check_framed(m, "the synthetic car");
    check_edges_clear(m, "the synthetic car");
    model_scene_close(m);
}

/* the same on every car of the CD (skipped without original/): the 911, the other racers, the traffic cars and the police car */
static void test_the_cds_cars_are_framed(void)
{
    char dir[600];
    const int nd = snprintf(dir, sizeof dir, "%s/original/SIMDATA/CARFAMS", TNFS_SOURCE_DIR);
    CHECK(nd > 0 && (size_t)nd < sizeof dir);
    DIR *d = opendir(dir);
    if (d == NULL) return;
    int cars = 0;
    for (struct dirent *e; (e = readdir(d)) != NULL;) {
        const size_t len = strlen(e->d_name);
        if (len < 5 || len > 40 || strcasecmp(e->d_name + len - 4, ".CFM") != 0) continue;
        char path[700], why[200] = "";
        CHECK(snprintf(path, sizeof path, "%s/%s", dir, e->d_name) < (int)sizeof path);
        ModelScene *m = model_scene_open(path, why, sizeof why);
        if (m == NULL) continue;   /* not a car the Model opens */
        check_framed(m, e->d_name);
        model_scene_close(m);
        cars++;
    }
    closedir(d);
    fprintf(stderr, "B2: %d CD cars framed\n", cars);
    CHECK(cars >= 19);
}

int main(void)
{
    snprintf(base, sizeof base, "%s/model_scene_XXXXXX", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp");
    if (mkdtemp(base) == NULL) return 2;
    test_a_synthetic_car();
    test_the_card_photo();
    test_the_body_shine();
    test_the_cds_911();
    test_the_cds_police_shines();
    test_a_texture_with_no_mask_keeps_its_colours();
    test_the_brake_lamp_is_the_races();
    test_the_cds_brake_lamps();
    test_the_car_is_framed();
    test_the_cds_cars_are_framed();
    return TEST_REPORT();
}
