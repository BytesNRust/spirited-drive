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

/* The folders of the CD's tree and the extensions each holds there, "-" for a name without one (docs/modding/inventory.md §1): a game
 * file anywhere else, or of another type, is nothing the game would ask for. */
static const struct {
    const char *dir, *exts;
} GAME_FOLDERS[] = {
    { "FRONTEND/ART", " ENG FFN FSH GER INV QFS " },        { "FRONTEND/GART", " ENG FFN FSH GER INV QFS " },
    { "FRONTEND/ART/CHECK", " FSH INV QFS " },              { "FRONTEND/GART/CHECK", " FSH INV QFS " },
    { "FRONTEND/ART/OPTION", " FSH INV QFS " },             { "FRONTEND/GART/OPTION", " FSH INV QFS " },
    { "FRONTEND/ART/CONTROL", " INV QFS " },                { "FRONTEND/GART/CONTROL", " INV QFS " },
    { "FRONTEND/ART/STATS", " INV QFS " },                  { "FRONTEND/GART/STATS", " INV QFS " },
    { "FRONTEND/ART/TRACKSEL", " INV QFS " },               { "FRONTEND/GART/TRACKSEL", " INV QFS " },
    { "FRONTEND/ART/FONTS", " FFN " },                      { "FRONTEND/GART/FONTS", " FFN " },
    { "FRONTEND/ART/IART", " - INV QFS TXT " },             { "FRONTEND/GART/IART", " - INV QFS TXT " },
    { "FRONTEND/SHOW", " QFS " },                           { "FRONTEND/GSHOW", " QFS " },
    { "FRONTEND/SPEECH", " EAS " },                         { "FRONTEND/GSPEECH", " EAS " },
    { "FRONTEND/MOVIE", " TGV " },                          { "FRONTEND/MUSIC", " AS4 ASF " },
    { "GAMEDATA/CONFIG", " DAT " },                         { "GAMEDATA/MODEM", " CFG " },
    { "GAMEDATA/REPLAY", " RPL " },
    { "SIMDATA/CARFAMS", " - CFG CFM FSH OUT PDN " },       { "SIMDATA/CARSPECS", " - PBS " },
    { "SIMDATA/DASH", " FMM FSH " },                        { "SIMDATA/GDASH", " FMM FSH " },
    { "SIMDATA/ETRACKFM", " FAM " },                        { "SIMDATA/GTRACKFM", " FAM " },
    { "SIMDATA/NTRACKFM", " FAM " },
    { "SIMDATA/SLIDES", " QFS " },                          { "SIMDATA/GSLIDES", " QFS " },
    { "SIMDATA/MISC", " - B00 B01 B02 B03 B10 B11 B12 B13 B20 B21 B22 B23 FSB FSH MMB QFS TRI " },
    { "SIMDATA/SOUNDBNK", " BNK " },
};

/* whether `rel` (at least two parts, `p` its parts) is in a folder of the CD's tree with an extension that folder holds */
static int game_file(const Parts *p, const char *rel)
{
    const size_t dir = (size_t)(p->last - rel) - 1;
    const size_t n = strlen(p->ext);
    char ext[8] = " - ";
    if (n > 3) return 0;
    if (n > 0) {
        for (size_t i = 0; i < n; i++) ext[1 + i] = (char)(p->ext[i] >= 'a' && p->ext[i] <= 'z' ? p->ext[i] - 32 : p->ext[i]);
        ext[1 + n] = ' ', ext[2 + n] = '\0';
    }
    for (size_t i = 0; i < sizeof GAME_FOLDERS / sizeof GAME_FOLDERS[0]; i++)
        if (strlen(GAME_FOLDERS[i].dir) == dir && strncasecmp(rel, GAME_FOLDERS[i].dir, dir) == 0) return strstr(GAME_FOLDERS[i].exts, ext) != NULL;
    return 0;
}

ModFileKind mod_file_kind(const char *rel)
{
    const Parts p = parts_of(rel);
    if (litter(&p)) return MOD_KIND_SKIP;
    if (p.count >= 3 && (is(p.first, "SIMDATA") || is(p.first, "GAMEDATA") || is(p.first, "FRONTEND")))
        return game_file(&p, rel) ? MOD_KIND_GAME : MOD_KIND_NONE;
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

/* the path entry `entry_name` is installed at, below the mod's folder: its cleaned name without the root; 0 when it has none */
static int install_path(const ModCheck *c, const char *entry_name, char *rel, size_t cap)
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

/* entry `rel`'s content checked by its kind; 0 with `why` set when it is refused. A picture was measured already (pictures_ok). */
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
        png_free(image.pixels);
        if (is(rel, "preview.png")) c->has_preview = 1;
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

enum { KIND_FOLDER = -3 };   /* in the check's own list: a folder entry below the root (never installed, but its name is taken) */

/* entry `i`'s path below the root and its kind; 0 with `why` set when its name refuses the mod. *kind is MOD_KIND_SKIP for the litter
 * and the root's own entry, KIND_FOLDER for any other folder entry. */
static int entry_ok(const ZipArchive *z, size_t i, const ModCheck *c, char *rel, int *kind, char *why, size_t cap)
{
    const ZipEntry *e = &z->entries[i];
    char clean[ZIP_NAME_MAX + 1];
    if (!modpath_clean(e->name, clean, sizeof clean)) return refuse(why, cap, "%s: the name leaves the mod's folder", e->name);
    if (e->is_special) return refuse(why, cap, "%s is a link or a special file", e->name);
    const Parts p = parts_of(clean);
    *kind = MOD_KIND_SKIP;
    if (litter(&p)) return 1;
    if (!install_path(c, e->name, rel, ZIP_NAME_MAX + 1)) {
        if (c->root[0] != '\0' && strcmp(clean, p.first) == 0 && e->is_dir) return 1;   /* the root folder's own entry */
        return refuse(why, cap, "%s is outside the mod's folder", e->name);
    }
    if (e->is_dir) return *kind = KIND_FOLDER, 1;
    *kind = mod_file_kind(rel);
    if (*kind == MOD_KIND_NONE) return refuse(why, cap, "%s is not a file the game reads", rel);
    return 1;
}

/* whether the file `file` would need the name of a folder the zip makes: a folder entry of that name, or the folder above another entry */
static int folder_named(const char *file, char (*rels)[ZIP_NAME_MAX + 1], const int *kinds, size_t count)
{
    const size_t n = strlen(file);
    for (size_t j = 0; j < count; j++) {
        if (kinds[j] < 0 && kinds[j] != KIND_FOLDER) continue;
        if (kinds[j] == KIND_FOLDER && is(rels[j], file)) return 1;
        if (strlen(rels[j]) > n && rels[j][n] == '/' && strncasecmp(rels[j], file, n) == 0) return 1;
    }
    return 0;
}

static int names_ok(const ZipArchive *z, ModCheck *c, char (*rels)[ZIP_NAME_MAX + 1], int *kinds, char *why, size_t cap)
{
    for (size_t i = 0; i < z->count; i++) {
        if (!entry_ok(z, i, c, rels[i], &kinds[i], why, cap)) return 0;
        if (kinds[i] < 0) continue;
        for (size_t j = 0; j < i; j++)
            if (kinds[j] >= 0 && is(rels[i], rels[j])) return refuse(why, cap, "%s and %s are the same file", rels[j], rels[i]);
    }
    for (size_t i = 0; i < z->count; i++)
        if (kinds[i] >= 0 && folder_named(rels[i], rels, kinds, z->count)) return refuse(why, cap, "%s is both a file and a folder", rels[i]);
    return 1;
}

/* every picture's size read from its header before any is unpacked and read: each within its limit, all of them within the total */
static int pictures_ok(const ZipArchive *z, char (*rels)[ZIP_NAME_MAX + 1], const int *kinds, char *why, size_t cap)
{
    uint64_t total = 0;
    for (size_t i = 0; i < z->count; i++) {
        if (kinds[i] != MOD_KIND_PNG) continue;
        uint8_t *data = NULL;
        size_t size = 0;
        const ZipResult r = zip_extract(z, i, &data, &size);
        if (r != ZIP_OK) return refuse(why, cap, "%s: %s", rels[i], zip_result_text(r));
        uint32_t w, h;
        const int read = png_size(data, size, &w, &h) == PNG_OK;
        free(data);
        if (!read) return refuse(why, cap, "%s is not a readable PNG", rels[i]);
        const int preview = is(rels[i], "preview.png");
        if (preview && (w > MOD_PREVIEW_W || h > MOD_PREVIEW_H))
            return refuse(why, cap, "preview.png is larger than %d x %d", MOD_PREVIEW_W, MOD_PREVIEW_H);
        if (w > MOD_PNG_MAX_SIDE || h > MOD_PNG_MAX_SIDE) return refuse(why, cap, "%s is larger than %d x %d", rels[i], MOD_PNG_MAX_SIDE, MOD_PNG_MAX_SIDE);
        total += (uint64_t)w * h;
        if (total > MOD_PNG_MAX_PIXELS) return refuse(why, cap, "the mod's pictures add up to more than %u pixels", (unsigned)MOD_PNG_MAX_PIXELS);
    }
    return 1;
}

static int contents_ok(const ZipArchive *z, ModCheck *c, char (*rels)[ZIP_NAME_MAX + 1], const int *kinds, char *why, size_t cap)
{
    for (size_t i = 0; i < z->count; i++) {
        if (kinds[i] < 0) continue;
        uint8_t *data = NULL;
        size_t size = 0;
        const ZipResult r = zip_extract(z, i, &data, &size);
        if (r != ZIP_OK) return refuse(why, cap, "%s: %s", rels[i], zip_result_text(r));
        const int good = content_ok((ModFileKind)kinds[i], rels[i], data, size, c, why, cap);
        free(data);
        if (!good) return 0;
        c->entries[c->entry_count].index = i;
        memcpy(c->entries[c->entry_count].path, rels[i], sizeof rels[i]);
        c->entry_count++, c->bytes += size, c->kinds[kinds[i]]++;
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
    int *kinds = calloc(z.count > 0 ? z.count : 1, sizeof *kinds);
    out->entries = calloc(z.count > 0 ? z.count : 1, sizeof *out->entries);
    int ok = rels != NULL && kinds != NULL && out->entries != NULL ? 1 : refuse(why, why_cap, "out of memory");
    ok = ok && names_ok(&z, out, rels, kinds, why, why_cap) && pictures_ok(&z, rels, kinds, why, why_cap) &&
         contents_ok(&z, out, rels, kinds, why, why_cap);
    free(rels), free(kinds);
    zip_close(&z);
    if (!ok) mod_check_free(out);
    return ok;
}

void mod_check_free(ModCheck *c)
{
    if (c == NULL) return;
    free(c->entries);
    const ModCheck empty = { 0 };
    *c = empty;
}
