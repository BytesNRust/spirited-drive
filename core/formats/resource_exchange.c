#include "formats/resource_exchange.h"

#include "formats/bnk.h"
#include "formats/bnk_exchange.h"
#include "formats/doc.h"
#include "formats/le.h"
#include "formats/mesh_exchange.h"
#include "formats/pbs.h"
#include "formats/pdn.h"
#include "formats/qfs.h"
#include "formats/shpi_exchange.h"
#include "formats/tri.h"
#include "formats/tri_exchange.h"
#include "formats/tsv.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { KIND_RAW, KIND_SHPI, KIND_TREE, KIND_TRI, KIND_PBS, KIND_PDN, KIND_BNK } Kind;

static const char *const kind_name[] = { "raw", "shpi", "tree", "tri", "pbs", "pdn", "bnk" };

static int has_extension(const char *name, const char *ext)
{
    const size_t n = strlen(name), e = strlen(ext);
    if (n < e) return 0;
    for (size_t i = 0; i < e; ++i)
        if (tolower((unsigned char)name[n - e + i]) != ext[i]) return 0;
    return 1;
}

static Kind kind_of(const uint8_t *d, size_t n, const char *name)
{
    if (n >= 4 && memcmp(d, "SHPI", 4) == 0) return KIND_SHPI;
    if (n >= 4 && memcmp(d, "wwww", 4) == 0) return KIND_TREE;
    Tri tri;
    if (n > TRI_HEADER_SIZE + 12 && d[0] == TRI_VERSION && memcmp(d + TRI_HEADER_SIZE, "SJBO", 4) == 0 && tri_open(&tri, d, n) == TRI_OK) return KIND_TRI;
    if (n == PBS_SIZE && has_extension(name, ".pbs")) return KIND_PBS;
    if (n == PDN_SIZE && has_extension(name, ".pdn")) return KIND_PDN;
    if (has_extension(name, ".bnk")) {
        BnkDoc *doc = NULL;
        if (bnk_doc_read(d, n, &doc) == BNK_OK) { bnk_doc_free(doc); return KIND_BNK; }
    }
    return KIND_RAW;
}

/* The payload of a file: the file itself, or its unpacked content (malloc'd). */
static int unpack(const uint8_t *file, size_t size, uint8_t **payload, size_t *payload_size, int *packed, ExchangeError *err)
{
    size_t unpacked = 0;
    *packed = qfs_is_compressed(file, size) && qfs_decompressed_size(file, size, &unpacked) == QFS_OK;
    if (!*packed) {
        *payload = malloc(size ? size : 1);
        if (*payload == NULL) { exchange_fail(err, "out of memory"); return 0; }
        memcpy(*payload, file, size);
        *payload_size = size;
        return 1;
    }
    *payload = malloc(unpacked ? unpacked : 1);
    if (*payload == NULL) { exchange_fail(err, "out of memory"); return 0; }
    const QfsResult r = qfs_decompress(file, size, *payload, unpacked);
    if (r != QFS_OK) { free(*payload); exchange_fail(err, "QFS stream is damaged: %s", qfs_result_str(r)); return 0; }
    *payload_size = unpacked;
    return 1;
}

/* ------------------------------------------------------------------ trees */

typedef struct {
    const DocNode *node;
    char path[256];
} Seen;

typedef struct {
    Seen *seen;
    size_t count, cap;
} SeenList;

static const Seen *seen_find(const SeenList *l, const DocNode *n)
{
    for (size_t i = 0; i < l->count; ++i) if (l->seen[i].node == n) return &l->seen[i];
    return NULL;
}

static int seen_add(SeenList *l, const DocNode *n, const char *path)
{
    if (l->count == l->cap) {
        const size_t cap = l->cap ? l->cap * 2 : 32;
        Seen *grown = realloc(l->seen, cap * sizeof *grown);
        if (grown == NULL) return 0;
        l->seen = grown;
        l->cap = cap;
    }
    l->seen[l->count].node = n;
    snprintf(l->seen[l->count].path, sizeof l->seen[l->count].path, "%s", path);
    l->count++;
    return 1;
}

static int is_orip(const uint8_t *b, size_t n) { return n >= 8 && memcmp(b, "ORIP", 4) == 0 && (size_t)le_u32(b + 4) <= n; }

static int tree_export(const DocNode *node, const char *path, const uint8_t *palette, FileSet *out, TsvWriter *manifest, SeenList *seen, ExchangeError *err);

static int leaf_export(const DocNode *node, const char *path, const uint8_t *palette, FileSet *out, TsvWriter *manifest, ExchangeError *err)
{
    size_t n = 0;
    const uint8_t *b = doc_bytes(node, &n);
    FileSet tmp;
    fileset_init(&tmp);
    const char *kind = "raw";
    int ok;
    if (is_orip(b, n)) {
        kind = "orip";
        ok = mesh_export(b, n, "mesh", "textures", &tmp, err);
    } else if (n >= 4 && memcmp(b, "SHPI", 4) == 0) {
        kind = "shpi";
        ok = shpi_export(b, n, palette, &tmp, err);
    } else {
        ok = fileset_add(&tmp, "raw.bin", b, n);
        if (!ok) exchange_fail(err, "%s: out of memory", path);
    }
    char prefix[300];
    snprintf(prefix, sizeof prefix, "%s/", path);
    if (ok && !fileset_move_prefixed(out, &tmp, prefix)) { exchange_fail(err, "%s: cannot store the exported files", path); ok = 0; }
    else if (!ok) fileset_free(&tmp);
    if (ok) { tsv_str(manifest, path); tsv_str(manifest, kind); tsv_row(manifest); }
    return ok;
}

static int tree_export(const DocNode *node, const char *path, const uint8_t *palette, FileSet *out, TsvWriter *manifest, SeenList *seen, ExchangeError *err)
{
    for (uint32_t i = 0; i < doc_entry_count(node); ++i) {
        DocNode *child = NULL;
        char child_path[300];
        snprintf(child_path, sizeof child_path, path[0] ? "%s/c%u" : "%s" "c%u", path, i);
        doc_entry(node, i, &child);
        if (child == NULL) { tsv_str(manifest, child_path); tsv_str(manifest, "absent"); tsv_row(manifest); continue; }
        const Seen *earlier = seen_find(seen, child);
        if (earlier != NULL) {
            tsv_str(manifest, child_path);
            tsv_str(manifest, "same");
            tsv_str(manifest, earlier->path);
            tsv_row(manifest);
            continue;
        }
        if (!seen_add(seen, child, child_path)) { exchange_fail(err, "out of memory"); return 0; }
        if (doc_is_container(child)) {
            tsv_str(manifest, child_path);
            tsv_str(manifest, "tree");
            tsv_row(manifest);
            if (!tree_export(child, child_path, palette, out, manifest, seen, err)) return 0;
        } else if (!leaf_export(child, child_path, palette, out, manifest, err)) {
            return 0;
        }
    }
    return 1;
}

typedef struct {
    ImportStats *stats;
    const uint8_t *palette;
    const FileSet *in;
    SeenList seen;
} ImportCtx;

static int tree_import(DocNode *node, const char *path, ImportCtx *ctx, ExchangeError *err)
{
    for (uint32_t i = 0; i < doc_entry_count(node); ++i) {
        DocNode *child = NULL;
        char child_path[300], prefix[310];
        snprintf(child_path, sizeof child_path, path[0] ? "%s/c%u" : "%s" "c%u", path, i);
        doc_entry(node, i, &child);
        if (child == NULL || seen_find(&ctx->seen, child) != NULL) continue;
        if (!seen_add(&ctx->seen, child, child_path)) { exchange_fail(err, "out of memory"); return 0; }
        if (doc_is_container(child)) {
            if (!tree_import(child, child_path, ctx, err)) return 0;
            continue;
        }
        size_t n = 0;
        const uint8_t *b = doc_bytes(child, &n);
        FileSet sub;
        fileset_init(&sub);
        snprintf(prefix, sizeof prefix, "%s/", child_path);
        if (!fileset_copy_stripped(&sub, ctx->in, prefix)) { fileset_free(&sub); exchange_fail(err, "out of memory"); return 0; }
        uint8_t *bytes = NULL;
        size_t len = 0;
        int ok;
        if (is_orip(b, n)) {
            size_t declared = le_u32(b + 4);
            uint8_t *mesh = NULL;
            ok = mesh_import(b, n, "mesh", &sub, &mesh, &len, err);
            if (ok) {   /* keep whatever the extent holds after the declared length */
                bytes = malloc(len + (n - declared) + 1);
                if (bytes == NULL) { free(mesh); exchange_fail(err, "out of memory"); ok = 0; }
                else { memcpy(bytes, mesh, len); memcpy(bytes + len, b + declared, n - declared); len += n - declared; free(mesh); }
            }
        } else if (n >= 4 && memcmp(b, "SHPI", 4) == 0) {
            ImportStats s;
            ok = shpi_import(b, n, ctx->palette, &sub, &bytes, &len, &s, err);
            if (ok) { ctx->stats->pixels += s.pixels; ctx->stats->reassigned += s.reassigned; }
        } else {
            const SetFile *raw = fileset_find(&sub, "raw.bin");
            ok = raw != NULL;
            if (!ok) exchange_fail(err, "%sraw.bin not found", prefix);
            else { bytes = malloc(raw->size ? raw->size : 1); if (bytes) { memcpy(bytes, raw->data, raw->size); len = raw->size; } else ok = 0; }
        }
        fileset_free(&sub);
        if (!ok) { free(bytes); return 0; }
        const DocResult r = doc_set_bytes(child, bytes, len);
        free(bytes);
        if (r != DOC_OK) { exchange_fail(err, "%s: cannot replace the data: %s", child_path, doc_result_str(r)); return 0; }
    }
    return 1;
}

/* ------------------------------------------------------------------ files */

int resource_export(const uint8_t *file, size_t size, const char *name, const uint8_t *palette, FileSet *out, ExchangeError *err)
{
    uint8_t *payload = NULL;
    size_t n = 0;
    int packed = 0;
    if (!unpack(file, size, &payload, &n, &packed, err)) return 0;
    const Kind kind = kind_of(payload, n, name);
    FileSet tmp;
    fileset_init(&tmp);
    int ok = 1;
    TsvWriter res;
    tsv_writer_init(&res);
    tsv_comment(&res, "kind\tpacked\tpayload_bytes");
    switch (kind) {
    case KIND_SHPI: ok = shpi_export(payload, n, palette, &tmp, err); break;
    case KIND_TRI: ok = tri_export(payload, n, &tmp, err); break;
    case KIND_PBS: ok = pbs_export(payload, &tmp, err); break;
    case KIND_PDN: ok = pdn_export(payload, &tmp, err); break;
    case KIND_BNK: ok = bnk_export(payload, n, &tmp, err); break;
    case KIND_TREE: {
        DocNode *doc = NULL;
        const DocResult r = doc_parse_chunks(payload, n, &doc);
        if (r != DOC_OK) { exchange_fail(err, "not a readable chunk tree: %s", doc_result_str(r)); ok = 0; break; }
        TsvWriter manifest;
        SeenList seen = {0};
        tsv_writer_init(&manifest);
        tsv_comment(&manifest, "path\tkind\tsame_as");
        ok = tree_export(doc, "", palette, &tmp, &manifest, &seen, err);
        if (ok && (manifest.failed || !fileset_add(&tmp, "tree.tsv", manifest.data, manifest.len))) { exchange_fail(err, "cannot write tree.tsv"); ok = 0; }
        tsv_writer_free(&manifest);
        free(seen.seen);
        doc_free(doc);
        break;
    }
    default: ok = fileset_add(&tmp, "raw.bin", payload, n); if (!ok) exchange_fail(err, "out of memory"); break;
    }
    if (ok) {
        tsv_str(&res, kind_name[kind]);
        tsv_uint(&res, (uint32_t)packed);
        tsv_uint(&res, (uint32_t)n);
        tsv_row(&res);
        ok = !res.failed && fileset_add(&tmp, "resource.tsv", res.data, res.len);
        if (!ok) exchange_fail(err, "cannot write resource.tsv");
    }
    tsv_writer_free(&res);
    free(payload);
    if (ok && !fileset_move_prefixed(out, &tmp, "")) { exchange_fail(err, "cannot store the exported files"); ok = 0; }
    else if (!ok) fileset_free(&tmp);
    return ok;
}

int resource_import(const uint8_t *original, size_t size, const char *name, const uint8_t *palette, const FileSet *in,
                    uint8_t **out, size_t *out_len, ImportStats *stats, ExchangeError *err)
{
    uint8_t *payload = NULL;
    size_t n = 0;
    int packed = 0;
    memset(stats, 0, sizeof *stats);
    if (!unpack(original, size, &payload, &n, &packed, err)) return 0;
    const Kind kind = kind_of(payload, n, name);

    const SetFile *res = fileset_find(in, "resource.tsv");
    if (res == NULL) { exchange_fail(err, "resource.tsv not found"); free(payload); return 0; }
    {
        TsvReader r;
        TsvRow row;
        tsv_reader_init(&r, res->data, res->size);
        const int got = tsv_next(&r, &row) == TSV_OK && row.cells >= 1;
        const int match = got && strcmp(row.cell[0], kind_name[kind]) == 0;
        if (!got || !match) {
            exchange_fail(err, "resource.tsv says '%s' but the original file is a '%s'", got ? row.cell[0] : "?", kind_name[kind]);
            tsv_reader_free(&r);
            free(payload);
            return 0;
        }
        tsv_reader_free(&r);
    }

    uint8_t *edited = NULL;
    size_t edited_len = 0;
    int ok = 1;
    switch (kind) {
    case KIND_SHPI: ok = shpi_import(payload, n, palette, in, &edited, &edited_len, stats, err); break;
    case KIND_TRI: ok = tri_import(payload, n, in, &edited, &edited_len, err); break;
    case KIND_PBS: {
        edited = malloc(PBS_SIZE);
        ok = edited != NULL;
        if (ok) { memcpy(edited, payload, PBS_SIZE); edited_len = PBS_SIZE; ok = pbs_import(edited, in, err); }
        break;
    }
    case KIND_PDN: {
        edited = malloc(PDN_SIZE);
        ok = edited != NULL;
        if (ok) { memcpy(edited, payload, PDN_SIZE); edited_len = PDN_SIZE; ok = pdn_import(edited, in, err); }
        break;
    }
    case KIND_BNK: ok = bnk_import(payload, n, in, &edited, &edited_len, err); break;
    case KIND_TREE: {
        DocNode *doc = NULL;
        const DocResult r = doc_parse_chunks(payload, n, &doc);
        if (r != DOC_OK) { exchange_fail(err, "the original is not a readable chunk tree: %s", doc_result_str(r)); ok = 0; break; }
        ImportCtx ctx = { stats, palette, in, {0} };
        ok = tree_import(doc, "", &ctx, err);
        if (ok) {
            edited_len = doc_size(doc);
            edited = malloc(edited_len ? edited_len : 1);
            size_t w = 0;
            if (edited == NULL || doc_write(doc, edited, edited_len, &w) != DOC_OK) { exchange_fail(err, "cannot assemble the tree"); ok = 0; }
        }
        free(ctx.seen.seen);
        doc_free(doc);
        break;
    }
    default: {
        const SetFile *raw = fileset_find(in, "raw.bin");
        ok = raw != NULL;
        if (!ok) exchange_fail(err, "raw.bin not found");
        else { edited = malloc(raw->size ? raw->size : 1); ok = edited != NULL; if (ok) { memcpy(edited, raw->data, raw->size); edited_len = raw->size; } }
        break;
    }
    }
    free(payload);
    if (!ok) { free(edited); return 0; }

    /* unchanged: the original file as it was */
    uint8_t *check = NULL;
    size_t check_len = 0;
    int same = 0;
    {
        int p2 = 0;
        ExchangeError ignore = {{0}};
        if (unpack(original, size, &check, &check_len, &p2, &ignore)) same = check_len == edited_len && memcmp(check, edited, edited_len) == 0;
        free(check);
    }
    if (same) {
        *out = malloc(size ? size : 1);
        if (*out == NULL) { free(edited); exchange_fail(err, "out of memory"); return 0; }
        memcpy(*out, original, size);
        *out_len = size;
        free(edited);
        return 1;
    }
    if (!packed) { *out = edited; *out_len = edited_len; return 1; }
    uint8_t *packed_buf = malloc(qfs_compress_bound(edited_len));
    size_t packed_len = 0;
    if (packed_buf == NULL) { free(edited); exchange_fail(err, "out of memory"); return 0; }
    QfsResult q = qfs_compress(edited, edited_len, packed_buf, qfs_compress_bound(edited_len), &packed_len);
    if (q == QFS_OK && !qfs_inplace_ok(packed_buf, packed_len, edited_len)) q = QFS_ERR_OVERFLOW;
    free(edited);
    if (q != QFS_OK) {
        free(packed_buf);
        exchange_fail(err, "the edited file cannot be packed so that the game unpacks it in place (a loader limit of 004a2520: %s)", qfs_result_str(q));
        return 0;
    }
    *out = packed_buf;
    *out_len = packed_len;
    return 1;
}
