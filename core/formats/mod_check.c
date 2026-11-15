/* See mod_check.h. */
#include "formats/mod_check.h"

#include "formats/modpath.h"
#include "formats/png.h"
#include "formats/validate.h"
#include "formats/wav.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int refuse(char *why, size_t cap, const char *format, ...) __attribute__((format(printf, 3, 4)));
static int refuse(char *why, size_t cap, const char *format, ...)
{
    if (why != NULL && cap > 0) {
        va_list ap;
        va_start(ap, format);
        vsnprintf(why, cap, format, ap);
        va_end(ap);
    }
    return 0;
}

/* ---- names */

/* the path's components: their count, and the first, the last and the last's extension (after its last '.', "" without one) */
typedef struct {
    int count;
    char first[ZIP_NAME_MAX + 1];
    const char *last, *ext;
} Parts;

static Parts parts_of(const char *rel)
{
    Parts p = { 1, "", rel, "" };
    const char *slash = strchr(rel, '/');
    const size_t first = slash != NULL ? (size_t)(slash - rel) : strlen(rel);
    memcpy(p.first, rel, first);
    p.first[first] = '\0';
    for (const char *c = rel; *c; c++)
        if (*c == '/') p.count++, p.last = c + 1;
    const char *dot = strrchr(p.last, '.');
    p.ext = dot != NULL ? dot + 1 : "";
    return p;
}

static int is(const char *a, const char *b) { return strcasecmp(a, b) == 0; }

static int litter(const Parts *p)
{
    return is(p->first, "__MACOSX") || is(p->last, ".DS_Store") || is(p->last, "Thumbs.db") || is(p->last, "desktop.ini") ||
           strncmp(p->last, "._", 2) == 0;
}

ModFileKind mod_file_kind(const char *rel)
{
    const Parts p = parts_of(rel);
    if (litter(&p)) return MOD_KIND_SKIP;
    if (p.count >= 2 && (is(p.first, "SIMDATA") || is(p.first, "GAMEDATA") || is(p.first, "FRONTEND"))) return MOD_KIND_GAME;
    if (p.count == 2 && (is(p.first, "track_light") || is(p.first, "track")) && is(p.ext, "txt")) return MOD_KIND_TEXT;
    if (p.count == 3 && (is(p.first, "car_masks") || is(p.first, "dash_masks") || is(p.first, "track_masks")) && is(p.ext, "png"))
        return MOD_KIND_PNG;
    if (p.count == 2 && is(p.first, "sounds") && is(p.ext, "wav")) return MOD_KIND_WAV;
    if (p.count == 1) {
        if (is(rel, "mod.ini")) return MOD_KIND_CARD;
        if (is(rel, "car_envmap.png") || is(rel, "preview.png")) return MOD_KIND_PNG;
        static const char *const DOCS[] = { "README", "README.txt", "README.md", "LICENSE", "LICENSE.txt", "LICENSE.md" };
        for (size_t i = 0; i < sizeof DOCS / sizeof DOCS[0]; i++)
            if (is(rel, DOCS[i])) return MOD_KIND_DOC;
    }
    return MOD_KIND_NONE;
}

/* the folder that is the mod's root: "" when the card is at the zip's root, else the one top folder that holds the card and everything
 * else (the litter apart); 0 when there is neither */
static int find_root(const ZipArchive *z, char root[ZIP_NAME_MAX + 1])
{
    char rel[ZIP_NAME_MAX + 1];
    root[0] = '\0';
    for (size_t i = 0; i < z->count; i++)
        if (modpath_clean(z->entries[i].name, rel, sizeof rel) && is(rel, "mod.ini")) return 1;
    for (size_t i = 0; i < z->count; i++) {
        if (!modpath_clean(z->entries[i].name, rel, sizeof rel)) continue;
        const Parts p = parts_of(rel);
        if (litter(&p)) continue;
        if (root[0] == '\0') snprintf(root, ZIP_NAME_MAX + 1, "%s/", p.first);
        if (strncmp(rel, root, strlen(root)) != 0 && strcmp(rel, p.first) != 0) return 0;   /* outside the folder */
    }
    if (root[0] == '\0') return 0;
    char card[ZIP_NAME_MAX + 16];
    snprintf(card, sizeof card, "%smod.ini", root);
    for (size_t i = 0; i < z->count; i++)
        if (modpath_clean(z->entries[i].name, rel, sizeof rel) && is(rel, card)) return 1;
    root[0] = '\0';
    return 0;
}

int mod_check_install_path(const ModCheck *c, const char *entry_name, char *rel, size_t cap)
{
    char clean[ZIP_NAME_MAX + 1];
    if (!modpath_clean(entry_name, clean, sizeof clean)) return 0;
    const size_t r = strlen(c->root);
    if (strncmp(clean, c->root, r) != 0 || clean[r] == '\0' || strlen(clean + r) + 1 > cap) return 0;
    memcpy(rel, clean + r, strlen(clean + r) + 1);
    return 1;
}

/* ---- content */

static int text_ok(const uint8_t *data, size_t size, size_t max) { return size <= max && memchr(data, '\0', size) == NULL; }

/* entry `rel`'s content checked by its kind; 0 with `why` set when it is refused */
static int content_ok(ModFileKind kind, const char *rel, const uint8_t *data, size_t size, ModCheck *c, char *why, size_t cap)
{
    switch (kind) {
    case MOD_KIND_GAME: {
        Report report;
        report_init(&report);
        validate_file(data, size, rel, &report);
        const int bad = report_fatal_count(&report) > 0;
        char text[200] = "";
        if (bad) report_text(&report, VALIDATE_COMPATIBLE, text, sizeof text);
        report_free(&report);
        return bad ? refuse(why, cap, "%s is not valid for the game: %s", rel, text) : 1;
    }
    case MOD_KIND_TEXT: return text_ok(data, size, MOD_TEXT_MAX) ? 1 : refuse(why, cap, "%s is not a text file of up to 1 MB", rel);
    case MOD_KIND_DOC: return text_ok(data, size, MOD_DOC_MAX) ? 1 : refuse(why, cap, "%s is not a text file of up to 64 KB", rel);
    case MOD_KIND_PNG: {
        PngImage image;
        if (png_read(data, size, &image) != PNG_OK) return refuse(why, cap, "%s is not a readable PNG", rel);
        const int w = image.width, h = image.height;
        png_free(image.pixels);
        if (is(rel, "preview.png")) {
            if (w > MOD_PREVIEW_W || h > MOD_PREVIEW_H) return refuse(why, cap, "preview.png is larger than %d x %d", MOD_PREVIEW_W, MOD_PREVIEW_H);
            c->has_preview = 1;
        }
        return 1;
    }
    case MOD_KIND_WAV: {
        WavInfo info;
        uint8_t *pcm = NULL;
        const int good = wav_read(data, size, &info, &pcm) == WAV_OK;
        free(pcm);
        return good ? 1 : refuse(why, cap, "%s is not a readable WAV", rel);
    }
    case MOD_KIND_CARD: return mod_ini_parse(data, size, &c->info, why, cap);
    default: return refuse(why, cap, "%s is not a file the game reads", rel);
    }
}

/* ---- the whole check */

/* entry `i`'s path below the root and its kind; 0 with `why` set when its name refuses the mod. *kind is MOD_KIND_SKIP for a folder
 * entry and the litter. */
static int entry_ok(const ZipArchive *z, size_t i, const ModCheck *c, char *rel, ModFileKind *kind, char *why, size_t cap)
{
    const ZipEntry *e = &z->entries[i];
    char clean[ZIP_NAME_MAX + 1];
    if (!modpath_clean(e->name, clean, sizeof clean)) return refuse(why, cap, "%s: the name leaves the mod's folder", e->name);
    if (e->is_link) return refuse(why, cap, "%s is a symbolic link", e->name);
    const Parts p = parts_of(clean);
    *kind = MOD_KIND_SKIP;
    if (litter(&p)) return 1;
    if (!mod_check_install_path(c, e->name, rel, ZIP_NAME_MAX + 1)) {
        if (c->root[0] != '\0' && strcmp(clean, p.first) == 0 && e->is_dir) return 1;   /* the root folder's own entry */
        return refuse(why, cap, "%s is outside the mod's folder", e->name);
    }
    if (e->is_dir) return 1;
    *kind = mod_file_kind(rel);
    if (*kind == MOD_KIND_NONE) return refuse(why, cap, "%s is not a file the game reads", rel);
    return 1;
}

static int names_ok(const ZipArchive *z, ModCheck *c, char (*rels)[ZIP_NAME_MAX + 1], ModFileKind *kinds, char *why, size_t cap)
{
    for (size_t i = 0; i < z->count; i++) {
        if (!entry_ok(z, i, c, rels[i], &kinds[i], why, cap)) return 0;
        if (kinds[i] < 0) continue;
        for (size_t j = 0; j < i; j++)
            if (kinds[j] >= 0 && is(rels[i], rels[j])) return refuse(why, cap, "%s and %s are the same file", rels[j], rels[i]);
    }
    return 1;
}

static int contents_ok(const ZipArchive *z, ModCheck *c, char (*rels)[ZIP_NAME_MAX + 1], const ModFileKind *kinds, char *why, size_t cap)
{
    for (size_t i = 0; i < z->count; i++) {
        if (kinds[i] < 0) continue;
        uint8_t *data = NULL;
        size_t size = 0;
        const ZipResult r = zip_extract(z, i, &data, &size);
        if (r != ZIP_OK) return refuse(why, cap, "%s: %s", rels[i], zip_result_text(r));
        const int good = content_ok(kinds[i], rels[i], data, size, c, why, cap);
        free(data);
        if (!good) return 0;
        c->files++, c->bytes += size, c->kinds[kinds[i]]++;
    }
    return c->kinds[MOD_KIND_CARD] == 1 ? 1 : refuse(why, cap, "the mod has no mod.ini");
}

int mod_check_zip(const uint8_t *zip, size_t size, ModCheck *out, char *why, size_t why_cap)
{
    memset(out, 0, sizeof *out);
    ZipArchive z;
    const ZipResult r = zip_open(zip, size, &z);
    if (r != ZIP_OK) return refuse(why, why_cap, "%s", zip_result_text(r));
    if (!find_root(&z, out->root)) return zip_close(&z), refuse(why, why_cap, "the mod has no mod.ini at its root");
    char (*rels)[ZIP_NAME_MAX + 1] = calloc(z.count > 0 ? z.count : 1, sizeof *rels);
    ModFileKind *kinds = calloc(z.count > 0 ? z.count : 1, sizeof *kinds);
    int ok = rels != NULL && kinds != NULL ? 1 : refuse(why, why_cap, "out of memory");
    ok = ok && names_ok(&z, out, rels, kinds, why, why_cap) && contents_ok(&z, out, rels, kinds, why, why_cap);
    free(rels), free(kinds);
    zip_close(&z);
    if (!ok) {
        const ModCheck empty = { 0 };
        *out = empty;
    }
    return ok;
}
