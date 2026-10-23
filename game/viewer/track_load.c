/* The viewers' track loading (track_load.h): the TRI and FAM, the art, the INFO's sky and tint, the scenery's models and
 * a car parked ahead. */
#include "viewer/track_load.h"

#include "race/fam_mirror.h"
#include "race/lunar_springs.h"

#include "viewer/screen_layout.h"
#include "viewer/ported_view.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 0043a370's file: the INFO text read whole (the port's name, not "%s%sInfo"). */
typedef struct {
    const char *p, *end;
    Track *t;
} InfoFile;

static int32_t info_open(void *ctx, const char *name) { (void)name; return ((InfoFile *)ctx)->p < ((InfoFile *)ctx)->end; }
static void info_close(void *ctx, int32_t file) { (void)ctx, (void)file; }

static int info_gets(void *ctx, char *line, int32_t size, int32_t file)
{
    (void)file;
    InfoFile *f = ctx;
    if (f->p >= f->end) return 0;
    int32_t n = 0;
    while (f->p < f->end && n + 1 < size) {
        const char c = *f->p++;
        line[n++] = c;
        if (c == '\n') break;
    }
    line[n] = '\0';
    return 1;
}

static void info_tint(void *ctx, int32_t red, int32_t green, int32_t blue, int32_t saturation, int32_t brightness)
{
    static RacePalette scratch;  /* 004471ac on the tint's words alone */
    race_palette_set_tint(&scratch, red, green, blue, saturation, brightness);
    ((InfoFile *)ctx)->t->tint = scratch.tint;
}

/* 0043a370 for the track `name` (its stem lower case as the frontend gives it), the time of day (004fa0ec), 640 x 480. */
static void load_info(Track *t, const char *name, int time_of_day, const char *text, size_t size)
{
    char stem[16] = { 0 };
    for (size_t i = 0; name[i] != '\0' && i + 1 < sizeof stem; i++) stem[i] = (char)tolower((unsigned char)name[i]);
    InfoFile f = { text, text + size, t };
    t->tint = (RacePaletteTint){ 0x10000, 0x100, 0, 0x100, 0x100 };
    const TrackInfoOps ops = { &f, info_open, info_gets, info_close, info_tint };
    const TrackInfoInputs in = { stem, "", 1, time_of_day, 1 };
    memset(&t->info, 0, sizeof t->info);
    track_info_load(&ops, &in, &t->info);
}

uint8_t *track_viewer_read_any(const char *data_root, const char *dir, const char *name, const char *const *suffixes,
                         size_t *size)
{
    for (int i = 0; suffixes[i] != NULL; i++) {
        char path[512];
        snprintf(path, sizeof path, "%s/SIMDATA/%s/%s%s", data_root, dir, name, suffixes[i]);
        uint8_t *data = platform_read_file(path, size);
        if (data != NULL) return data;
    }
    fprintf(stderr, "track_viewer: no %s file for %s in %s\n", dir, name, data_root);
    return NULL;
}

/* 004510e8's FAM for the time of day and the variant (track_fam.h) under data_root, its ".fam" as the CD's capitals
 * on a second try; NULL when the CD holds none. */
static uint8_t *read_fam_named(const char *data_root, const char *name, const TrackFamInputs *look, size_t *size)
{
    char dir[400], dir_midday[400], path[512];
    snprintf(dir, sizeof dir, "%s/SIMDATA/NTRACKFM", data_root);
    snprintf(dir_midday, sizeof dir_midday, "%s/SIMDATA/%s", data_root, resource_german() ? "GTRACKFM" : "ETRACKFM");
    TrackFamInputs in = *look;
    in.stem = name, in.dir = dir, in.dir_midday = dir_midday;
    track_fam_name(&in, 1, path, sizeof path);
    /* port: TNFS_TRACE_FAM names the FAM 004510e8 chose (its "m"/"r" and the TR1->TR5 remap decide it), so a
     * both-variants race can be told apart from a mirror-only one without reading the art */
    if (getenv("TNFS_TRACE_FAM") != NULL)
        fprintf(stderr, "track_viewer: fam %s (m=%d r=%d both=%d)\n", path, in.variant_m, in.variant_r,
                in.port_both_variants);
    uint8_t *data = platform_read_file(path, size);
    const size_t n = strlen(path);
    if (data == NULL && n >= 3) {
        for (size_t i = n - 3; i < n; i++) path[i] = (char)toupper((unsigned char)path[i]);
        data = platform_read_file(path, size);
    }
    if (data == NULL) fprintf(stderr, "track_viewer: no %s\n", path);
    return data;
}

/* port: the midday FAM of `look`'s track with the variants given, for the art the CD does not hold (fam_mirror.h) */
static uint8_t *read_fam_variant(const char *data_root, const char *name, const TrackFamInputs *look, int variant_m,
                                 int variant_r, size_t *size)
{
    TrackFamInputs in = *look;
    in.time_of_day = 1, in.variant_m = variant_m, in.variant_r = variant_r, in.port_both_variants = 0;
    return read_fam_named(data_root, name, &in, size);
}

/* Port guard: a FAM the CD does not hold falls back to the first of NTRACKFM's. */
static uint8_t *read_fam(const char *data_root, const char *name, const TrackFamInputs *look, size_t *size)
{
    /* port: Lunar Springs (lunar_springs.h): the rally on Rusty Springs reads the moon's textures, the TRI staying Rusty's;
     * a CD without them falls back to the rally's own FAM */
    char moon[512];
    if (lunar_springs_fam(data_root, look->track, look->variant_r, look->port_lunar, moon, sizeof moon)) {
        uint8_t *moon_data = platform_read_file(moon, size);
        if (moon_data != NULL) return moon_data;
        fprintf(stderr, "track_viewer: no %s, the rally's own FAM\n", moon);
    }
    uint8_t *data = read_fam_named(data_root, name, look, size);
    if (data != NULL) return data;
    static const char *const FAM_SUFFIX[] = { "_T01.FAM", "_M01.FAM", "_R01.FAM", NULL };
    return track_viewer_read_any(data_root, "NTRACKFM", name, FAM_SUFFIX, size);
}

/* port: --drive without the menus has no setup to give 004fa0ec: a track the CD holds no FAM for at time 0 (TR4's:
 * no _T01) is taken at midday as the game takes it (004fbae0 = 1, ETRACKFM's _001) */
int track_viewer_fam_missing(const char *data_root, const char *name, const TrackFamInputs *look)
{
    size_t size = 0;
    uint8_t *data = read_fam_named(data_root, name, look, &size);
    platform_free_file(data);
    return data == NULL;
}

/* The track `name` with 004510e8's art and 0043a370's sky for `look` (its time of day, variants and track). */
int track_viewer_load(Track *t, const char *data_root, const char *name, const TrackFamInputs *look)
{
    static const char *const TRI_SUFFIX[] = { ".TRI", ".tri", NULL };
    size_t tri_size = 0, fam_size = 0;
    memset(t, 0, sizeof *t);
    t->tri_file = track_viewer_read_any(data_root, "MISC", name, TRI_SUFFIX, &tri_size);
    t->fam_file = read_fam(data_root, name, look, &fam_size);
    if (t->tri_file == NULL || t->fam_file == NULL) return 0;
    if (look->time_of_day == 1 && look->port_both_variants && look->variant_m && look->variant_r) {
        /* port: the rally's art on the mirror's geometry; the signs the mirror's own art holds flipped are flipped */
        size_t mirror_size = 0;
        uint8_t *const m = read_fam_variant(data_root, name, look, 1, 0, &mirror_size);
        if (m != NULL) fam_mirror_signs(t->fam_file, fam_size, t->fam_file, fam_size, m, mirror_size);
        platform_free_file(m);
    } else if (look->time_of_day != 1 && (look->variant_m || look->variant_r)) {
        /* port: a variant at another time of day (extras.cfg's variant_time), on the _T01 art: the rally's own
         * bitmaps darkened into it, then the mirror's signs flipped */
        size_t day_size = 0, other_size = 0;
        uint8_t *const day = read_fam_variant(data_root, name, look, 0, 0, &day_size);
        uint8_t *const rally = day != NULL && look->variant_r ? read_fam_variant(data_root, name, look, 0, 1, &other_size)
                                                              : NULL;
        const int put = rally != NULL ? fam_time_rally(t->fam_file, fam_size, day, day_size, rally, other_size) : 0;
        platform_free_file(rally);
        uint8_t *const mirror = day != NULL && look->variant_m ? read_fam_variant(data_root, name, look, 1, 0, &other_size)
                                                               : NULL;
        const int flipped = mirror != NULL ? fam_mirror_signs(t->fam_file, fam_size, day, day_size, mirror, other_size)
                                           : 0;
        if (getenv("TNFS_TRACE_FAM") != NULL)
            fprintf(stderr, "track_viewer: time %d art: %d rally bitmaps put, %d signs flipped\n", look->time_of_day, put,
                    flipped);
        platform_free_file(mirror);
        platform_free_file(day);
    }
    TriResult tr = tri_open(&t->tri, t->tri_file, tri_size);
    ChunkResult cr = chunk_open(&t->fam, t->fam_file, fam_size);
    if (tr != TRI_OK || cr != CHUNK_OK || !track_art_load(&t->art, &t->fam)) {
        fprintf(stderr, "track_viewer: %s: tri %s, fam %s\n", name, tri_result_str(tr), chunk_result_str(cr));
        return 0;
    }
    track_art_load_animated(&t->art, &t->fam, t->tri.data);  /* 0044c36c's "ga%d0" */
    char info_path[512];
    size_t info_size = 0;
    snprintf(info_path, sizeof info_path, "%s/SIMDATA/MISC/%sINFO", data_root, name);
    uint8_t *info_text = platform_read_file(info_path, &info_size);
    if (info_text == NULL) fprintf(stderr, "track_viewer: no %s, the built-in sky settings\n", info_path);
    load_info(t, name, look->time_of_day, (const char *)info_text, info_text != NULL ? info_size : 0);
    platform_free_file(info_text);
    t->objects = calloc(t->tri.scenery_count + 1u, sizeof *t->objects);
    if (t->objects == NULL) return 0;
    Chunk objects, pair;
    if (chunk_child(&t->fam, FAM_CHILD_OBJECTS, &objects) == CHUNK_OK && objects.kind == CHUNK_KIND_HEADER)
        for (uint32_t i = 0; i < objects.count && i < TRACK_MODELS; i++)
            t->model_ok[i] = chunk_child(&objects, i, &pair) == CHUNK_OK && model_load(&t->models[i], &pair);
    for (uint32_t i = 0; i < t->tri.scenery_count; i++)
        if (track_object(&t->tri, i, &t->objects[t->object_count]) == TRI_OK) t->object_count++;
    return 1;
}

/* A CFM's high (children 0, 1) and low (2, 3) detail models. */
static int load_car_half(ViewModel *m, const Chunk *cfm, uint32_t mesh, uint32_t bitmaps)
{
    Chunk orip, shpi;
    if (chunk_child(cfm, mesh, &orip) != CHUNK_OK || mesh_open(&m->mesh, &orip) != MESH_OK) return 0;
    m->has_textures = chunk_child(cfm, bitmaps, &shpi) == CHUNK_OK && shpi.kind == CHUNK_KIND_BITMAP &&
                      chunk_bitmaps(&shpi, &m->textures) == FSH_OK;
    return 1;
}

/* CARFAMS/NAME.CFM into `file` (kept: the models point into it), its top
 * chunk in `cfm`. */
static int load_cfm(const char *data_root, const char *name, uint8_t **file, Chunk *cfm, ViewModel models[2])
{
    static const char *const suffixes[] = { ".CFM", ".cfm", NULL };
    size_t size = 0;
    memset(cfm, 0, sizeof *cfm);
    *file = track_viewer_read_any(data_root, "CARFAMS", name, suffixes, &size);
    const int ok = *file != NULL && chunk_open(cfm, *file, size) == CHUNK_OK &&
                   load_car_half(&models[0], cfm, CFM_CHILD_MESH_HIGH, CFM_CHILD_BITMAPS_HIGH) &&
                   load_car_half(&models[1], cfm, CFM_CHILD_MESH_LOW, CFM_CHILD_BITMAPS_LOW);
    if (!ok) fprintf(stderr, "track_viewer: no car %s\n", name);
    if (!ok) memset(cfm, 0, sizeof *cfm);
    return ok;
}

void track_viewer_load_car(Track *t, const char *data_root, const char *name)
{
    Chunk cfm;
    t->car_ok = load_cfm(data_root, name, &t->car_file, &cfm, t->car);
}

void track_viewer_free(Track *t)
{
    ported_view_forget_track();
    platform_free_file(t->car_file);
    platform_free_file(t->tri_file);
    platform_free_file(t->fam_file);
    free(t->objects);
}

int track_viewer_write_ppm(const char *path, const uint8_t *pixels, int w, int h, const uint8_t *rgb)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) return 0;
    int ok = fprintf(f, "P6\n%d %d\n255\n", w, h) > 0;
    for (int i = 0; ok && i < w * h; i++) ok = fwrite(rgb + 3 * pixels[i], 1, 3, f) == 3;
    return fclose(f) == 0 && ok;
}
