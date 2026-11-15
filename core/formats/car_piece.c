/* See car_piece.h. */
#include "formats/car_piece.h"

#include "formats/mod_ini.h"
#include "formats/chunk.h"
#include "formats/mesh.h"
#include "formats/pbs.h"
#include "formats/pdn.h"
#include "formats/qfs.h"
#include "formats/slots.h"
#include "formats/validate_internal.h"

#include <ctype.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static const char *const WORDS[] = { "supra", "diablo", "911", "zr1", "512tr", "viper", "nsx", "rx7" };
enum { WORD_COUNT = sizeof WORDS / sizeof WORDS[0] };

/* a root the race loads by that slots.h does not list: car_load.c's car_model_names[10] */
static const char *const RACE_ONLY_STEMS[] = { "P911T", NULL };

int car_piece_model_of_word(const char *word)
{
    if (word == NULL) return -1;
    for (int i = 0; i < WORD_COUNT; i++)
        if (strcmp(word, WORDS[i]) == 0) return i;
    return -1;
}

const char *car_piece_word_of_model(int model)
{
    return model >= 0 && model < WORD_COUNT ? WORDS[model] : NULL;
}

/* 1 when `stem` is `own` or 'M' + `own`, whatever the case */
static int names(const char *stem, const char *own)
{
    return strcasecmp(stem, own) == 0 || ((stem[0] == 'M' || stem[0] == 'm') && strcasecmp(stem + 1, own) == 0);
}

int car_piece_stem_ok(const char *stem)
{
    if (stem == NULL) return 0;
    const size_t n = strlen(stem);
    if (n == 0 || n > MOD_INI_STEM_MAX) return 0;
    for (size_t i = 0; i < n; i++)
        if (!((stem[i] >= 'A' && stem[i] <= 'Z') || (stem[i] >= '0' && stem[i] <= '9'))) return 0;
    for (const char *const *s = slot_stems(SLOT_CAR); *s != NULL; s++)
        if (names(stem, *s)) return 0;
    for (const char *const *s = RACE_ONLY_STEMS; *s != NULL; s++)
        if (names(stem, *s)) return 0;
    return 1;
}

void car_piece_paths(const char *stem, char paths[CAR_PIECE_FILES][CAR_PIECE_PATH])
{
    snprintf(paths[0], CAR_PIECE_PATH, "SIMDATA/CARFAMS/%s.CFM", stem);
    snprintf(paths[1], CAR_PIECE_PATH, "SIMDATA/CARFAMS/%s.PDN", stem);
    snprintf(paths[2], CAR_PIECE_PATH, "SIMDATA/CARSPECS/%s.PBS", stem);
}

/* the file of `path` in `files`, as it is, in upper case or in lower case (the overlay's rule, docs/modding.md) */
static const SetFile *find_any_case(const FileSet *files, const char *path)
{
    const SetFile *f = fileset_find(files, path);
    char other[CAR_PIECE_PATH];
    for (int lower = 0; f == NULL && lower < 2; lower++) {
        size_t i = 0;
        for (; path[i] != '\0' && i + 1 < sizeof other; i++)
            other[i] = (char)(lower ? tolower((unsigned char)path[i]) : toupper((unsigned char)path[i]));
        other[i] = '\0';
        f = fileset_find(files, other);
    }
    return f;
}

/* PIECE.FROM: `word` (key `key`) names an original car, or is not given and need not be */
static void from_ok(const char *key, const char *word, int required, Report *r)
{
    if (word[0] == '\0') {
        if (required) report_add(r, "PIECE.FROM", 0, "mod.ini: no %s", key);
        return;
    }
    if (car_piece_model_of_word(word) < 0)
        report_add(r, "PIECE.FROM", 0, "mod.ini: %s=%s is not supra, diablo, 911, zr1, 512tr, viper, nsx or rx7", key, word);
}

static int32_t le32(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
}

/* PIECE.SPECS: what the race divides by or walks, and the checksum without which the exe zeroes the power */
static void specs_ok(const SetFile *f, const char *path, Report *r)
{
    uint8_t rec[PBS_SIZE];
    if (!pbs_read(rec, f->data, f->size)) return;   /* validate_file said why */
    if (!pbs_checksum_ok(rec)) report_add(r, "PIECE.SPECS", 0, "%s: the checksum does not match (the game would zero its power)", path);
    const int32_t power = le32(rec + CAR_PIECE_PBS_POWER), points = le32(rec + CAR_PIECE_PBS_TORQUE_POINTS);
    if (power <= 0) report_add(r, "PIECE.SPECS", power, "%s: power is not above 0", path);
    if (le32(rec + CAR_PIECE_PBS_UNKNOWN_320) == 0) report_add(r, "PIECE.SPECS", 0, "%s: unknown_320 is 0", path);
    if (le32(rec + CAR_PIECE_PBS_FORCE_SCALE) == 0) report_add(r, "PIECE.SPECS", 0, "%s: force_scale is 0", path);
    if (points < CAR_PIECE_TORQUE_POINTS_MIN || points > CAR_PIECE_TORQUE_POINTS_MAX)
        report_add(r, "PIECE.SPECS", points, "%s: torque_points is not %d..%d", path, CAR_PIECE_TORQUE_POINTS_MIN, CAR_PIECE_TORQUE_POINTS_MAX);
}

/* ---- the model (tools stage A): what the race looks up in it and walks, which validate leaves to a car it knows by name */

enum { NAME_SIZE = 8, SIZE_SHIFT = 9, LIGHTS = 4, LIGHT_POINTS = 8, GROUPS = 4 };

/* a record's name against `want` as the race compares them (object3d.c labels_match): 8 bytes, letters in any case */
static int name_is(const char stored[NAME_SIZE], const char *want)
{
    for (int i = 0; i < NAME_SIZE; i++) {
        const int x = tolower((unsigned char)stored[i]), y = tolower((unsigned char)want[i]);
        if (x != y) return 0;
        if (y == 0) return 1;
    }
    return 1;
}

static int has_point(const Mesh *m, const char *name)
{
    for (uint32_t i = 0; i < m->section_c_count; i++) {
        MeshSectionC c;
        if (mesh_section_c(m, i, &c) == MESH_OK && name_is(c.name, name)) return 1;
    }
    return 0;
}

static int has_label(const Mesh *m, const char *name)
{
    for (uint32_t i = 0; i < m->label_count; i++) {
        MeshLabel l;
        if (mesh_label(m, i, &l) == MESH_OK && name_is(l.name, name)) return 1;
    }
    return 0;
}

/* PIECE.MODEL_NAME: the exhaust point, the lights' labels and their points the player's car is set up with (car_model.c signals()) */
static void model_names(const Mesh *m, const char *path, Report *r)
{
    static const char *const LABELS[LIGHTS] = { "lt_frnt", "rt_frnt", "lt_rear", "rt_rear" };
    static const char *const POINTS[LIGHT_POINTS] = { "fl0", "fl1", "fr0", "fr1", "rl0", "rl1", "rr0", "rr1" };
    if (!has_point(m, "smok")) report_add(r, "PIECE.MODEL_NAME", 0, "%s: high-detail mesh: no point 'smok' (the exhaust)", path);
    for (int i = 0; i < LIGHTS; i++)
        if (!has_label(m, LABELS[i])) report_add(r, "PIECE.MODEL_NAME", 0, "%s: high-detail mesh: no label '%s' (a light)", path, LABELS[i]);
    for (int i = 0; i < LIGHT_POINTS; i++)
        if (!has_point(m, POINTS[i])) report_add(r, "PIECE.MODEL_NAME", 0, "%s: high-detail mesh: no point '%s' (a light's)", path, POINTS[i]);
}

/* PIECE.MODEL_INDEX: every polygon's texture, corners and uvs inside their tables; one finding a kind, with the first and the count */
static void model_indexes(const Mesh *m, const char *path, const char *level, Report *r)
{
    static const char *const KIND[3] = { "texture", "vertex", "uv" };
    const uint32_t size[3] = { m->texture_count, m->vertex_count, m->uv_count };
    uint32_t bad[3] = { 0, 0, 0 }, first[3] = { 0, 0, 0 }, value[3] = { 0, 0, 0 };
    for (uint32_t i = 0; i < m->polygon_count; i++) {
        MeshPolygon p;
        uint32_t vertices[4], uvs[4], index[3];
        if (mesh_polygon(m, i, &p) != MESH_OK) break;
        const int corners = (p.flags & 7) < 4 ? p.flags & 7 : 4;
        const int read = mesh_polygon_corners(m, &p, vertices, uvs) == MESH_OK;
        for (int c = 0; c < (corners > 0 ? corners : 1); c++) {
            index[0] = p.texture;
            index[1] = read ? vertices[c] : UINT32_MAX;   /* slots past the map: no vertex to read */
            index[2] = read && p.uv_slot != p.vertex_slot ? uvs[c] : 0;
            for (int k = c == 0 ? 0 : 1; k < 3; k++) {
                if (index[k] < size[k] || (k == 2 && p.uv_slot == p.vertex_slot)) continue;
                if (bad[k]++ == 0) first[k] = i, value[k] = index[k];
            }
        }
    }
    for (int k = 0; k < 3; k++)
        if (bad[k] > 0)
            report_add(r, "PIECE.MODEL_INDEX", (int64_t)value[k], "%s: %s mesh: polygon %u's %s index %u is past the %u of its table (%u such)",
                       path, level, first[k], KIND[k], value[k], size[k], bad[k]);
}

/* PIECE.MODEL_GROUPS: a polygon of the high-detail mesh in none of the groups the race draws the car close up with (a warning) */
static void model_groups(const Mesh *m, const char *path, Report *r)
{
    static const char *const NAMES[GROUPS] = { "inside", "outside", "surface", "convex" };
    if (m->polygon_count == 0) return;
    uint8_t *seen = calloc(m->polygon_count, 1);
    if (seen == NULL) return;
    for (uint32_t i = 0; i < m->section_b_count; i++) {
        MeshSectionB b;
        int group = 0;
        if (mesh_section_b(m, i, &b) != MESH_OK) continue;
        for (int g = 0; g < GROUPS; g++) group |= strncmp(b.name, NAMES[g], NAME_SIZE) == 0;   /* as object3d_section */
        const uint32_t start = b.unknown[1], count = b.unknown[2];
        if (!group || (uint64_t)start + count > m->vertex_map_slots) continue;
        for (uint32_t k = 0; k < count; k++) {
            uint32_t polygon;
            if (mesh_vertex_map(m, start + k, &polygon) == MESH_OK && polygon < m->polygon_count) seen[polygon] = 1;
        }
    }
    uint32_t loose = 0, first = 0;
    for (uint32_t i = m->polygon_count; i-- > 0;)
        if (!seen[i]) loose++, first = i;
    free(seen);
    if (loose > 0)
        report_add(r, "PIECE.MODEL_GROUPS", loose, "%s: high-detail mesh: %u polygons, the first %u, are in no group (inside, outside, surface, convex)",
                   path, loose, first);
}

/* the model's half sizes x y z as the race measures them (car_model_size: its first unknown_14 vertices, from 0, << 9), halved */
static void model_halves(const Mesh *m, int64_t half[3])
{
    int32_t least[3] = { 0, 0, 0 }, most[3] = { 0, 0, 0 };
    const uint32_t count = m->unknown_14 < m->vertex_count ? m->unknown_14 : m->vertex_count;
    for (uint32_t i = 0; i < count; i++) {
        MeshVertex v;
        if (mesh_vertex(m, i, &v) != MESH_OK) break;
        const int32_t at[3] = { v.x, v.y, v.z };
        for (int k = 0; k < 3; k++) {
            if (at[k] < least[k]) least[k] = at[k];
            if (at[k] > most[k]) most[k] = at[k];
        }
    }
    for (int k = 0; k < 3; k++) half[k] = (((int64_t)most[k] - least[k]) << SIZE_SHIFT) / 2;
}

/* PIECE.BODY_SIZE: the PDN's body half sizes, those not 0, within a tenth of the model's (a warning: the race collides with the PDN's) */
static void body_size(const SetFile *pdn, const char *path, const int64_t half[3], Report *r)
{
    static const char AXIS[3] = { 'x', 'y', 'z' };
    uint8_t rec[PDN_SIZE];
    if (!pdn_read(rec, pdn->data, pdn->size)) return;   /* validate_file said why */
    for (int k = 0; k < 3; k++) {
        const int64_t body = pdn_i32(rec, 4 * k), off = body > half[k] ? body - half[k] : half[k] - body;
        if (body != 0 && off * 10 > half[k])
            report_add(r, "PIECE.BODY_SIZE", body, "%s: the body's half size in %c is %lld, the model's %lld", path, AXIS[k], (long long)body, (long long)half[k]);
    }
}

/* the CFM's model checked as a race car's, its names, indexes and groups; its half sizes in `half` (0 when unreadable) */
static void model_ok(const SetFile *f, const char *path, int64_t half[3], Report *r)
{
    half[0] = half[1] = half[2] = 0;
    const uint8_t *d = f->data;
    size_t n = f->size, unpacked_size = 0;
    uint8_t *unpacked = NULL;
    if (qfs_is_compressed(d, n)) {
        if (qfs_decompressed_size(d, n, &unpacked_size) != QFS_OK || (unpacked = malloc(unpacked_size ? unpacked_size : 1)) == NULL ||
            qfs_decompress(d, n, unpacked, unpacked_size) != QFS_OK) {
            free(unpacked);   /* validate_file said why */
            return;
        }
        d = unpacked, n = unpacked_size;
    }
    validate_car_piece_cfm(d, n, path, r);
    Chunk root, chunk;
    Mesh m;
    static const char *const LEVEL[2] = { "high-detail", "low-detail" };
    for (int level = 0; level < 2; level++) {
        if (chunk_open(&root, d, n) != CHUNK_OK || chunk_child(&root, 2 * (uint32_t)level, &chunk) != CHUNK_OK || mesh_open(&m, &chunk) != MESH_OK)
            break;
        model_indexes(&m, path, LEVEL[level], r);
        if (level > 0) continue;
        model_names(&m, path, r);
        model_groups(&m, path, r);
        model_halves(&m, half);
    }
    free(unpacked);
}

int car_piece_check(const ModInfo *card, const FileSet *files, Report *r)
{
    const size_t fatal_before = report_fatal_count(r);
    if (card->type != MOD_TYPE_CAR) report_add(r, "PIECE.TYPE", card->type, "mod.ini: type is not car");
    from_ok("sound_from", card->sound_from, 1, r);
    from_ok("ai_from", card->ai_from, 1, r);
    from_ok("dash_from", card->dash_from, 0, r);
    if (!car_piece_stem_ok(card->stem)) {
        report_add(r, "PIECE.STEM", (int64_t)strlen(card->stem), "mod.ini: stem \"%s\" is not 1..8 of A-Z 0-9, or is an original car's", card->stem);
        return 0;   /* no files to look for */
    }
    char paths[CAR_PIECE_FILES][CAR_PIECE_PATH];
    int found[CAR_PIECE_FILES] = { 0 };
    int64_t half[3] = { 0, 0, 0 };
    car_piece_paths(card->stem, paths);
    for (int i = 0; i < CAR_PIECE_FILES; i++) {
        const SetFile *f = find_any_case(files, paths[i]);
        if (f == NULL) {
            report_add(r, "PIECE.FILE_MISSING", 0, "%s is not in the piece", paths[i]);
            continue;
        }
        validate_file(f->data, f->size, paths[i], r);
        if (i == 0) model_ok(f, paths[i], half, r);
        if (i == 1 && found[0]) body_size(f, paths[i], half, r);
        if (i == CAR_PIECE_FILES - 1) specs_ok(f, paths[i], r);
        found[i] = 1;
    }
    return report_fatal_count(r) == fatal_before;
}
