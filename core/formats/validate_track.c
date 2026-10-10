/* validate for tracks (TRI, FAM) and car meshes (CFM): the limits of 0043e4e0, 0044c36c, 004576f0 and 00457d40
 * (core/formats/limits.c has the ids and addresses). */
#include "formats/chunk.h"
#include "formats/le.h"
#include "formats/mesh.h"
#include "formats/tri.h"
#include "formats/validate_internal.h"

#include <ctype.h>
#include <string.h>

enum { SEGMENT_FIELD = 6, CIRCUIT_FIELD = 4, FAM_BUFFER = 0x162400, SCENERY_AREA = TRI_SCENERY_MAX * TRI_SCENERY_SIZE, REPORT_LIMIT = 3 };

/* ------------------------------------------------------------------ TRI */

static int power_of_two(uint32_t v) { return v != 0 && (v & (v - 1)) == 0; }

void validate_tri(const uint8_t *d, size_t n, const char *name, Report *r)
{
    const Limit *segments = limit_find("TRI.SEGMENTS");
    int reported_segments = 0;
    if (n >= 8) {
        const uint16_t seg = (uint16_t)(d[SEGMENT_FIELD] | d[SEGMENT_FIELD + 1] << 8);
        if (seg > segments->max) { report_add(r, "TRI.SEGMENTS", seg, "%s: %u segments (offset 0x%x)", name, seg, SEGMENT_FIELD); reported_segments = 1; }
        const uint16_t circuit = (uint16_t)(d[CIRCUIT_FIELD] | d[CIRCUIT_FIELD + 1] << 8);
        if (circuit != 0 && !power_of_two((uint32_t)circuit * TRI_NODES_PER_SEGMENT))
            report_add(r, "TRI.CIRCUIT_POW2", (int64_t)circuit * TRI_NODES_PER_SEGMENT, "%s: %u circuit segments = %u nodes (offset 0x%x)", name, circuit, circuit * TRI_NODES_PER_SEGMENT, CIRCUIT_FIELD);
    }
    Tri t;
    const TriResult tr = tri_open(&t, d, n);
    if (tr != TRI_OK) {
        if (!reported_segments) report_add(r, "FILE.READABLE", 0, "%s: %s (file is %zu bytes; the SJBO chunk starts at offset 0x%x)", name, tri_result_str(tr), n, TRI_HEADER_SIZE);
        return;
    }
    const size_t area = (t.terrain_offset - t.scenery_offset) / TRI_SCENERY_SIZE;
    int terminated = 0;
    for (size_t i = 0; i < area && i < TRI_SCENERY_MAX; ++i) {
        if ((int32_t)le_u32(d + t.scenery_offset + i * TRI_SCENERY_SIZE) == -1) { terminated = 1; break; }
    }
    if (!terminated)
        report_add(r, "TRI.SCENERY_RECORDS", (int64_t)area, "%s: %zu placement records and no -1 terminator in the first %d (offset 0x%zx)", name, area, TRI_SCENERY_MAX, t.scenery_offset);
    if (t.scenery_slots < t.scenery_count)
        report_add(r, "TRI.SCENERY_SLOTS", (int64_t)t.scenery_slots, "%s: scenery_slots %u for %u placements (offset 0x%x)", name, t.scenery_slots, t.scenery_count, 0x16218);
    int bad = 0;
    for (uint32_t i = 0; i < t.scenery_count; ++i) {
        const uint8_t index = d[t.scenery_offset + (size_t)i * TRI_SCENERY_SIZE + 4];
        if (index >= t.descriptor_count && bad++ < REPORT_LIMIT)
            report_add(r, "TRI.DESCRIPTOR_INDEX", index, "%s: placement %u uses descriptor %u of %u (offset 0x%zx)", name, i, index, t.descriptor_count,
                       t.scenery_offset + (size_t)i * TRI_SCENERY_SIZE + 4);
    }
    if (bad > REPORT_LIMIT) report_add(r, "TRI.DESCRIPTOR_INDEX", bad, "%s: %d placements in all", name, bad);
}

/* ------------------------------------------------------------------ meshes */

static int blank4(const char *s) { return s[0] == 0 && s[1] == 0 && s[2] == 0 && s[3] == 0; }

enum { MAX_DIRECTORIES = 128 };

/* The bitmap directories a mesh's texture names are looked up in. */
typedef struct {
    Fsh dir[MAX_DIRECTORIES];
    int count;
} Directories;

static void add_directory(Directories *d, const Chunk *bitmap)
{
    if (bitmap != NULL && bitmap->kind == CHUNK_KIND_BITMAP && d->count < MAX_DIRECTORIES && chunk_bitmaps(bitmap, &d->dir[d->count]) == FSH_OK) d->count++;
}

static int in_directories(const Directories *d, const char name[4])
{
    uint32_t index;
    for (int i = 0; i < d->count; ++i)
        if (fsh_find(&d->dir[i], name, &index) == FSH_OK) return 1;
    return 0;
}

/* References the original CD itself ships without a texture (the EXE draws them untextured): object, name. */
static int shipped_without_texture(const char *file, const char *where, const char name[4])
{
    static const struct { const char *file; const char *where; const char *name; } known[] = {
        { "tr1_r01.fam", "scenery object 4", "cutp" },
        { "cy2_t01.fam", "scenery object 0", "left" }, { "cy2_t01.fam", "scenery object 0", "rite" },
        { "cy2_r01.fam", "scenery object 0", "left" }, { "cy2_r01.fam", "scenery object 0", "rite" },
        { "cy2_m01.fam", "scenery object 0", "left" }, { "cy2_m01.fam", "scenery object 0", "rite" },
    };
    for (size_t i = 0; i < sizeof known / sizeof known[0]; ++i)
        if (validate_has_extension(file, known[i].file) && strlen(file) == strlen(known[i].file) && strcmp(where, known[i].where) == 0 &&
            memcmp(name, known[i].name, 4) == 0) return 1;
    return 0;
}

/* Texture references of `mesh` against the directories; polygon 0's quirk flag. */
static void check_mesh(const Chunk *mesh_chunk, const Directories *dirs, const char *name, const char *where, Report *r)
{
    Mesh mesh;
    if (mesh_open(&mesh, mesh_chunk) != MESH_OK) {
        report_add(r, "FILE.READABLE", 0, "%s: %s: not a readable ORIP mesh", name, where);
        return;
    }
    for (uint32_t i = 0; dirs->count > 0 && i < mesh.texture_count; ++i) {
        MeshTexture t;
        if (mesh_texture(&mesh, i, &t) != MESH_OK || blank4(t.name)) continue;
        if (!in_directories(dirs, t.name) && !shipped_without_texture(validate_base_name(name), where, t.name))
            report_add(r, "MESH.TEXTURE_NAME", 0, "%s: %s: texture reference %u names '%.4s', which no bitmap directory it can see has", name, where, i, t.name);
    }
    MeshPolygon p;
    if (mesh.polygon_count > 0 && mesh_polygon(&mesh, 0, &p) == MESH_OK && (p.unknown_1 & 0x4))
        report_add(r, "MESH.POLYGON_0_FLAG", 0, "%s: %s: polygon 0 has bit 0x4 of its second byte set", name, where);
}

/* ------------------------------------------------------------------ track FAM */

static uint32_t banks_of(const Chunk *c)
{
    if (c->kind == CHUNK_KIND_HEADER) return c->count;
    return c->kind == CHUNK_KIND_ABSENT ? 0 : 1;
}

void validate_track_fam(const uint8_t *d, size_t n, const char *name, Report *r)
{
    if (n > FAM_BUFFER) report_add(r, "FAM.FILE_SIZE", (int64_t)n, "%s: %zu bytes", name, n);
    Chunk root;
    ChunkResult cr = chunk_open(&root, d, n);
    ChunkTotals totals;
    if (cr == CHUNK_OK) cr = chunk_survey(&root, &totals);
    if (cr != CHUNK_OK || root.kind != CHUNK_KIND_HEADER) {
        report_add(r, "FILE.READABLE", 0, "%s: %s (file is %zu bytes)", name, cr != CHUNK_OK ? chunk_result_str(cr) : "the root is not a wwww table", n);
        return;
    }
    if (root.count < 3) {
        report_add(r, "FAM.CHILDREN", root.count, "%s: %u children", name, root.count);
        return;
    }
    Chunk background, foreground, objects;
    chunk_child(&root, FAM_CHILD_TERRAIN, &background);
    chunk_child(&root, FAM_CHILD_SCENERY, &foreground);
    const uint32_t nb = banks_of(&background), nf = banks_of(&foreground);
    if (nb > limit_find("FAM.BANKS_BACKGROUND")->max) report_add(r, "FAM.BANKS_BACKGROUND", nb, "%s: child 0 holds %u banks", name, nb);
    if (nf > limit_find("FAM.BANKS_FOREGROUND")->max) report_add(r, "FAM.BANKS_FOREGROUND", nf, "%s: child 1 holds %u banks", name, nf);
    if (nb + nf > limit_find("FAM.BANKS_TOTAL")->max) report_add(r, "FAM.BANKS_TOTAL", nb + nf, "%s: %u + %u banks", name, nb, nf);
    if (root.count < 4 || chunk_child(&root, FAM_CHILD_OBJECTS, &objects) != CHUNK_OK || objects.kind != CHUNK_KIND_HEADER) return;
    if (objects.count > limit_find("FAM.OBJECTS")->max) report_add(r, "FAM.OBJECTS", objects.count, "%s: %u scenery objects", name, objects.count);
    Directories shared = {0};
    Chunk horizon;
    for (uint32_t i = 0; i < nb; ++i) {
        Chunk bank;
        if (background.kind == CHUNK_KIND_HEADER ? chunk_child(&background, i, &bank) == CHUNK_OK : (bank = background, 1)) add_directory(&shared, &bank);
    }
    for (uint32_t i = 0; i < nf; ++i) {
        Chunk bank;
        if (foreground.kind == CHUNK_KIND_HEADER ? chunk_child(&foreground, i, &bank) == CHUNK_OK : (bank = foreground, 1)) add_directory(&shared, &bank);
    }
    if (chunk_child(&root, FAM_CHILD_HORIZON, &horizon) == CHUNK_OK) add_directory(&shared, &horizon);
    for (uint32_t i = 0; i < objects.count; ++i) {
        Chunk pair, mesh, bitmaps;
        char where[40];
        if (chunk_child(&objects, i, &pair) != CHUNK_OK || pair.kind != CHUNK_KIND_HEADER || pair.count < 2) continue;
        if (chunk_child(&pair, 0, &mesh) != CHUNK_OK || mesh.kind != CHUNK_KIND_MESH) continue;
        snprintf(where, sizeof where, "scenery object %u", i);
        if (chunk_child(&pair, 1, &bitmaps) != CHUNK_OK) bitmaps.kind = CHUNK_KIND_ABSENT;
        Directories dirs = shared;
        add_directory(&dirs, &bitmaps);
        check_mesh(&mesh, &dirs, name, where, r);
    }
}

/* ------------------------------------------------------------------ car CFM */

static int name8_equal(const char *field, size_t field_len, const char *want)
{
    char stored[12] = {0};
    memcpy(stored, field, field_len < sizeof stored ? field_len : sizeof stored);
    return strncmp(stored, want, 8) == 0;
}

static int is_race_car(const char *base)
{
    static const char *const stems[] = { "tsupra", "ldiabl", "p911", "czr1", "f512tr", "dviper", "ansx", "mrx7" };
    char stem[16];
    size_t i = 0;
    for (; base[i] && base[i] != '.' && i < sizeof stem - 1; ++i) stem[i] = (char)tolower((unsigned char)base[i]);
    stem[i] = '\0';
    for (size_t k = 0; k < sizeof stems / sizeof stems[0]; ++k)
        if (strcmp(stem, stems[k]) == 0) return 1;
    return 0;
}

/* The car models the EXE can open: the race stems (004c5660, with the restricted 'm' prefix 004b1174), traffic
 * (004c56a0) and the cops. The CD's other CFMs are leftovers whose contents cannot matter. */
static int cfm_is_opened(const char *base)
{
    static const char *const stems[] = {
        "tsupra", "ldiabl", "p911", "czr1", "f512tr", "dviper", "ansx", "mrx7", "traffc",
        "wagon", "crx", "vandura", "bmw", "sunbird", "jetta", "rodeo", "axxess", "probe", "jeep", "lemans", "pickup",
        "copmust", "copcamaro", "copcaprice", "copvic"
    };
    char stem[24];
    size_t i = 0;
    for (; base[i] && base[i] != '.' && i < sizeof stem - 1; ++i) stem[i] = (char)tolower((unsigned char)base[i]);
    stem[i] = '\0';
    for (int prefixed = 0; prefixed < 2; ++prefixed)
        for (size_t k = 0; k < sizeof stems / sizeof stems[0]; ++k)
            if (strcmp(stem + prefixed, stems[k]) == 0 && (!prefixed || stem[0] == 'm')) return 1;
    return 0;
}

void validate_car_cfm(const uint8_t *d, size_t n, const char *name, Report *r)
{
    Chunk root;
    ChunkResult cr = chunk_open(&root, d, n);
    ChunkTotals totals;
    if (cr == CHUNK_OK) cr = chunk_survey(&root, &totals);
    if (cr != CHUNK_OK || root.kind != CHUNK_KIND_HEADER || root.count < 4) {
        report_add(r, "FILE.READABLE", 0, "%s: %s (file is %zu bytes)", name, cr != CHUNK_OK ? chunk_result_str(cr) : "a car model is a wwww table of 4 children", n);
        return;
    }
    if (!cfm_is_opened(validate_base_name(name))) return;
    Chunk high, high_bitmaps, low, low_bitmaps;
    chunk_child(&root, CFM_CHILD_MESH_HIGH, &high);
    chunk_child(&root, CFM_CHILD_BITMAPS_HIGH, &high_bitmaps);
    chunk_child(&root, CFM_CHILD_MESH_LOW, &low);
    chunk_child(&root, CFM_CHILD_BITMAPS_LOW, &low_bitmaps);
    if (high.kind != CHUNK_KIND_MESH || low.kind != CHUNK_KIND_MESH) {
        report_add(r, "FILE.READABLE", 0, "%s: children 0 and 2 must be ORIP meshes", name);
        return;
    }
    Directories high_dirs = {0}, low_dirs = {0};
    add_directory(&high_dirs, &high_bitmaps);
    add_directory(&low_dirs, &low_bitmaps);
    check_mesh(&high, &high_dirs, name, "high-detail mesh", r);
    check_mesh(&low, &low_dirs, name, "low-detail mesh", r);

    /* the bitmap directories must carry the palette entries 00457d40 looks up */
    const Chunk *dirs[2] = { &high_bitmaps, &low_bitmaps };
    const char *dir_names[2] = { "high-detail bitmaps", "low-detail bitmaps" };
    for (int k = 0; k < 2; ++k) {
        Fsh fsh;
        uint32_t index;
        if (dirs[k]->kind != CHUNK_KIND_BITMAP || chunk_bitmaps(dirs[k], &fsh) != FSH_OK) {
            report_add(r, "CFM.REQUIRED_NAME", 0, "%s: %s: no bitmap directory", name, dir_names[k]);
            continue;
        }
        if (fsh_find(&fsh, "!PAL", &index) != FSH_OK) report_add(r, "CFM.REQUIRED_NAME", 0, "%s: %s: no '!PAL' entry (004b1160)", name, dir_names[k]);
        if (k == 0 && fsh_find(&fsh, "!xxx", &index) != FSH_OK) report_add(r, "CFM.REQUIRED_NAME", 0, "%s: %s: no '!xxx' entry (004b1180)", name, dir_names[k]);
    }

    Mesh mesh;
    if (mesh_open(&mesh, &high) != MESH_OK) return;
    /* section B: inside and surface at an index below 8, outside and convex tolerated absent */
    static const char *const elements[] = { "inside", "surface" };
    for (int e = 0; e < 2; ++e) {
        int found = -1;
        for (uint32_t i = 0; i < mesh.section_b_count && found < 0; ++i) {
            MeshSectionB b;
            if (mesh_section_b(&mesh, i, &b) == MESH_OK && name8_equal(b.name, sizeof b.name, elements[e])) found = (int)i;
        }
        if (found < 0) report_add(r, "CFM.REQUIRED_NAME", 0, "%s: high-detail mesh: no element '%s' (004b1188)", name, elements[e]);
        else if (found > limit_find("CFM.ELEMENT_INDEX")->max)
            report_add(r, "CFM.ELEMENT_INDEX", found, "%s: element '%s' is number %d", name, elements[e], found);
    }
    const char *base = validate_base_name(name);
    if (!is_race_car(base)) return;
    /* race cars: the parts the player's car needs (section A) and the tyre labels (texture references) */
    static const char *const parts[] = { "left_turn", "right_turn" };
    for (int p = 0; p < 2; ++p) {
        int found = 0;
        for (uint32_t i = 0; i < mesh.section_a_count && !found; ++i) {
            MeshSectionA a;
            found = mesh_section_a(&mesh, i, &a) == MESH_OK && name8_equal(a.name, sizeof a.name, parts[p]);
        }
        if (!found) report_add(r, "CFM.REQUIRED_NAME", 0, "%s: high-detail mesh: no part '%s' (004b11c0)", name, parts[p]);
    }
    static const char *const tyres[] = { "frntires", "rertires", "tire1", "tire2", "tire3", "tire4" };
    for (int t = 0; t < 6; ++t) {
        int found = 0;
        for (uint32_t i = 0; i < mesh.texture_count && !found; ++i) {
            MeshTexture tex;
            found = mesh_texture(&mesh, i, &tex) == MESH_OK && name8_equal(tex.label, sizeof tex.label, tyres[t]);
        }
        if (!found) report_add(r, "CFM.REQUIRED_NAME", 0, "%s: high-detail mesh: no texture label '%s' (004c5778)", name, tyres[t]);
    }
}
