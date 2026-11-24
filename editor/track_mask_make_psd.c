/* See track_mask_make_psd.h. */
#include "track_mask_make_psd.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { MP_PATH = 1024, MP_MAX_SIDE = 8192, MP_MASK_OPACITY = 128 };

__attribute__((format(printf, 3, 4))) static int say(char *why, size_t cap, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(why, cap, fmt, ap);
    va_end(ap);
    return 0;
}

static void put16(uint8_t **p, unsigned v)
{
    *(*p)++ = (uint8_t)(v >> 8);
    *(*p)++ = (uint8_t)v;
}

static void put32(uint8_t **p, uint32_t v)
{
    put16(p, v >> 16);
    put16(p, v & 0xffffu);
}

static void put_bytes(uint8_t **p, const void *src, size_t n)
{
    memcpy(*p, src, n);
    *p += n;
}

/* one layer record: the whole canvas, `channels` channels (ids in `ids`), a Pascal name padded to four, and, locked, the 'lspf' protection block */
static void layer_record(uint8_t **p, const char *name, int w, int h, int opacity, int lock, const int *ids, int channels)
{
    put32(p, 0), put32(p, 0), put32(p, (uint32_t)h), put32(p, (uint32_t)w);
    put16(p, (unsigned)channels);
    for (int i = 0; i < channels; ++i) put16(p, (unsigned)ids[i] & 0xffffu), put32(p, 2u + (uint32_t)w * (uint32_t)h);   /* the compression word, then raw rows */
    put_bytes(p, "8BIMnorm", 8);
    *(*p)++ = (uint8_t)opacity, *(*p)++ = 0, *(*p)++ = 0, *(*p)++ = 0;   /* opacity, clipping, flags, filler */
    const size_t n = strlen(name), padded = (1 + n + 3) / 4 * 4;
    put32(p, 4 + 4 + (uint32_t)padded + (lock ? 16u : 0u));   /* the extra data: no layer mask, no blending ranges, the name, the lock */
    put32(p, 0), put32(p, 0);
    *(*p)++ = (uint8_t)n;
    put_bytes(p, name, n);
    for (size_t i = 1 + n; i < padded; ++i) *(*p)++ = 0;
    if (lock) put_bytes(p, "8BIMlspf", 8), put32(p, 4), put32(p, 0x80000000u);
}

static void plane(uint8_t **p, size_t pixels, int value)   /* raw channel data: the compression word 0, then the plane */
{
    put16(p, 0);
    memset(*p, value, pixels);
    *p += pixels;
}

uint8_t *track_mask_psd_bytes(const uint8_t *rgba, int w, int h, size_t *len)
{
    if (rgba == NULL || w < 1 || h < 1 || w > MP_MAX_SIDE || h > MP_MAX_SIDE) return NULL;
    const size_t px = (size_t)w * (size_t)h, chan = 2 + px;
    static const int tex_ids[3] = { 0, 1, 2 }, mask_ids[4] = { -1, 0, 1, 2 };   /* the mask's alpha first, as Photoshop writes it */
    const size_t records = 2 * (16 + 2 + 3 * 6 + 12 + 4 + 4 + 4 + 8 + 8 + 4) + 64;   /* an upper bound: the records are well under it */
    const size_t info = 2 + records + 3 * chan + 4 * chan + 1;
    const size_t total = 26 + 4 + 4 + 4 + info + 4 + 2 + 3 * px + 64;
    uint8_t *out = malloc(total), *p = out;
    if (out == NULL) return NULL;
    put_bytes(&p, "8BPS", 4), put16(&p, 1);
    memset(p, 0, 6), p += 6;
    put16(&p, 3), put32(&p, (uint32_t)h), put32(&p, (uint32_t)w), put16(&p, 8), put16(&p, 3);   /* 3 channels, 8 bits, RGB */
    put32(&p, 0), put32(&p, 0);   /* no colour mode data, no image resources */
    uint8_t *section_len = p;
    p += 4;
    uint8_t *info_len = p;
    p += 4;
    uint8_t *info_start = p;
    put16(&p, 2);
    layer_record(&p, "texture", w, h, 255, 1, tex_ids, 3);
    layer_record(&p, "mask", w, h, MP_MASK_OPACITY, 0, mask_ids, 4);
    for (int c = 0; c < 3; ++c) {   /* the texture's planes */
        put16(&p, 0);
        for (size_t i = 0; i < px; ++i) *p++ = rgba[4 * i + (size_t)c];
    }
    plane(&p, px, 0);   /* the mask: alpha 0 everywhere (nothing painted), its colour planes white as the Python writer has them */
    plane(&p, px, 255), plane(&p, px, 255), plane(&p, px, 255);
    if ((size_t)(p - info_start) % 2 != 0) *p++ = 0;
    uint8_t *q = info_len;
    put32(&q, (uint32_t)(p - info_start));
    put32(&p, 0);   /* no global layer mask */
    q = section_len;
    put32(&q, (uint32_t)(p - info_start) + 4);   /* the layer info's length word, its bytes and the global mask word */
    put16(&p, 0);   /* the merged image, raw: the texture under an empty mask is the texture */
    for (int c = 0; c < 3; ++c)
        for (size_t i = 0; i < px; ++i) *p++ = rgba[4 * i + (size_t)c];
    *len = (size_t)(p - out);
    return out;
}

static int mkdirs(const char *path)   /* path and its parents, as mkdir -p */
{
    char tmp[MP_PATH];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *s = tmp + 1; *s != '\0'; ++s)
        if (*s == '/') {
            *s = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return 0;
            *s = '/';
        }
    return mkdir(tmp, 0755) == 0 || errno == EEXIST;
}

int track_mask_make_psd(const char *root, const char *name, const uint8_t *rgba, int w, int h, char *why, size_t why_cap)
{
    char dir[MP_PATH], dest[MP_PATH], tmp[MP_PATH];
    if (rgba == NULL || w <= 0 || h <= 0) return say(why, why_cap, "%s has no pixels to make a PSD from", name);
    if (name[0] == '\0' || strchr(name, '/') != NULL || strchr(name, '.') != NULL) return say(why, why_cap, "\"%s\" is not a texture name", name);
    snprintf(dir, sizeof dir, "%s/art-source/masks", root);
    snprintf(dest, sizeof dest, "%s/%s.psd", dir, name);
    snprintf(tmp, sizeof tmp, "%s/.%s.psd.part", dir, name);
    if (access(dest, F_OK) == 0) return say(why, why_cap, "%s.psd is already in art-source/masks: Studio does not overwrite a PSD", name);
    size_t len = 0;
    uint8_t *bytes = track_mask_psd_bytes(rgba, w, h, &len);
    if (bytes == NULL) return say(why, why_cap, "the PSD of %s could not be made (%dx%d)", name, w, h);
    int ok = mkdirs(dir);
    if (!ok) say(why, why_cap, "art-source/masks could not be made: %s", strerror(errno));
    FILE *f = ok ? fopen(tmp, "wb") : NULL;
    if (ok && f == NULL) ok = say(why, why_cap, "%s could not be written: %s", name, strerror(errno));
    if (f != NULL) {
        const int wrote = fwrite(bytes, 1, len, f) == len;
        if (fclose(f) != 0 || !wrote) ok = say(why, why_cap, "%s.psd could not be written: %s", name, strerror(errno));
    }
    free(bytes);
    if (ok && access(dest, F_OK) != 0 && rename(tmp, dest) != 0) ok = say(why, why_cap, "%s.psd could not be put in art-source/masks: %s", name, strerror(errno));
    if (!ok) unlink(tmp);
    else why[0] = '\0';
    return ok;
}
