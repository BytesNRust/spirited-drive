#include "studio/mod_validate.h"

#include "formats/car_piece.h"
#include "formats/mod_check.h"
#include "formats/mod_ini.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* 1 when `name` is one of the piece's three files, in any case (car_piece_check finds them so) */
static int piece_file(const ModInfo *card, const char *name)
{
    char paths[CAR_PIECE_FILES][CAR_PIECE_PATH];
    car_piece_paths(card->stem, paths);
    for (int i = 0; i < CAR_PIECE_FILES; i++)
        if (strcasecmp(name, paths[i]) == 0) return 1;
    return 0;
}

/* a car piece: its check, and validate_tree for the files it does not look at */
static int piece_tree(const ModInfo *card, const FileSet *set, Report *r, ExchangeError *err)
{
    car_piece_check(card, set, r);
    FileSet rest;
    fileset_init(&rest);
    for (size_t i = 0; i < set->count; i++) {
        if (card->stem[0] != '\0' && piece_file(card, set->file[i].name)) continue;
        if (!fileset_add(&rest, set->file[i].name, set->file[i].data, set->file[i].size)) {
            fileset_free(&rest);
            exchange_fail(err, "out of memory");
            return 0;
        }
    }
    validate_tree(&rest, r);
    fileset_free(&rest);
    return 1;
}

int mod_validate_tree(const FileSet *set, Report *r, ExchangeError *err)
{
    const SetFile *ini = fileset_find(set, "mod.ini");
    if (ini == NULL) {
        validate_tree(set, r);
        return 1;
    }
    ModInfo card;
    char why[160];
    if (!mod_ini_parse(ini->data, ini->size, &card, why, sizeof why)) {
        exchange_fail(err, "%s", why);   /* it names mod.ini */
        return 0;
    }
    if (card.type == MOD_TYPE_CAR) return piece_tree(&card, set, r, err);
    validate_tree(set, r);
    return 1;
}

/* the entries the installation would unpack, by their path below the mod's folder */
static int unpack(const uint8_t *zip, size_t size, const ModCheck *c, FileSet *set, ExchangeError *err)
{
    ZipArchive z;
    ZipResult zr = zip_open(zip, size, &z);
    for (size_t i = 0; zr == ZIP_OK && i < c->entry_count; i++) {
        uint8_t *data = NULL;
        size_t n = 0;
        zr = zip_extract(&z, c->entries[i].index, &data, &n);
        if (zr == ZIP_OK && !fileset_add_owned(set, c->entries[i].path, data, n)) free(data), zr = ZIP_ERR_MEMORY;
    }
    zip_close(&z);
    if (zr != ZIP_OK) exchange_fail(err, "%s", zip_result_text(zr));
    return zr == ZIP_OK;
}

int mod_validate_zip(const uint8_t *zip, size_t size, Report *r, ExchangeError *err)
{
    ModCheck c;
    char why[256];
    if (!mod_check_zip(zip, size, &c, why, sizeof why)) {
        exchange_fail(err, "the game would not install it: %s", why);
        return 0;
    }
    int ok = 1;
    if (c.info.type == MOD_TYPE_CAR) {
        FileSet set;
        fileset_init(&set);
        ok = unpack(zip, size, &c, &set, err);
        if (ok) car_piece_check(&c.info, &set, r);
        fileset_free(&set);
    }
    mod_check_free(&c);
    return ok;
}
