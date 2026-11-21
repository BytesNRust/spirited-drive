/* See car_physics_from.h. */
#include "car_physics_from.h"

#include "formats/car_piece.h"
#include "formats/exchange.h"
#include "formats/pbs.h"
#include "formats/pdn.h"
#include "formats/slots.h"
#include "studio/fsio.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { RACE_CARS = 8, WARRIOR_SLOT = 8, PATH = 1024 };

static const char *const WARRIOR = "warrior";

int car_physics_words(const char **out, int cap)
{
    int n = 0;
    for (int m = 0; m < RACE_CARS && n < cap; ++m)
        if (car_piece_word_of_model(m) != NULL) out[n++] = car_piece_word_of_model(m);
    if (n < cap) out[n++] = WARRIOR;
    return n;
}

const char *car_physics_stem(const char *word)
{
    if (word == NULL) return NULL;
    if (strcmp(word, WARRIOR) == 0) return slot_stems(SLOT_CAR)[WARRIOR_SLOT];
    const int model = car_piece_model_of_word(word);
    return model >= 0 && model < RACE_CARS ? slot_stems(SLOT_CAR)[model] : NULL;
}

static int refuse(char *why, size_t cap, const char *text, const char *detail)
{
    snprintf(why, cap, "%s%s%s", text, detail[0] != '\0' ? ": " : "", detail);
    return 0;
}

/* 1 when the row called `name` of the PBS (pdn 0) or the PDN (pdn 1) is the game car's to give: the kit keeps what is its own */
static int copies(const char *name, int pdn)
{
    if (pdn) return strcmp(name, "mass") == 0 || strcmp(name, "ai_speed") == 0 || strcmp(name, "factor") == 0;
    static const char *const OWN[] = { "half_length", "half_width", "unknown_334", "unknown_338", "checksum" };
    for (size_t i = 0; i < sizeof OWN / sizeof OWN[0]; ++i)
        if (strcmp(name, OWN[i]) == 0) return 0;
    return 1;
}

/* the line at `p` (up to its newline), its offset and name fields in `key` (a copy: "0x2e4\thalf_length"); 0 for a comment or a blank line */
static int row_key(const char *p, const char *end, char *key, size_t cap)
{
    if (p >= end || *p == '#' || *p == '\n' || *p == '\r') return 0;
    const char *t = memchr(p, '\t', (size_t)(end - p));
    const char *u = t != NULL ? memchr(t + 1, '\t', (size_t)(end - t - 1)) : NULL;
    if (u == NULL || (size_t)(u - p) >= cap) return 0;
    memcpy(key, p, (size_t)(u - p));
    key[u - p] = '\0';
    return 1;
}

/* the line of `text` whose offset and name are `key`: its start and its length without the newline; 0 when there is none */
static int find_row(const char *text, size_t size, const char *key, const char **line, size_t *len)
{
    const char *end = text + size;
    for (const char *p = text; p < end;) {
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        const char *stop = nl != NULL ? nl : end;
        char k[96];
        if (row_key(p, stop, k, sizeof k) && strcmp(k, key) == 0) {
            *line = p, *len = (size_t)(stop - p);
            while (*len > 0 && (*line)[*len - 1] == '\r') --*len;
            return 1;
        }
        p = nl != NULL ? nl + 1 : end;
    }
    return 0;
}

/* `kit_text` with each row the game car gives replaced by the game car's row (same offset and name); `*changed` counts the rows whose line differs. malloc'd, NULL on no memory. */
static char *merge(const char *kit_text, size_t kit_size, const char *src_text, size_t src_size, int pdn, size_t *out_size, int *changed)
{
    char *out = malloc(kit_size + src_size + 1);
    if (out == NULL) return NULL;
    size_t used = 0;
    const char *end = kit_text + kit_size;
    *changed = 0;
    for (const char *p = kit_text; p < end;) {
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        const char *stop = nl != NULL ? nl : end;
        size_t keep = (size_t)(stop - p);
        const char *from = p;
        char key[96];
        if (row_key(p, stop, key, sizeof key) && copies(strchr(key, '\t') + 1, pdn)) {
            const char *row;
            size_t row_len;
            size_t kit_len = keep;
            while (kit_len > 0 && p[kit_len - 1] == '\r') --kit_len;
            if (find_row(src_text, src_size, key, &row, &row_len)) {
                if (row_len != kit_len || memcmp(row, p, row_len) != 0) ++*changed;
                from = row, keep = row_len;
            }
        }
        memcpy(out + used, from, keep), used += keep;
        if (nl != NULL) out[used++] = '\n';
        p = nl != NULL ? nl + 1 : end;
    }
    *out_size = used;
    return out;
}

/* the PBS (pdn 0) or the PDN (pdn 1) of `stem` in the game's files as the tsv text `car kit` writes; malloc'd, NULL with `why` */
static char *game_table(const char *cd, const char *stem, int pdn, size_t *size, char *why, size_t cap)
{
    char paths[CAR_PIECE_FILES][CAR_PIECE_PATH], real[PATH];
    car_piece_paths(stem, paths);
    const char *rel = paths[pdn ? 1 : 2];
    ExchangeError err = { { 0 } };
    if (!fsio_find_ci(cd, rel, real, sizeof real)) return refuse(why, cap, "no such file in the game's files", rel), NULL;
    size_t file_size = 0;
    uint8_t *file = fsio_read_file(real, &file_size, &err);
    if (file == NULL) return refuse(why, cap, rel, err.message), NULL;
    uint8_t record[PBS_SIZE > PDN_SIZE ? PBS_SIZE : PDN_SIZE];
    FileSet set;
    fileset_init(&set);
    const int ok = (pdn ? pdn_read(record, file, file_size) && pdn_export(record, &set, &err) : pbs_read(record, file, file_size) && pbs_export(record, &set, &err)) && set.count > 0;
    free(file);
    char *text = NULL;
    if (ok && (text = malloc(set.file[0].size + 1)) != NULL) memcpy(text, set.file[0].data, set.file[0].size), *size = set.file[0].size;
    if (text == NULL) refuse(why, cap, rel, ok ? "out of memory" : err.message[0] != '\0' ? err.message : "is not a car file");
    fileset_free(&set);
    return text;
}

static char *read_text(const char *path, size_t *size, char *why, size_t cap)
{
    ExchangeError err = { { 0 } };
    uint8_t *bytes = fsio_read_file(path, size, &err);
    if (bytes == NULL) return refuse(why, cap, "this kit has no such file (a kit without it keeps the game's own)", path), NULL;
    return (char *)bytes;
}

/* writes `text` to "<path>.new" */
static int write_new(const char *path, const char *text, size_t size, char *why, size_t cap)
{
    char tmp[PATH + 8];
    ExchangeError err = { { 0 } };
    snprintf(tmp, sizeof tmp, "%s.new", path);
    return fsio_write_file(tmp, (const uint8_t *)text, size, 1, &err) || refuse(why, cap, "cannot write", err.message);
}

/* "<path>.new" over `path`; a "<path>.new" that is left is taken away */
static int put_in_place(const char *path, char *why, size_t cap)
{
    char tmp[PATH + 8];
    snprintf(tmp, sizeof tmp, "%s.new", path);
    if (rename(tmp, path) == 0) return 1;
    remove(tmp);
    return refuse(why, cap, "cannot replace", path);
}

int car_physics_from(const char *cd, const char *kit, const char *word, int *specs_rows, int *pdn_rows, char *why, size_t why_cap)
{
    why[0] = '\0';
    const char *stem = car_physics_stem(word);
    if (stem == NULL) return refuse(why, why_cap, "not one of the cars Physics from offers", word != NULL ? word : "");
    if (cd[0] == '\0') return refuse(why, why_cap, "the game's files are not found", "Physics from reads one of its cars");
    char kit_specs[PATH], kit_pdn[PATH];
    snprintf(kit_specs, sizeof kit_specs, "%s/specs/specs.tsv", kit), snprintf(kit_pdn, sizeof kit_pdn, "%s/specs/pdn.tsv", kit);
    size_t ks = 0, kp = 0, gs = 0, gp = 0, ms = 0, mp = 0;
    char *kit_s = read_text(kit_specs, &ks, why, why_cap), *kit_p = kit_s != NULL ? read_text(kit_pdn, &kp, why, why_cap) : NULL;
    char *game_s = kit_p != NULL ? game_table(cd, stem, 0, &gs, why, why_cap) : NULL, *game_p = game_s != NULL ? game_table(cd, stem, 1, &gp, why, why_cap) : NULL;
    char *new_s = game_p != NULL ? merge(kit_s, ks, game_s, gs, 0, &ms, specs_rows) : NULL, *new_p = new_s != NULL ? merge(kit_p, kp, game_p, gp, 1, &mp, pdn_rows) : NULL;
    int ok = new_p != NULL;
    if (game_p != NULL && !ok) refuse(why, why_cap, "out of memory", "");
    ok = ok && write_new(kit_specs, new_s, ms, why, why_cap) && write_new(kit_pdn, new_p, mp, why, why_cap);   // both are whole before either goes in place
    ok = ok && put_in_place(kit_specs, why, why_cap) && put_in_place(kit_pdn, why, why_cap);
    if (!ok && why[0] != '\0') {
        char tmp[PATH + 8];
        snprintf(tmp, sizeof tmp, "%s.new", kit_specs), remove(tmp);
        snprintf(tmp, sizeof tmp, "%s.new", kit_pdn), remove(tmp);
    }
    free(kit_s), free(kit_p), free(game_s), free(game_p), free(new_s), free(new_p);
    return ok;
}
