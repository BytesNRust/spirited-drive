/* port: the rally's art made to read on the mirrored track (fam_mirror.h). */
#include "game/race/fam_mirror.h"

#include "formats/chunk.h"

#include <string.h>

/* 1 when `m` is `r` flipped left to right, both w x h */
static int is_flip(const uint8_t *r, const uint8_t *m, int w, int h)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (m[y * w + x] != r[y * w + (w - 1 - x)]) return 0;
    return 1;
}

static void flip(uint8_t *p, int w, int h)
{
    for (int y = 0; y < h; y++)
        for (int a = 0, b = w - 1; a < b; a++, b--) {
            const uint8_t t = p[y * w + a];
            p[y * w + a] = p[y * w + b], p[y * w + b] = t;
        }
}

static int bank(uint8_t *rally, const Chunk *r, const Chunk *m)
{
    Fsh rf, mf;
    if (chunk_bitmaps(r, &rf) != FSH_OK || chunk_bitmaps(m, &mf) != FSH_OK) return 0;
    int flipped = 0;
    for (uint32_t i = 0; i < rf.count; i++) {
        FshEntry re, me;
        uint32_t j;
        if (fsh_entry(&rf, i, &re) != FSH_OK || re.type != FSH_TYPE_BITMAP) continue;
        if (fsh_find(&mf, re.name, &j) != FSH_OK || fsh_entry(&mf, j, &me) != FSH_OK) continue;
        if (me.type != FSH_TYPE_BITMAP || me.width != re.width || me.height != re.height) continue;
        const size_t n = (size_t)re.width * re.height;
        if (memcmp(re.payload, me.payload, n) == 0 || !is_flip(re.payload, me.payload, re.width, re.height)) continue;
        flip(rally + (re.payload - rally), re.width, re.height);
        flipped++;
    }
    return flipped;
}

static int walk(uint8_t *rally, const Chunk *r, const Chunk *m, int depth)
{
    if (r->kind != m->kind || depth > CHUNK_MAX_DEPTH) return 0;
    if (r->kind == CHUNK_KIND_BITMAP) return bank(rally, r, m);
    if (r->kind != CHUNK_KIND_HEADER) return 0;
    int flipped = 0;
    for (uint32_t i = 0; i < r->count && i < m->count; i++) {
        Chunk rc, mc;
        if (chunk_child(r, i, &rc) == CHUNK_OK && chunk_child(m, i, &mc) == CHUNK_OK)
            flipped += walk(rally, &rc, &mc, depth + 1);
    }
    return flipped;
}

int fam_mirror_signs(uint8_t *rally, size_t rally_size, const uint8_t *mirror, size_t mirror_size)
{
    Chunk r, m;
    if (chunk_open(&r, rally, rally_size) != CHUNK_OK || chunk_open(&m, mirror, mirror_size) != CHUNK_OK) return 0;
    return walk(rally, &r, &m, 0);
}
