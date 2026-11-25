/* Pack zip of a tweak (tweak.h): Check, then the files that differ from the game, in a temporary folder, through `mod pack`. */
#include "tweak.h"

#include "formats/exchange.h"
#include "formats/mod_check.h"
#include "formats/png.h"
#include "studio/fsio.h"
#include "studio/mod_pack.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum { PATH = 4096, COLOUR_TEXT_MAX = 32 };

/* the colour of texel i as RGBA */
static void texel(const PngImage *img, size_t i, uint8_t out[4])
{
    if (img->indexed) {
        memcpy(out, img->palette + img->pixels[i] * 3, 3);
        out[3] = 255;
    } else {
        memcpy(out, img->pixels + i * 4, 4);
    }
}

static int one_of(const uint8_t rgb[3], const uint8_t (*set)[3], int n)
{
    for (int i = 0; i < n; ++i)
        if (memcmp(rgb, set[i], 3) == 0) return 1;
    return 0;
}

static int car_class(int v)
{
    static const int CAR[] = { 0, 128, 192, 224, 240, 244, 248, 252, 255 };
    for (size_t k = 0; k < sizeof CAR / sizeof CAR[0]; ++k)
        if (v == CAR[k]) return 1;
    return 0;
}

/* 0 naming the first texel that is no class of the mask's kind; kind: 0 car (a grey level that is a class), 1 dash (a class colour), 2 track (black, white, yellow, red, transparent) */
static int classes_ok(const PngImage *img, int kind, char *bad, size_t cap)
{
    static const uint8_t DASH[][3] = { { 0, 0, 0 }, { 0, 0, 255 }, { 0, 255, 0 }, { 255, 255, 255 }, { 255, 0, 0 }, { 0, 255, 255 } };
    static const uint8_t TRACK[][3] = { { 0, 0, 0 }, { 255, 255, 255 }, { 255, 255, 0 }, { 255, 0, 0 } };
    const size_t n = (size_t)img->width * img->height;
    for (size_t i = 0; i < n; ++i) {
        uint8_t c[4];
        texel(img, i, c);
        int ok;
        if (kind == 0) {
            ok = c[0] == c[1] && c[1] == c[2] && car_class(c[0]);
        } else if (kind == 1) {
            ok = one_of(c, DASH, 6);
        } else {
            ok = c[3] == 0 || one_of(c, TRACK, 4);
        }
        if (!ok) {
            snprintf(bad, cap, "texel (%u,%u) is %d,%d,%d%s", (unsigned)(i % img->width), (unsigned)(i / img->width), c[0], c[1], c[2], kind == 0 ? ", no class of a car mask" : kind == 1 ? ", no class colour of a dash mask" : ", not black, white, yellow, red or transparent");
            return 0;
        }
    }
    return 1;
}

/* dash_color.png carries a colour: a few flat ones, not a picture (a picture could hold the original dash) */
static int few_colours(const PngImage *img, char *bad, size_t cap)
{
    uint8_t seen[COLOUR_TEXT_MAX][3];
    int count = 0;
    const size_t n = (size_t)img->width * img->height;
    for (size_t i = 0; i < n; ++i) {
        uint8_t c[4];
        texel(img, i, c);
        if (c[3] == 0 || one_of(c, (const uint8_t (*)[3])seen, count)) continue;
        if (count == COLOUR_TEXT_MAX) return snprintf(bad, cap, "more than %d colours: a picture, not flat colours", COLOUR_TEXT_MAX), 0;
        memcpy(seen[count++], c, 3);
    }
    return 1;
}

static int check_png(const char *dir, const char *sub, const char *folder, const char *file, int kind, char *report, size_t cap)
{
    char path[PATH], bad[160] = "";
    snprintf(path, sizeof path, "%s/%s/%s/%s", dir, sub, folder, file);
    ExchangeError err;
    size_t size = 0;
    uint8_t *data = fsio_read_file(path, &size, &err);
    PngImage img;
    if (data == NULL || png_read(data, size, &img) != PNG_OK) {
        free(data);
        return snprintf(report, cap, "%s/%s/%s does not read as a PNG", sub, folder, file), 0;
    }
    free(data);
    const int ok = strcmp(file, "dash_color.png") == 0 ? few_colours(&img, bad, sizeof bad) : classes_ok(&img, kind, bad, sizeof bad);
    png_free(img.pixels);
    if (!ok) snprintf(report, cap, "%s/%s/%s: %s: a mask holds classes only, so it cannot carry the game's picture", sub, folder, file, bad);
    return ok;
}

int tweak_check_masks(const char *dir, char *report, size_t cap)
{
    static const char *const SUBS[] = { "car_masks", "dash_masks", "track_masks" };
    for (int s = 0; s < 3; ++s) {
        char sub[PATH];
        snprintf(sub, sizeof sub, "%s/%s", dir, SUBS[s]);
        DIR *d = opendir(sub);
        for (struct dirent *e; d != NULL && (e = readdir(d)) != NULL;) {
            if (e->d_name[0] == '.') continue;
            char folder[PATH];
            snprintf(folder, sizeof folder, "%s/%s", sub, e->d_name);
            DIR *f = opendir(folder);
            for (struct dirent *g; f != NULL && (g = readdir(f)) != NULL;) {
                const size_t n = strlen(g->d_name);
                if (g->d_name[0] == '.') continue;
                if (n > 4 && strcmp(g->d_name + n - 4, ".psd") == 0) {
                    snprintf(report, cap, "%s/%s/%s is a PSD: it holds the original texture in a layer", SUBS[s], e->d_name, g->d_name);
                    closedir(f), closedir(d);
                    return 0;
                }
                if (n > 4 && strcmp(g->d_name + n - 4, ".png") == 0 && !check_png(dir, SUBS[s], e->d_name, g->d_name, s, report, cap)) {
                    closedir(f), closedir(d);
                    return 0;
                }
            }
            if (f != NULL) closedir(f);
        }
        if (d != NULL) closedir(d);
    }
    return 1;
}

static int is_fact(const char *p)
{
    char first[16] = "";
    sscanf(p, "%15s", first);
    return strcmp(first, "tunnel") == 0 || strcmp(first, "snow") == 0 || strcmp(first, "snow_map") == 0;
}

/* the lines of `text` that are facts of the race (tunnel, snow, snow_map) joined, and the text without them in `rest` (both malloc'd) */
static void split_facts(const char *text, char **facts, char **rest)
{
    const size_t n = strlen(text);
    *facts = calloc(n + 1, 1);
    *rest = calloc(n + 1, 1);
    if (*facts == NULL || *rest == NULL) return;
    for (const char *p = text; *p != '\0';) {
        const char *end = strchr(p, '\n');
        const size_t len = end != NULL ? (size_t)(end - p) + 1 : strlen(p);
        strncat(is_fact(p) ? *facts : *rest, p, len);
        p += len;
    }
}

/* the first line of `a` that `b` does not have at the same place; 0 when they are the same. `line` gets its text. */
static int first_difference(const char *a, const char *b, char *line, size_t cap)
{
    int n = 1;
    for (; *a != '\0' || *b != '\0'; ++n) {
        const char *ae = strchr(a, '\n'), *be = strchr(b, '\n');
        const size_t al = ae != NULL ? (size_t)(ae - a) : strlen(a), bl = be != NULL ? (size_t)(be - b) : strlen(b);
        if (al != bl || strncmp(a, b, al) != 0) {
            snprintf(line, cap, "%.*s", (int)(al < cap - 1 ? al : cap - 1), al > 0 ? a : b);
            return n;
        }
        a += al + (ae != NULL), b += bl + (be != NULL);
    }
    return 0;
}

static char *read_all(const char *path)
{
    ExchangeError err;
    size_t size = 0;
    uint8_t *data = fsio_read_file(path, &size, &err);
    if (data == NULL) return NULL;
    char *text = malloc(size + 1);
    if (text != NULL) memcpy(text, data, size), text[size] = '\0';
    free(data);
    return text;
}

/* a track light file as a zip may carry it: without the lines that are facts of the race (the game keeps its own); refused, naming the line, when the tweak changed one */
static int stage_light(const char *dir, const char *base_data, const char *tmp, const char *rel, char *report, size_t cap)
{
    char from[PATH], to[PATH], line[160];
    snprintf(from, sizeof from, "%s/%s", dir, rel);
    snprintf(to, sizeof to, "%s/%s", tmp, rel);
    char *mine = read_all(from), *theirs, *mine_facts = NULL, *mine_rest = NULL, *their_facts = NULL, *their_rest = NULL;
    snprintf(from, sizeof from, "%s/%s", base_data, rel);
    theirs = read_all(from);
    int ok = mine != NULL;
    if (ok) {
        split_facts(mine, &mine_facts, &mine_rest);
        split_facts(theirs != NULL ? theirs : "", &their_facts, &their_rest);
        ok = mine_facts != NULL && mine_rest != NULL && their_facts != NULL;
    }
    if (!ok) snprintf(report, cap, "cannot read %s", rel);
    const int at = ok ? first_difference(mine_facts, their_facts, line, sizeof line) : 0;
    if (at > 0) {
        snprintf(report, cap, "%s: the fact \"%s\" is not the game's: it changes the race, so it goes to the game repo only, not in a zip", rel, line);
        ok = 0;
    }
    if (ok) {
        ExchangeError err;
        if (!fsio_write_file(to, (const uint8_t *)mine_rest, strlen(mine_rest), 1, &err)) snprintf(report, cap, "cannot stage %s", rel), ok = 0;
    }
    free(mine), free(theirs), free(mine_facts), free(mine_rest), free(their_facts), free(their_rest);
    return ok;
}

static int stage_file(const char *dir, const char *base_data, const char *tmp, const char *rel, char *report, size_t cap)
{
    if (strncmp(rel, "track_light/", 12) == 0) return stage_light(dir, base_data, tmp, rel, report, cap);
    char from[PATH], to[PATH];
    snprintf(from, sizeof from, "%s/%s", dir, rel);
    snprintf(to, sizeof to, "%s/%s", tmp, rel);
    ExchangeError err;
    size_t size = 0;
    uint8_t *data = fsio_read_file(from, &size, &err);
    const int ok = data != NULL && fsio_write_file(to, data, size, 1, &err);
    free(data);
    if (!ok) snprintf(report, cap, "cannot stage %s", rel);
    return ok;
}

int tweak_pack(const char *dir, const char *base_data, const char *zip, int force, char *report, size_t cap)
{
    static TweakStatus status;
    if (!tweak_check(dir, report, cap) || !tweak_check_masks(dir, report, cap)) return 0;
    if (!tweak_status(dir, base_data, &status, report, cap)) return 0;
    char tmp[PATH];
    const char *root = getenv("TMPDIR");
    snprintf(tmp, sizeof tmp, "%s/tweak-pack-XXXXXX", root != NULL && root[0] != '\0' ? root : "/tmp");
    if (mkdtemp(tmp) == NULL) return snprintf(report, cap, "cannot make a folder to pack from in %s", tmp), 0;
    int files = 0, colors = 0, ok = stage_file(dir, base_data, tmp, "mod.ini", report, cap);
    for (int i = 0; i < status.count && ok; ++i) {
        if (status.item[i].state == TWEAK_SAME) continue;
        if (strcmp(status.item[i].file, "colors") == 0) { colors = 1; continue; }
        ok = stage_file(dir, base_data, tmp, status.item[i].file, report, cap);
        ++files;
    }
    ExchangeError err = { 0 };
    if (ok && files == 0) ok = 0, snprintf(report, cap, "nothing differs from the game%s: no zip made", colors ? " but the colors, which go to the game repo only" : "");
    if (ok) {
        char *argv[] = { "mod", "pack", tmp, (char *)zip, "--force" };
        ok = mod_command(force ? 5 : 4, argv, &err);
        if (!ok) snprintf(report, cap, "%s", err.message);
        else snprintf(report, cap, "%d file%s packed into %s%s", files, files == 1 ? "" : "s", zip, colors ? " (the colors go to the game repo only)" : "");
    }
    fsio_remove_tree(tmp);
    return ok;
}
