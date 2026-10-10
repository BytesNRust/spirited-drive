/* validate tests: every limit in core/formats/limits.c is hit by a synthetic file that exceeds it by one unit
 * (and, where the limit is a number, a file exactly at it passes). The last check walks the limit table and
 * fails for any id no test reached, so a new limit cannot be added without a test. */
#include "formats/exchange.h"
#include "formats/limits.h"
#include "formats/pbs.h"
#include "formats/qfs.h"
#include "formats/tri.h"
#include "formats/validate.h"
#include "test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int hit[64];

static void put_u32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i)); }

typedef struct { uint8_t *data; size_t size; } Blob;

static Blob blob_new(size_t size) { Blob b = { calloc(1, size ? size : 1), size }; return b; }

/* ---- running validate */

static size_t count_id(const Report *r, const char *id)
{
    size_t n = 0;
    for (size_t i = 0; i < r->count; ++i) n += strcmp(r->item[i].limit->id, id) == 0;
    return n;
}

static void mark(const char *id)
{
    for (size_t i = 0; i < limit_count; ++i)
        if (strcmp(limit_table[i].id, id) == 0) hit[i] = 1;
}

/* The file must produce `id` (and only findings of it); `value` is checked when >= 0. */
static void expect(const char *id, const Blob *b, const char *name, int64_t value)
{
    Report r;
    report_init(&r);
    validate_file(b->data, b->size, name, &r);
    const size_t n = count_id(&r, id);
    if (n == 0 || r.count != n) {
        fprintf(stderr, "%s on %s: expected only %s, got %zu findings:\n", id, name, id, r.count);
        report_print(&r, VALIDATE_COMPATIBLE, stderr);
    }
    CHECK(n > 0);
    CHECK_EQ_INT(r.count, n);
    if (n > 0 && value >= 0) CHECK_EQ_INT(r.item[0].value, value);
    if (n > 0) mark(id);
    report_free(&r);
}

/* The file must produce `id`, possibly among others. */
static void expect_has(const char *id, const Blob *b, const char *name, int64_t value)
{
    Report r;
    report_init(&r);
    validate_file(b->data, b->size, name, &r);
    size_t found = 0;
    for (size_t i = 0; i < r.count; ++i)
        if (strcmp(r.item[i].limit->id, id) == 0 && (value < 0 || r.item[i].value == value)) ++found;
    if (!found) { fprintf(stderr, "%s on %s missing:\n", id, name); report_print(&r, VALIDATE_COMPATIBLE, stderr); }
    CHECK(found > 0);
    if (found) mark(id);
    report_free(&r);
}

static void expect_clean(const Blob *b, const char *name)
{
    Report r;
    report_init(&r);
    validate_file(b->data, b->size, name, &r);
    if (r.count) {
        fprintf(stderr, "%s should be clean:\n", name);
        report_print(&r, VALIDATE_COMPATIBLE, stderr);
    }
    CHECK_EQ_INT(r.count, 0);
    report_free(&r);
}

/* ---- builders */

enum { SCENERY_BYTES = TRI_SCENERY_MAX * TRI_SCENERY_SIZE };

typedef struct { uint32_t segments, circuit, descriptors, placements, slots, descriptor_of_placement; int terminated; } TriSpec;

static Blob make_tri(const TriSpec *s)
{
    static uint8_t header[TRI_HEADER_SIZE], scenery[SCENERY_BYTES];
    static uint8_t descriptors[8 * TRI_DESCRIPTOR_SIZE];
    uint8_t *terrain = calloc(s->segments ? s->segments : 1, TRI_SEGMENT_SIZE);
    memset(header, 0, sizeof header);
    header[0] = TRI_VERSION;
    header[4] = (uint8_t)s->circuit;
    header[5] = (uint8_t)(s->circuit >> 8);
    put_u32(header + 0x16218, s->slots);
    memset(scenery, 0, sizeof scenery);
    for (uint32_t i = 0; i < TRI_SCENERY_MAX; ++i) {
        if (i < s->placements) { put_u32(scenery + 16 * i, i); scenery[16 * i + 4] = (uint8_t)s->descriptor_of_placement; }
        else put_u32(scenery + 16 * i, s->terminated ? 0xffffffffu : 1);
    }
    for (uint32_t i = 0; i < s->segments; ++i) {
        memcpy(terrain + i * TRI_SEGMENT_SIZE, "TRKD", 4);
        put_u32(terrain + i * TRI_SEGMENT_SIZE + 4, TRI_SEGMENT_SIZE - 12);
    }
    const TriParts parts = { header, descriptors, s->descriptors, scenery, terrain, s->segments, NULL, 0 };
    Blob b = blob_new(tri_write_size(&parts));
    size_t len = 0;
    tri_write(&parts, b.data, b.size, &len);
    free(terrain);
    return b;
}

static TriSpec good_tri(void) { TriSpec s = { 10, 0, 4, 3, 100, 2, 1 }; return s; }

/* A wwww table of the given children (NULL = absent). */
static Blob make_wwww(const Blob *child, uint32_t n)
{
    size_t total = 8 + 4 * (size_t)n;
    for (uint32_t i = 0; i < n; ++i) total += child[i].data ? child[i].size : 0;
    Blob b = blob_new(total);
    memcpy(b.data, "wwww", 4);
    put_u32(b.data + 4, n);
    size_t at = 8 + 4 * (size_t)n;
    for (uint32_t i = 0; i < n; ++i) {
        if (!child[i].data) continue;
        put_u32(b.data + 8 + 4 * i, (uint32_t)at);
        memcpy(b.data + at, child[i].data, child[i].size);
        at += child[i].size;
    }
    return b;
}

/* SHPI with n 1x1 bitmaps named by names[i] (4 chars), one of the given type. */
static Blob make_shpi(const char *const *names, uint32_t n, uint8_t type)
{
    const size_t dir = 16 + 8 * (size_t)n, entry = 17;
    Blob b = blob_new(dir + entry * n);
    memcpy(b.data, "SHPI", 4);
    put_u32(b.data + 4, (uint32_t)b.size);
    put_u32(b.data + 8, n);
    memcpy(b.data + 12, "GIMX", 4);
    for (uint32_t i = 0; i < n; ++i) {
        memcpy(b.data + 16 + 8 * i, names[i], 4);
        put_u32(b.data + 20 + 8 * i, (uint32_t)(dir + entry * i));
        uint8_t *e = b.data + dir + entry * i;
        e[0] = type;
        e[1] = 17;
        e[4] = 1;
        e[6] = 1;
    }
    return b;
}

typedef struct {
    const char *label[8], *texture[8];       /* texture references: 8-char label, 4-char FSH name (NULL = none) */
    uint32_t textures;
    const char *part[4];                     /* section A names */
    uint32_t parts;
    const char *group[16];                   /* section B names */
    uint32_t groups;
    uint8_t polygon0_flag;                   /* byte 1 of polygon 0 */
} OripSpec;

static void put_name(uint8_t *p, const char *s, size_t cap) { if (s) memcpy(p, s, strlen(s) < cap ? strlen(s) : cap); }

static Blob make_orip(const OripSpec *s)
{
    const uint32_t vo = 0x70, po = vo + 12, to = po + 12, ao = to + 20 * s->textures, bo = ao + 20 * s->parts, mo = bo + 28 * s->groups;
    Blob b = blob_new(mo + 16);
    memcpy(b.data, "ORIP", 4);
    put_u32(b.data + 4, (uint32_t)b.size);
    put_u32(b.data + 0x10, 1);
    put_u32(b.data + 0x18, vo);
    put_u32(b.data + 0x20, vo);                   /* no uvs, but the offset still has to clear the header */
    put_u32(b.data + 0x24, 1);
    put_u32(b.data + 0x28, po);
    memcpy(b.data + 0x2c, "_TEST", 5);
    put_u32(b.data + 0x38, s->textures);
    put_u32(b.data + 0x3c, to);
    put_u32(b.data + 0x40, s->parts);
    put_u32(b.data + 0x44, ao);
    put_u32(b.data + 0x48, s->groups);
    put_u32(b.data + 0x4c, bo);
    put_u32(b.data + 0x50, mo);
    put_u32(b.data + 0x54, 0);
    put_u32(b.data + 0x58, mo);
    put_u32(b.data + 0x5c, 0);
    put_u32(b.data + 0x60, mo);
    put_u32(b.data + 0x68, po);
    b.data[po] = 3;                               /* one triangle */
    b.data[po + 1] = s->polygon0_flag;
    for (uint32_t i = 0; i < s->textures; ++i) {
        put_name(b.data + to + 20 * i, s->label[i], 8);
        put_name(b.data + to + 20 * i + 8, s->texture[i], 4);
    }
    for (uint32_t i = 0; i < s->parts; ++i) put_name(b.data + ao + 20 * i, s->part[i], 8);
    for (uint32_t i = 0; i < s->groups; ++i) put_name(b.data + bo + 28 * i, s->group[i], 12);
    return b;
}

static OripSpec car_orip(void)
{
    OripSpec s = {{ "frntires", "rertires", "tire1", "tire2", "tire3", "tire4", "" , "" }, { "tyr0", "tyr0", "tyr1", "tyr1", "tyr1", "tyr1", "topv", NULL }, 7,
                  { "left_turn", "right_turn" }, 2, { "NON-SORT", "inside", "surface", "outside" }, 4, 0 };
    return s;
}

static Blob make_cfm(const OripSpec *high, const char *const *high_names, uint32_t nh, const char *const *low_names, uint32_t nl)
{
    OripSpec low = {{ "" }, { "frnt" }, 1, { NULL }, 0, { "inside", "surface" }, 2, 0};
    Blob parts[4] = { make_orip(high), make_shpi(high_names, nh, 0x7b), make_orip(&low), make_shpi(low_names, nl, 0x7b) };
    Blob cfm = make_wwww(parts, 4);
    for (int i = 0; i < 4; ++i) free(parts[i].data);
    return cfm;
}

static void test_files(void)
{
    /* names, lengths, fixed sizes */
    Blob any = blob_new(10);
    char longest[160];
    memset(longest, 'a', 123);
    strcpy(longest + 123, ".dat");                    /* 127 characters */
    expect_clean(&any, longest);
    memset(longest, 'a', 124);
    strcpy(longest + 124, ".dat");                    /* 128 */
    expect("FILE.NAME_LENGTH", &any, longest, 128);
    expect("FILE.FIXED_SIZE", &any, "PATHS.DAT", 10);
    Blob paths = blob_new(0x5f0), config = blob_new(24402), save = blob_new(0x5b6);
    expect_clean(&paths, "paths.dat");
    expect_clean(&config, "gamedata/config/CONFIG.DAT");
    expect_clean(&save, "GAME1.SAV");
    expect("FILE.FIXED_SIZE", &any, "config.dat", 10);
    expect("FILE.FIXED_SIZE", &any, "x.sav", 10);
    free(any.data); free(paths.data); free(config.data); free(save.data);

    /* QFS */
    uint8_t bad_codec[8] = { 0x20, 0xfb, 0, 0, 1, 0xfd, 'A', 0 };
    Blob q = { bad_codec, sizeof bad_codec };
    expect("QFS.PACK_CODE", &q, "x.qfs", 0x20);
    uint8_t ok_codec[8] = { 0x10, 0xfb, 0, 0, 1, 0xfd, 'A', 0 };
    Blob good = { ok_codec, 7 };
    expect_clean(&good, "x.qfs");
    Blob fat = blob_new(0x400 + 1 + 6);                 /* one unpacked byte, 0x400 over: the limit is exceeded by one */
    memcpy(fat.data, ok_codec, 6);
    expect("QFS.INPLACE_SIZE", &fat, "x.qfs", 6 + 0x400);
    Blob atmax = blob_new(0x400 + 1);
    memcpy(atmax.data, ok_codec, 6);
    expect_clean(&atmax, "x.qfs");
    free(fat.data);
    free(atmax.data);
    {   /* a compressible head, then incompressible data: rule 2 */
        enum { HEAD = 100000, TAIL = 140000 };
        uint8_t *plain = calloc(HEAD + TAIL, 1);
        uint32_t x = 1;
        for (int i = 0; i < TAIL; ++i) { x = x * 1664525u + 1013904223u; plain[HEAD + i] = (uint8_t)(x >> 24); }
        Blob packed = blob_new(qfs_compress_bound(HEAD + TAIL));
        size_t len = 0;
        CHECK_EQ_INT(qfs_compress(plain, HEAD + TAIL, packed.data, packed.size, &len), QFS_OK);
        packed.size = len;
        expect("QFS.INPLACE_ORDER", &packed, "x.qfs", -1);
        free(packed.data);
        free(plain);
    }

    /* damaged QFS */
    Blob cut = { ok_codec, 5 };
    (void)good;
    expect("FILE.READABLE", &cut, "x.qfs", -1);
}

static void test_shpi(void)
{
    const char *names[] = { "aaaa", "bbbb" };
    const uint8_t good_types[] = { 0x7a, 0x7b, 0x78, 0x7e, 0x7f, 0x7d, 0x68, 0x6f, 0x22, 0x24 };
    for (size_t i = 0; i < sizeof good_types; ++i) {
        Blob b = make_shpi(names, 2, good_types[i]);
        expect_clean(&b, "x.fsh");
        free(b.data);
    }
    const uint8_t bad_types[] = { 0x79, 0x7c, 0x67, 0x70, 0x00, 0x55, 0x23 };
    for (size_t i = 0; i < sizeof bad_types; ++i) {
        Blob b = make_shpi(names, 2, bad_types[i]);
        expect("SHPI.SHAPE_TYPE", &b, "x.fsh", bad_types[i]);
        free(b.data);
    }
}

static void test_pbs_pdn(void)
{
    Blob pbs = blob_new(PBS_SIZE);
    for (int i = 0; i < PBS_SIZE; ++i) pbs.data[i] = (uint8_t)(i * 3);
    pbs_seal(pbs.data);
    expect_clean(&pbs, "CZR1.PBS");
    pbs.data[10] ^= 1;
    expect("PBS.CHECKSUM", &pbs, "CZR1.PBS", -1);
    Blob shortpbs = blob_new(PBS_SIZE - 1);
    expect("PBS.SIZE", &shortpbs, "CZR1.PBS", PBS_SIZE - 1);
    Blob longpbs = blob_new(PBS_SIZE + 1);
    expect("PBS.SIZE", &longpbs, "CZR1.PBS", PBS_SIZE + 1);
    Blob pdn = blob_new(460);
    expect_clean(&pdn, "CZR1.PDN");
    Blob pdn2 = blob_new(461);
    expect("PDN.SIZE", &pdn2, "CZR1.PDN", 461);
    Blob pdn3 = blob_new(459);
    expect("PDN.SIZE", &pdn3, "CZR1.PDN", 459);
    free(pbs.data); free(shortpbs.data); free(longpbs.data); free(pdn.data); free(pdn2.data); free(pdn3.data);
}

static void test_tri(void)
{
    TriSpec s = good_tri();
    Blob b = make_tri(&s);
    expect_clean(&b, "TR1.TRI");
    free(b.data);

    s = good_tri(); s.segments = 599;
    b = make_tri(&s); expect_clean(&b, "TR1.TRI"); free(b.data);
    s.segments = 600;
    b = make_tri(&s); expect("TRI.SEGMENTS", &b, "TR1.TRI", 600); free(b.data);

    s = good_tri(); s.circuit = 128;
    b = make_tri(&s); expect_clean(&b, "TR1.TRI"); free(b.data);
    s.circuit = 129;
    b = make_tri(&s); expect("TRI.CIRCUIT_POW2", &b, "TR1.TRI", 129 * 4); free(b.data);

    s = good_tri(); s.placements = 999; s.slots = 999;
    b = make_tri(&s); expect_clean(&b, "TR1.TRI"); free(b.data);
    s.placements = 1000; s.slots = 1000;                     /* 1000 records fill the area: no terminator */
    b = make_tri(&s); expect("TRI.SCENERY_RECORDS", &b, "TR1.TRI", 1000); free(b.data);
    s = good_tri(); s.terminated = 0; s.slots = 1000;        /* junk records, no -1 anywhere */
    b = make_tri(&s); expect("TRI.SCENERY_RECORDS", &b, "TR1.TRI", 1000); free(b.data);

    s = good_tri(); s.descriptors = 3; s.descriptor_of_placement = 2;
    b = make_tri(&s); expect_clean(&b, "TR1.TRI"); free(b.data);
    s.descriptor_of_placement = 3;
    b = make_tri(&s); expect("TRI.DESCRIPTOR_INDEX", &b, "TR1.TRI", 3); free(b.data);

    s = good_tri(); s.slots = 3;
    b = make_tri(&s); expect_clean(&b, "TR1.TRI"); free(b.data);
    s.slots = 2;
    b = make_tri(&s); expect("TRI.SCENERY_SLOTS", &b, "TR1.TRI", 2); free(b.data);

    Blob cut = { make_tri(&(TriSpec){ 10, 0, 4, 3, 100, 2, 1 }).data, 3000 };
    expect("FILE.READABLE", &cut, "TR1.TRI", -1);
    free(cut.data);
}

/* ---- track FAM */

static Blob make_fam(uint32_t background, uint32_t foreground, uint32_t objects, size_t pad, uint32_t root_children)
{
    const char *names[] = { "aaaa" };
    Blob shpi = make_shpi(names, 1, 0x7b);
    Blob *banks = calloc(background + foreground + 1, sizeof *banks);
    for (uint32_t i = 0; i < background + foreground; ++i) banks[i] = shpi;
    Blob bg = make_wwww(banks, background), fg = make_wwww(banks + background, foreground);
    OripSpec os = { {""}, {"aaaa"}, 1, {NULL}, 0, {NULL}, 0, 0 };
    Blob orip = make_orip(&os);
    Blob pair_children[2] = { orip, shpi };
    Blob pair = make_wwww(pair_children, 2);
    Blob *objs = calloc(objects + 1, sizeof *objs);
    for (uint32_t i = 0; i < objects; ++i) objs[i] = pair;
    Blob objects_tree = make_wwww(objs, objects);
    Blob filler = blob_new(pad ? pad : 8);
    memcpy(filler.data, "SHPI", 4);
    put_u32(filler.data + 4, (uint32_t)filler.size);
    put_u32(filler.data + 8, 0);
    Blob root_parts[4] = { bg, fg, pad ? filler : shpi, objects_tree };
    Blob fam = make_wwww(root_parts, root_children);
    free(banks); free(objs); free(bg.data); free(fg.data); free(pair.data); free(objects_tree.data); free(filler.data); free(orip.data); free(shpi.data);
    return fam;
}

static void test_fam(void)
{
    Blob f = make_fam(1, 1, 1, 0, 4);
    expect_clean(&f, "TR1_001.FAM");
    free(f.data);

    f = make_fam(1, 1, 1, 0, 2);
    expect("FAM.CHILDREN", &f, "TR1_001.FAM", 2);
    free(f.data);

    f = make_fam(1, 1, 30, 0, 4); expect_clean(&f, "TR1_001.FAM"); free(f.data);
    f = make_fam(1, 1, 31, 0, 4); expect("FAM.OBJECTS", &f, "TR1_001.FAM", 31); free(f.data);

    f = make_fam(48, 1, 1, 0, 4); expect_clean(&f, "TR1_001.FAM"); free(f.data);
    f = make_fam(49, 1, 1, 0, 4); expect("FAM.BANKS_BACKGROUND", &f, "TR1_001.FAM", 49); free(f.data);
    f = make_fam(1, 63, 1, 0, 4); expect_clean(&f, "TR1_001.FAM"); free(f.data);
    f = make_fam(1, 64, 1, 0, 4); expect("FAM.BANKS_FOREGROUND", &f, "TR1_001.FAM", 64); free(f.data);
    f = make_fam(48, 48, 1, 0, 4); expect_clean(&f, "TR1_001.FAM"); free(f.data);
    f = make_fam(48, 49, 1, 0, 4); expect("FAM.BANKS_TOTAL", &f, "TR1_001.FAM", 97); free(f.data);

    f = make_fam(1, 1, 1, 0x162400 - 700, 4);
    CHECK(f.size <= 0x162400);
    const size_t base = f.size;
    free(f.data);
    f = make_fam(1, 1, 1, 0x162400 - 700 + (0x162400 - base), 4);
    CHECK_EQ_INT(f.size, 0x162400);
    expect_clean(&f, "TR1_001.FAM");
    free(f.data);
    f = make_fam(1, 1, 1, 0x162400 - 700 + (0x162400 - base) + 1, 4);
    expect("FAM.FILE_SIZE", &f, "TR1_001.FAM", 0x162401);
    free(f.data);

    Blob cut = { make_fam(1, 1, 1, 0, 4).data, 20 };
    expect("FILE.READABLE", &cut, "TR1_001.FAM", -1);
    free(cut.data);
}

/* ---- meshes and cars */

static void test_mesh_textures(void)
{
    /* a scenery object whose ORIP names 'miss' while its directory has 'aaaa' and the track banks have 'bbbb' */
    OripSpec os = { {"", "", ""}, {"aaaa", "bbbb", "miss"}, 3, {NULL}, 0, {NULL}, 0, 0 };
    const char *obj_names[] = { "aaaa" }, *bank_names[] = { "bbbb" };
    Blob orip = make_orip(&os), shpi = make_shpi(obj_names, 1, 0x7b), bank = make_shpi(bank_names, 1, 0x7b);
    Blob pair_children[2] = { orip, shpi };
    Blob pair = make_wwww(pair_children, 2);
    Blob objects = make_wwww(&pair, 1);
    Blob bg = make_wwww(&bank, 1), fg = make_wwww(&bank, 1);
    Blob horizon = make_shpi(bank_names, 1, 0x7b);
    Blob parts[4] = { bg, fg, horizon, objects };
    Blob fam = make_wwww(parts, 4);
    expect("MESH.TEXTURE_NAME", &fam, "TR1_001.FAM", -1);
    free(fam.data);

    os.texture[2] = "aaaa";                                   /* now every name resolves */
    free(orip.data);
    orip = make_orip(&os);
    free(pair.data); free(objects.data);
    pair_children[0] = orip;
    pair = make_wwww(pair_children, 2);
    objects = make_wwww(&pair, 1);
    parts[3] = objects;
    fam = make_wwww(parts, 4);
    expect_clean(&fam, "TR1_001.FAM");
    free(fam.data);

    os.polygon0_flag = 4;                                     /* bit 0x4 of polygon 0's second byte */
    free(orip.data); free(pair.data); free(objects.data);
    orip = make_orip(&os);
    pair_children[0] = orip;
    pair = make_wwww(pair_children, 2);
    objects = make_wwww(&pair, 1);
    parts[3] = objects;
    fam = make_wwww(parts, 4);
    expect("MESH.POLYGON_0_FLAG", &fam, "TR1_001.FAM", -1);
    free(fam.data); free(orip.data); free(pair.data); free(objects.data); free(bg.data); free(fg.data); free(horizon.data);
    free(shpi.data); free(bank.data);
}

static void test_cfm(void)
{
    const char *high_names[] = { "!PAL", "!xxx", "tyr0", "tyr1", "topv" }, *low_names[] = { "!PAL", "frnt" };
    OripSpec os = car_orip();
    Blob cfm = make_cfm(&os, high_names, 5, low_names, 2);
    expect_clean(&cfm, "DVIPER.CFM");
    expect_clean(&cfm, "MTSUPRA.CFM");                      /* the restricted copy */
    free(cfm.data);

    /* an unreferenced model is not looked at */
    os.texture[6] = "miss";
    cfm = make_cfm(&os, high_names, 5, low_names, 2);
    expect("MESH.TEXTURE_NAME", &cfm, "DVIPER.CFM", -1);
    {
        Report r;
        report_init(&r);
        validate_file(cfm.data, cfm.size, "LDIABLO.CFM", &r);
        CHECK_EQ_INT(r.count, 0);
        report_free(&r);
    }
    free(cfm.data);
    os = car_orip();

    /* the bitmap directories of the car */
    const char *no_pal[] = { "tyr0", "tyr1", "topv" }, *no_xxx[] = { "!PAL", "tyr0", "tyr1", "topv" };
    cfm = make_cfm(&os, no_pal, 3, low_names, 2);
    expect("CFM.REQUIRED_NAME", &cfm, "DVIPER.CFM", -1);
    free(cfm.data);
    cfm = make_cfm(&os, no_xxx, 4, low_names, 2);
    expect("CFM.REQUIRED_NAME", &cfm, "DVIPER.CFM", -1);
    free(cfm.data);

    /* elements: inside and surface below index 8; missing ones hang the loader */
    os = car_orip();
    os.groups = 10;
    for (int i = 0; i < 10; ++i) os.group[i] = "filler";
    os.group[7] = "inside";
    os.group[8] = "surface";
    cfm = make_cfm(&os, high_names, 5, low_names, 2);
    expect("CFM.ELEMENT_INDEX", &cfm, "DVIPER.CFM", 8);
    free(cfm.data);
    os.group[8] = "filler";
    os.group[6] = "surface";
    cfm = make_cfm(&os, high_names, 5, low_names, 2);
    expect_clean(&cfm, "DVIPER.CFM");                         /* index 7 is the last that works */
    free(cfm.data);
    os.group[6] = "filler";
    cfm = make_cfm(&os, high_names, 5, low_names, 2);
    expect("CFM.REQUIRED_NAME", &cfm, "DVIPER.CFM", -1);     /* surface is gone */
    free(cfm.data);

    /* a race car needs its tyres and turn signals; a traffic car does not */
    os = car_orip();
    os.textures = 6;
    os.label[5] = "";                                        /* tire4 missing */
    os.texture[5] = "tyr1";
    cfm = make_cfm(&os, high_names, 5, low_names, 2);
    expect("CFM.REQUIRED_NAME", &cfm, "DVIPER.CFM", -1);
    expect_clean(&cfm, "Wagon.CFM");
    free(cfm.data);
    os = car_orip();
    os.parts = 1;                                            /* right_turn missing */
    cfm = make_cfm(&os, high_names, 5, low_names, 2);
    expect("CFM.REQUIRED_NAME", &cfm, "TSupra.CFM", -1);
    free(cfm.data);
}

/* ---- sound banks */

static Blob make_bank(uint32_t length, int slot)
{
    Blob b = blob_new(0x400);
    put_u32(b.data + 4 * slot, 0x200);
    uint8_t *h = b.data + 0x200;
    put_u32(h + 4, 0x228);
    memcpy(h + 0x28, "EACS", 4);
    put_u32(h + 0x2c, 16000);
    h[0x30] = 1;
    h[0x31] = 1;
    put_u32(h + 0x3c, length);
    put_u32(h + 0x40, 0x300);
    return b;
}

static void test_bnk(void)
{
    Blob b = make_bank(0x100, 65);                            /* 0x300 + 0x100 = the end of the file */
    expect_clean(&b, "OSUPMB.BNK");
    free(b.data);
    b = make_bank(0x101, 65);
    expect("BNK.DATA_BOUNDS", &b, "OSUPMB.BNK", -1);
    Report r;
    report_init(&r);
    validate_file(b.data, b.size, "TESTBANK.BNK", &r);       /* never opened by the EXE: whatever it holds */
    CHECK_EQ_INT(r.count, 0);
    report_free(&r);
    free(b.data);

    /* banks of different load groups must not share a sound number */
    FileSet set;
    fileset_init(&set);
    Blob player = make_bank(4, 1), opponent = make_bank(4, 65), clash = make_bank(4, 1);
    fileset_add(&set, "SIMDATA/SOUNDBNK/VIPERSW.BNK", player.data, player.size);
    fileset_add(&set, "SIMDATA/SOUNDBNK/ODIAMB.BNK", opponent.data, opponent.size);
    report_init(&r);
    validate_tree(&set, &r);
    CHECK_EQ_INT(r.count, 0);
    report_free(&r);
    fileset_add(&set, "SIMDATA/SOUNDBNK/ORX7MB.BNK", clash.data, clash.size);
    report_init(&r);
    validate_tree(&set, &r);
    CHECK_EQ_INT(count_id(&r, "BNK.SOUND_NUMBERS"), 1);
    if (r.count) mark("BNK.SOUND_NUMBERS");
    report_free(&r);
    fileset_free(&set);
    free(player.data); free(opponent.data); free(clash.data);
}

/* ---- INFO text */

static Blob text_blob(const char *s) { Blob b = blob_new(strlen(s)); memcpy(b.data, s, strlen(s)); return b; }

static void test_info(void)
{
    char text[4096];
    size_t n = 0;
    for (int i = 1; i <= 29; ++i) {
        n += (size_t)snprintf(text + n, sizeof text - n, "# comment %d\r\n", i);
        if (i == 9 || i == 10) n += (size_t)snprintf(text + n, sizeof text - n, "0 1 31\r\n\r\n");
        else n += (size_t)snprintf(text + n, sizeof text - n, "%d\r\n\r\n", i * 10);
    }
    Blob b = text_blob(text);
    expect_clean(&b, "AL1INFO");
    free(b.data);

    char *hit = strstr(text, "0 1 31");
    memcpy(hit, "0 1 32", 6);
    b = text_blob(text);
    expect("INFO.COLOUR", &b, "AL1INFO", 32);
    free(b.data);
    memcpy(hit, "0 -1 3", 6);
    b = text_blob(text);
    expect_has("INFO.COLOUR", &b, "AL1INFO", -1);
    free(b.data);
    memcpy(hit, "0 1 31", 6);

    /* 28 data lines: the last one would be repeated */
    char *last = strstr(text, "# comment 29");
    *last = '\0';
    b = text_blob(text);
    expect("INFO.DATA_LINES", &b, "AL1INFO", 28);
    free(b.data);

    /* a line of 126 bytes with its terminator is read whole; 127 is split */
    char line[200];
    memset(line, '1', 124);
    strcpy(line + 124, "\r\n");
    b = text_blob(line);
    Report r;
    report_init(&r);
    validate_file(b.data, b.size, "AL1INFO", &r);
    CHECK_EQ_INT(count_id(&r, "INFO.LINE_LENGTH"), 0);
    report_free(&r);
    free(b.data);
    memset(line, '1', 125);
    strcpy(line + 125, "\r\n");
    b = text_blob(line);
    expect_has("INFO.LINE_LENGTH", &b, "AL1INFO", 127);
    free(b.data);
    hit[0] = hit[0];

    /* a loadable track without a built-in row needs its INFO file */
    FileSet set;
    fileset_init(&set);
    TriSpec s = good_tri();
    Blob tri = make_tri(&s);
    fileset_add(&set, "SIMDATA/MISC/AL1.TRI", tri.data, tri.size);
    report_init(&r);
    validate_tree(&set, &r);
    CHECK_EQ_INT(count_id(&r, "INFO.REQUIRED"), 1);
    if (r.count) mark("INFO.REQUIRED");
    report_free(&r);
    fileset_add(&set, "SIMDATA/MISC/AL1INFO", (const uint8_t *)"x", 1);
    report_init(&r);
    validate_tree(&set, &r);
    CHECK_EQ_INT(count_id(&r, "INFO.REQUIRED"), 0);
    report_free(&r);
    fileset_free(&set);
    fileset_init(&set);
    fileset_add(&set, "SIMDATA/MISC/TR1.TRI", tri.data, tri.size);       /* built-in row */
    fileset_add(&set, "SIMDATA/MISC/AL22.TRI", tri.data, tri.size);      /* never loaded */
    report_init(&r);
    validate_tree(&set, &r);
    CHECK_EQ_INT(count_id(&r, "INFO.REQUIRED"), 0);
    report_free(&r);
    fileset_free(&set);
    free(tri.data);
}

static void test_report(void)
{
    Report r;
    report_init(&r);
    report_add(&r, "TRI.SEGMENTS", 600, "TR1.TRI: 600 segments");
    report_add(&r, "MESH.POLYGON_0_FLAG", 0, "x.cfm: polygon 0");
    report_add(&r, "NO.SUCH.ID", 1, "ignored");
    CHECK_EQ_INT(r.count, 2);
    CHECK_EQ_INT(report_fatal_count(&r), 1);                  /* the polygon quirk is a warning in both modes */
    char *text = NULL;
    size_t len = 0;
    FILE *f = open_memstream(&text, &len);
    report_print(&r, VALIDATE_COMPATIBLE, f);
    fclose(f);
    CHECK(strstr(text, "error TRI.SEGMENTS: TR1.TRI: 600 segments") != NULL);
    CHECK(strstr(text, "maximum 599") != NULL);
    CHECK(strstr(text, "0043e4e0") != NULL);                  /* the limit cites its address */
    CHECK(strstr(text, "warning MESH.POLYGON_0_FLAG") != NULL);
    free(text);
    f = open_memstream(&text, &len);
    report_print(&r, VALIDATE_PORT_ONLY, f);
    fclose(f);
    CHECK(strstr(text, "error ") == NULL);                    /* port-only: warnings only */
    free(text);
    report_free(&r);

    /* every limit has an address and a description (AC5) */
    for (size_t i = 0; i < limit_count; ++i) {
        CHECK(limit_table[i].exe != NULL && strlen(limit_table[i].exe) >= 4);
        CHECK(limit_table[i].what != NULL && strlen(limit_table[i].what) > 10);
        CHECK(strchr(limit_table[i].id, '.') != NULL);
    }
}

int main(void)
{
    test_files();
    test_shpi();
    test_pbs_pdn();
    test_tri();
    test_fam();
    test_mesh_textures();
    test_cfm();
    test_bnk();
    test_info();
    test_report();
    for (size_t i = 0; i < limit_count; ++i) {
        /* MOD.CARSPECS_WRITABLE needs a real directory: tests/studio_cli.sh covers it */
        if (!hit[i] && strcmp(limit_table[i].id, "MOD.CARSPECS_WRITABLE") != 0) {
            fprintf(stderr, "no test reaches %s\n", limit_table[i].id);
            CHECK(0);
        }
    }
    return TEST_REPORT();
}
