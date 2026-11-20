#include "car_edit.h"

#include "formats/car_piece.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct {
    const char *key, *label;
    size_t cap;   /* the longest value in bytes; 0 for a word */
} FieldInfo;

static const FieldInfo FIELDS[CE_FIELDS] = {
    { "version", "Version", MOD_INI_VERSION_MAX },
    { "year", "Year", MOD_INI_FIGURE_MAX },
    { "power", "Power", MOD_INI_FIGURE_MAX },
    { "weight", "Weight", MOD_INI_FIGURE_MAX },
    { "make", "Make", MOD_INI_MAKE_MAX },
    { "model", "Model", MOD_INI_MAKE_MAX },
    { "country", "Country", MOD_INI_MAKE_MAX },
    { "category", "Category", MOD_INI_CATEGORY_MAX },
    { "author", "Author", MOD_INI_AUTHOR_MAX },
    { "description", "Description", MOD_INI_DESCRIPTION_MAX },
    { "abs", "ABS", 0 },
    { "traction", "Traction control", 0 },
    { "sound_from", "Sound from", 0 },
};

int car_edit_sound_words(const char **out, int cap)
{
    int n = 0;
    for (int m = 0; m < 16 && n < cap; m++) {   /* the models that have a word: 0..7 and 11 (8..10 are the exe's copies) */
        const char *word = car_piece_sound_word_of_model(m);
        if (word != NULL) out[n++] = word;
    }
    return n;
}

const char *car_edit_key(int field)
{
    return field == CE_PAINTS ? "paints" : field >= 0 && field < CE_FIELDS ? FIELDS[field].key : "";
}

const char *car_edit_label(int field)
{
    return field == CE_PAINTS ? "Colors" : field == CE_CARD ? "Card" : field >= 0 && field < CE_FIELDS ? FIELDS[field].label : "";
}

size_t car_edit_cap(int field)
{
    return field >= 0 && field < CE_FIELDS ? FIELDS[field].cap : 0;
}

/* `src` without the spaces and tabs round it, into `dst` */
static void trimmed(char *dst, size_t cap, const char *src)
{
    while (*src == ' ' || *src == '\t') src++;
    size_t n = strlen(src);
    while (n > 0 && (src[n - 1] == ' ' || src[n - 1] == '\t')) n--;
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static int paints_equal(const CarEditState *a, const CarEditState *b)
{
    if (a->paint_count != b->paint_count) return 0;
    for (int i = 0; i < a->paint_count; i++)
        if (strcmp(a->paints[i].name, b->paints[i].name) != 0 || memcmp(a->paints[i].rgb, b->paints[i].rgb, 3) != 0) return 0;
    return 1;
}

static int field_equal(const CarEditState *a, const CarEditState *b, int f)
{
    char x[CE_VALUE_MAX], y[CE_VALUE_MAX];
    trimmed(x, sizeof x, a->value[f]);
    trimmed(y, sizeof y, b->value[f]);
    return strcmp(x, y) == 0;
}

int car_edit_modified(const CarEdit *e)
{
    for (int f = 0; f < CE_FIELDS; f++)
        if (!field_equal(&e->now, &e->saved, f)) return 1;
    return !paints_equal(&e->now, &e->saved);
}

static void fill_state(CarEditState *s, const ModInfo *m)
{
    memset(s, 0, sizeof *s);
    snprintf(s->value[CE_VERSION], CE_VALUE_MAX, "%s", m->version);
    snprintf(s->value[CE_YEAR], CE_VALUE_MAX, "%s", m->year);
    snprintf(s->value[CE_POWER], CE_VALUE_MAX, "%s", m->power);
    snprintf(s->value[CE_WEIGHT], CE_VALUE_MAX, "%s", m->weight);
    snprintf(s->value[CE_MAKE], CE_VALUE_MAX, "%s", m->make);
    snprintf(s->value[CE_MODEL], CE_VALUE_MAX, "%s", m->model);
    snprintf(s->value[CE_COUNTRY], CE_VALUE_MAX, "%s", m->country);
    snprintf(s->value[CE_CATEGORY], CE_VALUE_MAX, "%s", m->category);
    snprintf(s->value[CE_AUTHOR], CE_VALUE_MAX, "%s", m->author);
    snprintf(s->value[CE_DESCRIPTION], CE_VALUE_MAX, "%s", m->description);
    snprintf(s->value[CE_ABS], CE_VALUE_MAX, "%s", m->assist_abs);
    snprintf(s->value[CE_TRACTION], CE_VALUE_MAX, "%s", m->assist_traction);
    snprintf(s->value[CE_SOUND_FROM], CE_VALUE_MAX, "%s", m->sound_from);
    s->paint_count = m->paint_count;
    memcpy(s->paints, m->paints, sizeof(ModPaint) * (size_t)m->paint_count);
}

int car_edit_open(CarEdit *e, const char *path, char *why, size_t why_cap)
{
    memset(e, 0, sizeof *e);
    if (path == NULL || strlen(path) >= sizeof e->path) return snprintf(why, why_cap, "the card's path is too long"), 0;
    FILE *f = fopen(path, "rb");
    if (f == NULL) return snprintf(why, why_cap, "cannot read %s: %s", path, strerror(errno)), 0;
    e->original_len = fread(e->original, 1, MOD_INI_MAX_SIZE, f);
    const int over = fgetc(f) != EOF;
    fclose(f);
    ModInfo info;
    char reason[200];
    if (over) return snprintf(why, why_cap, "mod.ini is larger than %d bytes", MOD_INI_MAX_SIZE), 0;
    if (!mod_ini_parse((const uint8_t *)e->original, e->original_len, &info, reason, sizeof reason)) return snprintf(why, why_cap, "%s", reason), 0;
    if (info.type != MOD_TYPE_CAR) return snprintf(why, why_cap, "mod.ini is not a car's card (type=car)"), 0;
    snprintf(e->path, sizeof e->path, "%s", path);
    fill_state(&e->now, &info);
    e->saved = e->now;
    e->undo = calloc(CE_UNDO_MAX, sizeof *e->undo);
    e->redo = calloc(CE_UNDO_MAX, sizeof *e->redo);
    if (e->undo == NULL || e->redo == NULL) {
        car_edit_free(e);
        return snprintf(why, why_cap, "out of memory"), 0;
    }
    e->run = -1;
    return 1;
}

void car_edit_free(CarEdit *e)
{
    free(e->undo);
    free(e->redo);
    e->undo = e->redo = NULL;
    e->undo_count = e->redo_count = 0;
}

/* the state before an edit goes on the undo stack, unless the edit goes on the one before it (`run`: the same field typed in, the same colour dragged) */
static void before_edit(CarEdit *e, int run)
{
    e->redo_count = 0;
    if (run >= 0 && run == e->run && e->undo_count > 0) return;
    if (e->undo_count == CE_UNDO_MAX) {
        memmove(e->undo, e->undo + 1, sizeof *e->undo * (CE_UNDO_MAX - 1));
        e->undo_count--;
    }
    e->undo[e->undo_count++] = e->now;
    e->run = run;
}

enum { RUN_FIELD = 0, RUN_RGB = 100, RUN_NAME = 200 };

int car_edit_set(CarEdit *e, int field, const char *value)
{
    if (field < 0 || field >= CE_FIELDS || value == NULL || strlen(value) >= CE_VALUE_MAX || strcmp(e->now.value[field], value) == 0) return 0;
    before_edit(e, RUN_FIELD + field);
    snprintf(e->now.value[field], CE_VALUE_MAX, "%s", value);
    return 1;
}

int car_edit_paint_add(CarEdit *e, const char *name, const uint8_t rgb[3])
{
    if (e->now.paint_count >= MOD_INI_PAINTS_MAX || name == NULL) return 0;
    before_edit(e, -1);
    ModPaint *p = &e->now.paints[e->now.paint_count++];
    memset(p, 0, sizeof *p);
    snprintf(p->name, sizeof p->name, "%s", name);
    memcpy(p->rgb, rgb, 3);
    return 1;
}

int car_edit_paint_remove(CarEdit *e, int index)
{
    if (index < 0 || index >= e->now.paint_count) return 0;
    before_edit(e, -1);
    memmove(&e->now.paints[index], &e->now.paints[index + 1], sizeof(ModPaint) * (size_t)(e->now.paint_count - index - 1));
    e->now.paint_count--;
    memset(&e->now.paints[e->now.paint_count], 0, sizeof(ModPaint));
    return 1;
}

int car_edit_paint_move(CarEdit *e, int index, int delta)
{
    const int to = index + delta;
    if ((delta != -1 && delta != 1) || index < 0 || index >= e->now.paint_count || to < 0 || to >= e->now.paint_count) return 0;
    before_edit(e, -1);
    const ModPaint moved = e->now.paints[index];
    e->now.paints[index] = e->now.paints[to];
    e->now.paints[to] = moved;
    return 1;
}

int car_edit_paint_name(CarEdit *e, int index, const char *name)
{
    if (index < 0 || index >= e->now.paint_count || name == NULL || strlen(name) >= sizeof e->now.paints[0].name || strcmp(e->now.paints[index].name, name) == 0) return 0;
    before_edit(e, RUN_NAME + index);
    snprintf(e->now.paints[index].name, sizeof e->now.paints[index].name, "%s", name);
    return 1;
}

int car_edit_paint_rgb(CarEdit *e, int index, const uint8_t rgb[3])
{
    if (index < 0 || index >= e->now.paint_count || memcmp(e->now.paints[index].rgb, rgb, 3) == 0) return 0;
    before_edit(e, RUN_RGB + index);
    memcpy(e->now.paints[index].rgb, rgb, 3);
    return 1;
}

int car_edit_can_undo(const CarEdit *e)
{
    return e->undo_count > 0;
}

int car_edit_can_redo(const CarEdit *e)
{
    return e->redo_count > 0;
}

int car_edit_undo(CarEdit *e)
{
    if (e->undo_count == 0) return 0;
    if (e->redo_count < CE_UNDO_MAX) e->redo[e->redo_count++] = e->now;
    e->now = e->undo[--e->undo_count];
    e->run = -1;
    return 1;
}

int car_edit_redo(CarEdit *e)
{
    if (e->redo_count == 0) return 0;
    if (e->undo_count < CE_UNDO_MAX) e->undo[e->undo_count++] = e->now;
    e->now = e->redo[--e->redo_count];
    e->run = -1;
    return 1;
}

/* ---- the card as Save writes it ---- */

/* `text` with `key` set to `value` ("" removes it), `*len` bytes of `a`, into `b` */
static int apply(const char *text, size_t len, const char *key, const char *value, char *out, size_t *out_len, char *why, size_t why_cap)
{
    return mod_ini_set(text, len, key, value, out, MOD_INI_MAX_SIZE + 1, out_len, NULL, why, why_cap);
}

int car_edit_render(const CarEdit *e, char *out, size_t cap, size_t *len, char *why, size_t why_cap)
{
    char a[MOD_INI_MAX_SIZE + 1], b[MOD_INI_MAX_SIZE + 1];
    memcpy(a, e->original, e->original_len);
    size_t n = e->original_len;
    for (int f = 0; f < CE_FIELDS; f++) {
        if (field_equal(&e->now, &e->saved, f)) continue;
        char v[CE_VALUE_MAX];
        trimmed(v, sizeof v, e->now.value[f]);
        size_t m = 0;
        if (!apply(a, n, FIELDS[f].key, v, b, &m, why, why_cap)) return 0;
        memcpy(a, b, m);
        n = m;
    }
    if (!paints_equal(&e->now, &e->saved)) {
        char line[MOD_INI_PAINTS_MAX * (MOD_INI_PAINT_NAME_MAX + 8) + 1] = "";
        size_t m = 0;
        if (e->now.paint_count > 0 && !mod_ini_paints_format(e->now.paints, e->now.paint_count, line, sizeof line, why, why_cap)) return 0;
        if (!apply(a, n, "paints", line, b, &m, why, why_cap)) return 0;
        memcpy(a, b, m);
        n = m;
    }
    if (n + 1 > cap) return snprintf(why, why_cap, "no room for the card"), 0;
    memcpy(out, a, n);
    out[n] = '\0';
    if (len != NULL) *len = n;
    return 1;
}

/* ---- the problems ---- */

static const char *without_prefix(const char *why)
{
    static const char PREFIX[] = "mod.ini: ";
    return strncmp(why, PREFIX, sizeof PREFIX - 1) == 0 ? why + sizeof PREFIX - 1 : why;
}

static int ascii_only(const char *s)
{
    for (; *s != '\0'; s++)
        if ((unsigned char)*s >= 0x80) return 0;
    return 1;
}

/* the sentence about the allowed words of sound_from: "supra, diablo, ..." */
static void sound_words(char *out, size_t cap)
{
    const char *list[16];
    const int n = car_edit_sound_words(list, 16);
    out[0] = '\0';
    for (int i = 0; i < n; i++) {
        const size_t used = strlen(out);
        snprintf(out + used, cap - used, "%s%s", i > 0 ? ", " : "", list[i]);
    }
}

/* the problem of field `f` as it is on screen, in `text`; 0 when it has none */
static int field_problem(const CarEdit *e, int f, char *text)
{
    char v[CE_VALUE_MAX];
    trimmed(v, sizeof v, e->now.value[f]);
    const FieldInfo *fi = &FIELDS[f];
    if (f == CE_ABS || f == CE_TRACTION) {
        if (car_piece_assist_key_ok(v)) return 0;
        snprintf(text, CE_PROBLEM_TEXT, "%s: \"%.40s\" is not yes or no. The card takes yes, no or nothing (the car's own file then decides). Save is blocked until it is one of them.", fi->label, v);
        return 1;
    }
    if (f == CE_SOUND_FROM) {
        char words[96];
        sound_words(words, sizeof words);
        if (v[0] == '\0') snprintf(text, CE_PROBLEM_TEXT, "%s: not set. The game refuses a car with no original car to take its engine sound from (%s). Save is blocked until one is chosen.", fi->label, words);
        else if (car_piece_sound_model_of_word(v) < 0) snprintf(text, CE_PROBLEM_TEXT, "%s: \"%.40s\" is not one of %s. The game would refuse the car. Save is blocked until it is.", fi->label, v, words);
        else return 0;
        return 1;
    }
    if (f == CE_VERSION && v[0] == '\0') {
        snprintf(text, CE_PROBLEM_TEXT, "%s: empty. Every card needs a version. Save is blocked until it has one.", fi->label);
        return 1;
    }
    const size_t n = strlen(v);
    if (n > fi->cap) {
        snprintf(text, CE_PROBLEM_TEXT, "%s: %zu %s; the card keeps up to %zu. Save is blocked until it is %zu or fewer.", fi->label, n, ascii_only(v) ? "characters" : "bytes", fi->cap, fi->cap);
        return 1;
    }
    if (strchr(v, '\n') != NULL || strchr(v, '\r') != NULL) {
        snprintf(text, CE_PROBLEM_TEXT, "%s: holds a line break, and a card line is one line. Save is blocked until it is removed.", fi->label);
        return 1;
    }
    if (field_equal(&e->now, &e->saved, f)) return 0;
    /* the game's own reader has the last word: the card with only this change must still parse */
    char a[MOD_INI_MAX_SIZE + 1], reason[200];
    size_t m = 0;
    if (!apply(e->original, e->original_len, fi->key, v, a, &m, reason, sizeof reason)) return 0;   /* a card too long is the card's problem */
    ModInfo info;
    if (mod_ini_parse((const uint8_t *)a, m, &info, reason, sizeof reason)) return 0;
    snprintf(text, CE_PROBLEM_TEXT, "%s: %.150s. Save is blocked until this is fixed.", fi->label, without_prefix(reason));
    return 1;
}

static void add_problem(CarEditProblem *out, int *n, int cap, int field, int paint, const char *text)
{
    if (*n >= cap) return;
    out[*n].field = field;
    out[*n].paint = paint;
    snprintf(out[*n].text, sizeof out[*n].text, "%s", text);
    (*n)++;
}

int car_edit_problems(const CarEdit *e, CarEditProblem *out, int cap)
{
    int n = 0;
    char text[CE_PROBLEM_TEXT];
    for (int f = 0; f < CE_FIELDS; f++)
        if (field_problem(e, f, text)) add_problem(out, &n, cap, f, -1, text);
    int paints_bad = 0;
    for (int i = 0; i < e->now.paint_count && !paints_bad; i++) {
        char line[128], reason[160];
        if (!mod_ini_paints_format(&e->now.paints[i], 1, line, sizeof line, reason, sizeof reason)) {
            const char *why = without_prefix(reason);
            if (strncmp(why, "paints: ", 8) == 0) why += 8;
            snprintf(text, sizeof text, "Color %d: %.150s. Save is blocked until this is fixed.", i + 1, why);
            add_problem(out, &n, cap, CE_PAINTS, i, text);
            paints_bad = 1;
        }
        for (int k = 0; k < i && !paints_bad; k++) {
            if (strcasecmp(e->now.paints[k].name, e->now.paints[i].name) != 0) continue;
            snprintf(text, sizeof text, "Color %d: \"%.24s\" is also the name of color %d (names are compared without case). Save is blocked until each color has its own name.", i + 1,
                     e->now.paints[i].name, k + 1);
            add_problem(out, &n, cap, CE_PAINTS, i, text);
            paints_bad = 1;
        }
    }
    if (e->now.paint_count > MOD_INI_PAINTS_MAX && !paints_bad) {
        snprintf(text, sizeof text, "Colors: %d; the card keeps up to %d. Save is blocked until some are removed.", e->now.paint_count, MOD_INI_PAINTS_MAX);
        add_problem(out, &n, cap, CE_PAINTS, -1, text);
        paints_bad = 1;
    }
    if (n == 0) {   /* nothing wrong with a field: can the card hold the changes? */
        char card[MOD_INI_MAX_SIZE + 1], reason[200];
        if (!car_edit_render(e, card, sizeof card, NULL, reason, sizeof reason)) {
            snprintf(text, sizeof text, "The card would be larger than %d bytes with these changes. Save is blocked until it is shorter.", MOD_INI_MAX_SIZE);
            add_problem(out, &n, cap, CE_CARD, -1, text);
        }
    }
    return n;
}

int car_edit_save(CarEdit *e, char *why, size_t why_cap)
{
    CarEditProblem p[CE_PROBLEMS_MAX];
    if (car_edit_problems(e, p, CE_PROBLEMS_MAX) > 0) return snprintf(why, why_cap, "%s", p[0].text), 0;
    if (!car_edit_modified(e)) return 1;
    char card[MOD_INI_MAX_SIZE + 1];
    size_t len = 0;
    if (!car_edit_render(e, card, sizeof card, &len, why, why_cap)) return 0;
    ModInfo info;
    char reason[200];
    if (!mod_ini_parse((const uint8_t *)card, len, &info, reason, sizeof reason) || info.type != MOD_TYPE_CAR) {   /* what is written must be a card the game takes */
        return snprintf(why, why_cap, "the new card is not one the game would take (%s): nothing was written", reason), 0;
    }
    char tmp[sizeof e->path + 8];
    snprintf(tmp, sizeof tmp, "%s.tmp", e->path);
    FILE *f = fopen(tmp, "wb");
    int ok = f != NULL && fwrite(card, 1, len, f) == len;
    if (f != NULL) ok &= fclose(f) == 0;
    if (ok) ok = rename(tmp, e->path) == 0;
    if (!ok) {
        const int err = errno;
        remove(tmp);
        return snprintf(why, why_cap, "cannot write mod.ini: %s", strerror(err)), 0;
    }
    memcpy(e->original, card, len + 1);
    e->original_len = len;
    e->saved = e->now;
    e->run = -1;
    return 1;
}

int car_edit_set_by_text(CarEdit *e, const char *key_value)
{
    const char *eq = key_value != NULL ? strchr(key_value, '=') : NULL;
    if (eq == NULL) return 0;
    const size_t klen = (size_t)(eq - key_value);
    for (int f = 0; f < CE_FIELDS; f++) {
        if (strlen(FIELDS[f].key) != klen || memcmp(FIELDS[f].key, key_value, klen) != 0) continue;
        car_edit_set(e, f, eq + 1);
        return 1;
    }
    return 0;
}

/* ---- the version one step up (owner test R2) ---- */

int car_edit_version_next(const char *version, char *out, size_t cap)
{
    if (out == NULL || cap == 0) return 0;
    out[0] = '\0';
    if (version == NULL) return 0;
    const char *start = version, *end = version + strlen(version);
    while (*start == ' ' || *start == '\t') start++;
    while (end > start && (end[-1] == ' ' || end[-1] == '\t')) end--;
    const char *part[3];
    size_t len[3];
    int parts = 0;
    for (const char *p = start; p < end;) {   /* digits, a dot, digits ...: at most three parts, none empty */
        const char *q = p;
        while (q < end && *q >= '0' && *q <= '9') q++;
        if (q == p || parts == 3) return 0;
        part[parts] = p;
        len[parts++] = (size_t)(q - p);
        if (q == end) break;
        if (*q != '.' || q + 1 == end) return 0;
        p = q + 1;
    }
    if (parts == 0) return 0;
    char z[MOD_INI_VERSION_MAX + 2];   /* the third part plus one, as text: no number is too long to add to */
    size_t zn = 0;
    if (parts == 3) {
        if (len[2] > MOD_INI_VERSION_MAX) return 0;
        memcpy(z, part[2], len[2]);
        zn = len[2];
        size_t i = zn;
        while (i > 0 && z[i - 1] == '9') z[--i] = '0';
        if (i > 0) {
            z[i - 1]++;
        } else {   /* all nines: one more digit in front */
            memmove(z + 1, z, zn++);
            z[0] = '1';
        }
    } else {
        z[zn++] = '1';
    }
    z[zn] = '\0';
    const int n = snprintf(out, cap, "%.*s.%.*s.%s", (int)len[0], part[0], parts > 1 ? (int)len[1] : 1, parts > 1 ? part[1] : "0", z);
    if (n < 0 || (size_t)n >= cap || n > MOD_INI_VERSION_MAX) {
        out[0] = '\0';
        return 0;
    }
    return 1;
}
