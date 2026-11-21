/* See car_export_kit.h. */
#include "car_export_kit.h"

#include "formats/car_piece.h"
#include "formats/exchange.h"
#include "studio/car_kit.h"
#include "studio/fsio.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static int bad(char *why, size_t cap, const char *what, const char *detail)
{
    snprintf(why, cap, "%s: %s", what, detail);
    return 0;
}

/* `name` is below `dir` (the first folder of the path, any case) */
static int below(const char *name, const char *dir)
{
    const size_t n = strlen(dir);
    return strncasecmp(name, dir, n) == 0 && name[n] == '/';
}

/* the car piece `all`'s files (an export's project, read) made a piece `car piece` knows: masks under the stem; for a masks project (no SIMDATA of its own) the CFM in
 * original/ goes to SIMDATA/CARFAMS/<STEM>.CFM and a minimal mod.ini is made */
static int shape_piece(const ExportCar *car, FileSet *all, FileSet *out, char *why, size_t cap)
{
    char prefix[96], paths[CAR_PIECE_FILES][CAR_PIECE_PATH], folder_prefix[96];
    car_piece_paths(car->file, paths);
    snprintf(prefix, sizeof prefix, "car_masks/%s/", car->file);
    snprintf(folder_prefix, sizeof folder_prefix, "car_masks/%s", car->folder);
    int ok = 1;
    for (size_t i = 0; ok && i < all->count; ++i) {
        const SetFile *f = &all->file[i];
        if (below(f->name, folder_prefix) && strcasecmp(f->name + strlen(folder_prefix) + 1, "dash_mask.png") == 0) {
            continue;   /* data/car_masks/<car>/dash_mask.png is an old stray copy: no texture is called dash (the game reads <tex>_mask.png), the dash mask is dash_masks/<STEM>/ */
        } else if (below(f->name, folder_prefix)) {
            char to[512];
            snprintf(to, sizeof to, "%s%s", prefix, f->name + strlen(folder_prefix) + 1);
            ok = fileset_add(out, to, f->data, f->size);
        } else if (below(f->name, "original")) {
            ok = strcasecmp(f->name + 9, strrchr(paths[0], '/') + 1) == 0 ? fileset_add(out, paths[0], f->data, f->size) : 1;   /* the CFM, under its stem */
        } else if (strcmp(f->name, "project.ini") != 0) {
            ok = fileset_add(out, f->name, f->data, f->size);
        }
        if (!ok) return bad(why, cap, "out of memory", car->file);
    }
    if (car->kind != EXPORT_DRIVABLE) {
        char id[48], card[512];
        snprintf(id, sizeof id, "%s-original", car->file);
        for (char *c = id; *c != '\0'; ++c) *c = (char)tolower((unsigned char)*c);
        const int n = snprintf(card, sizeof card, "id=%s\nname=%s (original)\nversion=0.1\ntype=car\nstem=%s\nredistributable=0\n", id, car->label, car->file);
        if (n < 0 || (size_t)n >= sizeof card || !fileset_add(out, "mod.ini", (const uint8_t *)card, (size_t)n)) return bad(why, cap, "the card does not fit", car->file);
    }
    return fileset_find(out, paths[0]) != NULL ? 1 : bad(why, cap, "the car's CFM is not in its export", car->file);
}

int car_export_kit(const ExportSource *src, int index, const char *kit, char *why, size_t why_cap)
{
    const ExportCar *car = car_export_car(index);
    if (car == NULL) return bad(why, why_cap, "no such car", "");
    char project[EXPORT_PATH], piece[EXPORT_PATH], tmp[EXPORT_PATH];
    if (snprintf(project, sizeof project, "%s.project", kit) >= (int)sizeof project || snprintf(piece, sizeof piece, "%s.piece", kit) >= (int)sizeof piece ||
        snprintf(tmp, sizeof tmp, "%s.exporting", kit) >= (int)sizeof tmp)
        return bad(why, why_cap, "the path is too long", kit);
    struct stat st;
    if (lstat(kit, &st) == 0) return bad(why, why_cap, kit, "is already there: choose another folder (nothing was written)");
    ExchangeError err = { { 0 } };
    FileSet all, out;
    fileset_init(&all), fileset_init(&out);
    int ok = car_export(src, index, project, why, why_cap) && fsio_read_tree(project, &all, &err) && shape_piece(car, &all, &out, why, why_cap) &&
             fsio_write_tree(piece, &out, 0, &err);
    if (!ok && why[0] == '\0') snprintf(why, why_cap, "%s", err.message);
    ok = ok && car_kit_from_piece(piece, tmp, &err);
    if (!ok && err.message[0] != '\0') snprintf(why, why_cap, "%s", err.message);
    if (ok && rename(tmp, kit) != 0) ok = bad(why, why_cap, "cannot make the kit", strerror(errno));
    fsio_remove_tree(project), fsio_remove_tree(piece);
    if (!ok) fsio_remove_tree(tmp);
    fileset_free(&all), fileset_free(&out);
    return ok;
}
