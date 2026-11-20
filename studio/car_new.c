#include "studio/car_new.h"
#include "studio/car_import.h"
#include "studio/car_resize.h"
#include "studio/car_texture.h"
#include "studio/car_view.h"

#include "formats/car_piece.h"
#include "formats/mod_ini.h"
#include "formats/slots.h"
#include "studio/car_card.h"
#include "studio/fsio.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define USAGE "usage: tnfs-studio car new --template CAR --id ID --stem STEM --out DIR [--name NAME] [--author TEXT] [--description TEXT] [--data DIR] [--port-data DIR] [--force]"

enum { RACE_CARS = 8 };

typedef struct {
    const char *template, *id, *stem, *out, *name, *author, *description, *data, *port;
    int force;
} Args;

/* 1 when `text` (a flag's value) holds a line break, which would break mod.ini's line */
static int has_line_break(const char *text)
{
    return strchr(text, '\n') != NULL || strchr(text, '\r') != NULL;
}

static int parse(int argc, char **argv, Args *a, ExchangeError *err)
{
    memset(a, 0, sizeof *a);
    a->data = "original", a->port = "data";
    for (int i = 2; i < argc; ++i) {
        const int has_value = i + 1 < argc;
        if (strcmp(argv[i], "--template") == 0 && has_value) a->template = argv[++i];
        else if (strcmp(argv[i], "--id") == 0 && has_value) a->id = argv[++i];
        else if (strcmp(argv[i], "--stem") == 0 && has_value) a->stem = argv[++i];
        else if (strcmp(argv[i], "--out") == 0 && has_value) a->out = argv[++i];
        else if (strcmp(argv[i], "--name") == 0 && has_value) a->name = argv[++i];
        else if (strcmp(argv[i], "--author") == 0 && has_value) a->author = argv[++i];
        else if (strcmp(argv[i], "--description") == 0 && has_value) a->description = argv[++i];
        else if (strcmp(argv[i], "--data") == 0 && has_value) a->data = argv[++i];
        else if (strcmp(argv[i], "--port-data") == 0 && has_value) a->port = argv[++i];
        else if (strcmp(argv[i], "--force") == 0) a->force = 1;
        else { exchange_fail(err, "unexpected argument '%s'; %s", argv[i], USAGE); return 0; }
    }
    if (a->template == NULL || a->id == NULL || a->stem == NULL || a->out == NULL) { exchange_fail(err, USAGE); return 0; }
    if ((a->author != NULL && has_line_break(a->author)) || (a->description != NULL && has_line_break(a->description))) {
        exchange_fail(err, "--author and --description are one line of text");
        return 0;
    }
    return 1;
}

int car_template_model(const char *template)
{
    const int model = car_piece_model_of_word(template);
    if (model >= 0) return model;
    char stem[16];
    if (!slot_find(SLOT_CAR, template, stem, sizeof stem)) return -1;
    const char *const *stems = slot_stems(SLOT_CAR);
    for (int i = 0; i < RACE_CARS && stems[i] != NULL; i++)
        if (strcmp(stems[i], stem) == 0) return i;
    return -1;
}

/* the card, checked by the same parser the game reads it with */
static int add_card(FileSet *set, const Args *a, const char *word, const char *abs, const char *traction, ExchangeError *err)
{
    char head[MOD_INI_MAX_SIZE], texts[MOD_INI_MAX_SIZE], card[MOD_INI_MAX_SIZE];
    const int h = snprintf(head, sizeof head, "id=%s\nname=%s\nversion=0.1\ntype=car\nstem=%s\nshort_name=%s\nsound_from=%s\ndash_from=%s\nabs=%s\ntraction=%s\n", a->id,
                           a->name != NULL ? a->name : a->id, a->stem, a->stem, word, word, abs, traction);
    ModInfo info;
    char why[160];
    if (h < 0 || (size_t)h >= sizeof head || !mod_ini_text_keys(a->author, a->description, texts, sizeof texts)) {
        exchange_fail(err, "the card does not fit mod.ini's %d bytes", MOD_INI_MAX_SIZE);
        return 0;
    }
    const int n = snprintf(card, sizeof card, "%s%s", head, texts);
    if (n < 0 || (size_t)n >= sizeof card) { exchange_fail(err, "the card does not fit mod.ini's %d bytes", MOD_INI_MAX_SIZE); return 0; }
    if (!mod_ini_parse((const uint8_t *)card, (size_t)n, &info, why, sizeof why)) { exchange_fail(err, "mod.ini: %s", why); return 0; }
    if (!fileset_add(set, "mod.ini", (const uint8_t *)card, (size_t)n)) { exchange_fail(err, "out of memory"); return 0; }
    return 1;
}

/* port (dep78): "yes" or "no" for the donor's ABS and traction control, the words of the PBS copied under `stem` in `set` (the card then says what the car has) */
static void donor_assists(const FileSet *set, const char *stem, const char **abs, const char **traction)
{
    char paths[CAR_PIECE_FILES][CAR_PIECE_PATH];
    car_piece_paths(stem, paths);
    const SetFile *pbs = fileset_find(set, paths[2]);
    *abs = car_piece_assist("", pbs != NULL ? pbs->data : NULL, pbs != NULL ? pbs->size : 0, CAR_PIECE_PBS_ABS) ? "yes" : "no";
    *traction = car_piece_assist("", pbs != NULL ? pbs->data : NULL, pbs != NULL ? pbs->size : 0, CAR_PIECE_PBS_TRACTION) ? "yes" : "no";
}

/* the template's CFM, PDN and PBS under the new stem */
static int add_files(FileSet *set, const char *data, const char *from, const char *stem, ExchangeError *err)
{
    char src[CAR_PIECE_FILES][CAR_PIECE_PATH], dst[CAR_PIECE_FILES][CAR_PIECE_PATH];
    car_piece_paths(from, src);
    car_piece_paths(stem, dst);
    for (int i = 0; i < CAR_PIECE_FILES; i++) {
        char real[PATH_MAX];
        if (!fsio_find_ci(data, src[i], real, sizeof real)) { exchange_fail(err, "no such file in the data root %s: %s", data, src[i]); return 0; }
        size_t size = 0;
        uint8_t *bytes = fsio_read_file(real, &size, err);
        if (bytes == NULL) return 0;
        const int ok = fileset_add_owned(set, dst[i], bytes, size);
        if (!ok) { free(bytes); exchange_fail(err, "out of memory"); return 0; }
    }
    return 1;
}

/* what the port's data holds for the template in `kind` (car_masks, dash_masks), as kind/STEM/... */
static int add_masks(FileSet *set, const char *port, const char *kind, const char *from, const char *stem, ExchangeError *err)
{
    char rel[64], real[PATH_MAX], prefix[64];
    snprintf(rel, sizeof rel, "%s/%s", kind, from);
    if (!fsio_find_ci(port, rel, real, sizeof real)) return 1;
    FileSet masks;
    fileset_init(&masks);
    int ok = fsio_read_tree(real, &masks, err);
    snprintf(prefix, sizeof prefix, "%s/%s/", kind, stem);
    for (size_t i = 0; ok && i < masks.count; i++) {
        char name[PATH_MAX];
        snprintf(name, sizeof name, "%s%s", prefix, masks.file[i].name);
        ok = fileset_add(set, name, masks.file[i].data, masks.file[i].size);
        if (!ok) exchange_fail(err, "out of memory");
    }
    fileset_free(&masks);
    return ok;
}

/* port (dep59 step 3): the generic card.png, a plain ground the game draws the car's name over (studio/car_card.h) */
static int add_generic_card(FileSet *set, ExchangeError *err)
{
    uint8_t *png = NULL;
    size_t len = 0;
    if (!car_card_generic_png(&png, &len, err)) return 0;
    if (!fileset_add_owned(set, "card.png", png, len)) { free(png); exchange_fail(err, "out of memory"); return 0; }
    return 1;
}

static int car_new(const Args *a, ExchangeError *err)
{
    const int model = car_template_model(a->template);
    if (model < 0) { exchange_fail(err, "--template '%s' is not one of the eight race cars (a word such as viper, or a stem such as DVIPER)", a->template); return 0; }
    if (!mod_ini_id_ok(a->id)) { exchange_fail(err, "--id '%s' must be up to %d of a-z, 0-9 and '-', not first", a->id, MOD_INI_ID_MAX); return 0; }
    if (!car_piece_stem_ok(a->stem)) {
        exchange_fail(err, "--stem '%s' must be 1..8 of A-Z and 0-9, and not a stem of the game's cars (nor M and one)", a->stem);
        return 0;
    }
    const char *word = car_piece_word_of_model(model), *from = slot_stems(SLOT_CAR)[model];
    FileSet set;
    fileset_init(&set);
    const char *abs = "no", *traction = "no";
    int ok = add_files(&set, a->data, from, a->stem, err);
    if (ok) donor_assists(&set, a->stem, &abs, &traction);
    ok = ok && add_card(&set, a, word, abs, traction, err) && add_generic_card(&set, err);
    if (ok && !fsio_exists(a->port)) {
        fprintf(stderr, "car new: no port data at %s: the piece has no car or dash masks (--port-data DIR names the folder that holds them)\n", a->port);
    } else if (ok) {
        ok = add_masks(&set, a->port, "car_masks", from, a->stem, err) && add_masks(&set, a->port, "dash_masks", from, a->stem, err);
    }
    if (ok) ok = fsio_write_tree(a->out, &set, a->force, err);
    if (ok) {
        printf("car new: %s is the car %s (%s), from %s; %zu files\n", a->out, a->id, a->stem, word, set.count);
        printf("try it (Restomod only: an edition with cars=%s, edition=<id> in restomod.cfg; docs/modding.md, Editions): tnfs --mods %s --car %s\n",
               a->id, a->out, a->id);
    }
    fileset_free(&set);
    return ok;
}

int car_command(int argc, char **argv, ExchangeError *err)
{
    if (argc >= 2 && strcmp(argv[1], "card") == 0) return car_card_command(argc, argv, err);
    if (argc >= 2 && strcmp(argv[1], "import") == 0) return car_import_command(argc, argv, err);
    if (argc >= 2 && strcmp(argv[1], "resize") == 0) return car_resize_command(argc, argv, err);
    if (argc >= 2 && strcmp(argv[1], "texture") == 0) return car_texture_command(argc, argv, err);
    if (argc >= 2 && strcmp(argv[1], "view") == 0) return car_view_command(argc, argv, err);
    if (argc < 2 || strcmp(argv[1], "new") != 0) { exchange_fail(err, USAGE " | tnfs-studio car import PIECE_DIR --obj FILE.obj ... | tnfs-studio car resize PIECE_DIR NAME WxH ... | "
                                                                                 "tnfs-studio car texture add PIECE_DIR NAME FILE.png ... | tnfs-studio car card PIECE_DIR --generic | tnfs-studio car view CAR.CFM --shot OUT.png"); return 0; }
    Args a;
    return parse(argc, argv, &a, err) && car_new(&a, err);
}
