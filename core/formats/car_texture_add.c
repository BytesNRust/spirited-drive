/* dep71: a bitmap of the car's own (formats/car_texture_add.h). */
#include "formats/car_texture_add.h"
#include "formats/cfm_build.h"
#include "formats/chunk.h"
#include "formats/mesh.h"
#include "formats/mesh_build.h"
#include "formats/shpi_build.h"

#include <stdlib.h>
#include <string.h>

/* bank entries the race or the tools look up by name (car_model.c, psx_bigf.c, car_repaint.c) */
static const char *const RESERVED_NAMES[] = { "tyr1", "tyr2", "tyr3", "tyr4", "shad", "circ", "topv", "rsid", "bott", "wing",
                                               "frnt", "rear", "side", "back", "j000", "j001" };
/* reference labels the race looks up (car_model.c's tyres, the palette's) */
static const char *const RESERVED_LABELS[] = { "frntires", "rertires", "tire1", "tire2", "tire3", "tire4", "pal" };

static int is_lower_or_digit(char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); }

const char *car_texture_name_refused(const char *name)
{
    if (strlen(name) != 4) return "a bitmap's name is 4 characters";
    if (name[0] == '!') return "a name starting with '!' is a palette's";
    char lower[5] = { 0 };
    for (int i = 0; i < 4; i++) lower[i] = (char)(name[i] >= 'A' && name[i] <= 'Z' ? name[i] - 'A' + 'a' : name[i]);
    for (size_t i = 0; i < sizeof RESERVED_NAMES / sizeof *RESERVED_NAMES; i++)
        if (strcmp(lower, RESERVED_NAMES[i]) == 0) return "the race looks that name up for a texture of its own (tyres, shadow, paint)";
    for (int i = 0; i < 4; i++)
        if (!is_lower_or_digit(name[i])) return "only a-z and 0-9 (lower case)";
    return NULL;
}

const char *car_texture_label_refused(const char *label)
{
    const size_t n = strlen(label);
    if (n == 0 || n > 8) return "a label is 1 to 8 characters";
    for (size_t i = 0; i < n; i++)
        if (!is_lower_or_digit(label[i]) && label[i] != '_') return "only a-z, 0-9 and _ in a label";
    for (size_t i = 0; i < sizeof RESERVED_LABELS / sizeof *RESERVED_LABELS; i++)
        if (strcmp(label, RESERVED_LABELS[i]) == 0) return "the race looks that label up (the tyres, the palette)";
    return NULL;
}

/* the bank with the bitmap after its last entry (malloc'd SHPI) */
static int bank_with(const uint8_t *shpi, size_t len, const char *name, const uint8_t *pixels, uint16_t w, uint16_t h, uint8_t **out,
                     size_t *out_len, ExchangeError *err)
{
    const size_t declared = len >= 8 ? (size_t)shpi[4] | (size_t)shpi[5] << 8 | (size_t)shpi[6] << 16 | (size_t)shpi[7] << 24 : 0;
    if (declared >= 8 && declared <= len) len = declared;   /* not the CFM's padding after it */
    ShpiDescEntry *e = malloc((CAR_TEXTURE_ENTRIES_MAX + 1) * sizeof *e);
    size_t n = 0;
    if (e == NULL) { exchange_fail(err, "out of memory"); return 0; }
    int ok = shpi_describe(shpi, len, e, CAR_TEXTURE_ENTRIES_MAX, &n, err);
    for (size_t i = 0; ok && i < n; i++)
        if (memcmp(e[i].name, name, 4) == 0) ok = 0, exchange_fail(err, "the bank has a bitmap '%.4s' already", name);
    if (ok && n >= CAR_TEXTURE_ENTRIES_MAX) ok = 0, exchange_fail(err, "the bank is full (%d entries)", CAR_TEXTURE_ENTRIES_MAX);
    if (ok) {
        ShpiDescEntry *x = &e[n];
        memset(x, 0, sizeof *x);
        memcpy(x->name, name, 4);
        x->type = 0x7b, x->width = w, x->height = h, x->data = pixels, x->block_size = SHPI_BLOCK_AUTO, x->dir_index = (uint32_t)n;
        ok = shpi_build(e, n + 1, out, out_len, err);
    }
    free(e);
    return ok;
}

/* the mesh with a reference labelled `label` to `name` after its last (malloc'd ORIP), its index in `*ref` */
static int mesh_with(const uint8_t *orip, size_t len, const char *name, const char *label, uint8_t **out, size_t *out_len, uint32_t *ref,
                     ExchangeError *err)
{
    Chunk c;
    Mesh m;
    MeshDesc d;
    if (chunk_open(&c, orip, len) != CHUNK_OK || mesh_open(&m, &c) != MESH_OK) { exchange_fail(err, "the mesh does not open"); return 0; }
    if (!mesh_describe(&m, &d, err)) return 0;
    int ok = 1;
    if (d.texture_count >= CAR_TEXTURE_REFS_MAX) ok = 0, exchange_fail(err, "the model has %d textures, all a model holds", CAR_TEXTURE_REFS_MAX);
    MeshTexture *t = ok ? realloc(d.textures, (d.texture_count + 1) * sizeof *t) : NULL;
    if (ok && t == NULL) ok = 0, exchange_fail(err, "out of memory");
    if (ok) {
        d.textures = t;
        MeshTexture *x = &t[d.texture_count];
        memset(x, 0, sizeof *x);
        memcpy(x->label, label, strlen(label));
        memcpy(x->name, name, 4);
        for (uint32_t i = 0; i < d.texture_count; i++)   /* the flat colour word of the mesh's own plain references */
            if (t[i].label[0] == '\0' && t[i].name[0] != '\0') {
                memcpy(x->unknown_1, t[i].unknown_1, sizeof x->unknown_1);
                break;
            }
        *ref = d.texture_count++;
        ok = mesh_build(&d, out, out_len, err);
    }
    mesh_desc_free(&d);
    return ok;
}

int car_texture_add(const uint8_t *cfm, size_t len, int level, const char *name, const char *label, const uint8_t *pixels, uint16_t w,
                    uint16_t h, uint8_t **out, size_t *out_len, uint32_t *ref, ExchangeError *err)
{
    *out = NULL, *out_len = 0;
    const char *why = car_texture_name_refused(name);
    if (why != NULL) { exchange_fail(err, "'%s': %s", name, why); return 0; }
    if ((why = car_texture_label_refused(label)) != NULL) { exchange_fail(err, "label '%s': %s", label, why); return 0; }
    if (level != 0 && level != 1) { exchange_fail(err, "no level %d (0 the high model, 1 the far one)", level); return 0; }
    if (w == 0 || h == 0 || w > CAR_TEXTURE_PNG_MAX_SIDE || h > CAR_TEXTURE_PNG_MAX_SIDE) {
        exchange_fail(err, "%ux%u: a side is 1 to %d", w, h, CAR_TEXTURE_PNG_MAX_SIDE);
        return 0;
    }
    const uint8_t *child[CFM_CHILDREN];
    size_t clen[CFM_CHILDREN];
    if (!cfm_describe(cfm, len, child, clen, err)) return 0;
    uint8_t *bank = NULL, *mesh = NULL;
    size_t bank_len = 0, mesh_len = 0;
    int ok = bank_with(child[2 * level + 1], clen[2 * level + 1], name, pixels, w, h, &bank, &bank_len, err) &&
             mesh_with(child[2 * level], clen[2 * level], name, label, &mesh, &mesh_len, ref, err);
    if (ok) {
        const uint8_t *parts[CFM_CHILDREN];
        size_t parts_len[CFM_CHILDREN];
        for (int k = 0; k < CFM_CHILDREN; k++) parts[k] = child[k], parts_len[k] = clen[k];
        parts[2 * level] = mesh, parts_len[2 * level] = mesh_len, parts[2 * level + 1] = bank, parts_len[2 * level + 1] = bank_len;
        ok = cfm_build(parts, parts_len, out, out_len, err);
    }
    free(bank), free(mesh);
    return ok;
}
