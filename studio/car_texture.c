/* tnfs-studio car texture add (studio/car_texture.h). */
#include "studio/car_texture.h"

#include "formats/car_piece.h"
#include "formats/car_texture_add.h"
#include "studio/car_import.h"
#include "studio/car_resize.h"
#include "studio/fsio.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USAGE "usage: tnfs-studio car texture add PIECE_DIR NAME FILE.png [--low] [--label LABEL]"

typedef struct {
    const char *piece, *name, *png, *label;
    int low;
} Args;

static int parse(int argc, char **argv, Args *a, ExchangeError *err)
{
    memset(a, 0, sizeof *a);
    if (argc < 3 || strcmp(argv[2], "add") != 0) { exchange_fail(err, USAGE); return 0; }
    for (int i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--label") == 0 && i + 1 < argc) a->label = argv[++i];
        else if (strcmp(argv[i], "--low") == 0) a->low = 1;
        else if (argv[i][0] != '-' && a->piece == NULL) a->piece = argv[i];
        else if (argv[i][0] != '-' && a->name == NULL) a->name = argv[i];
        else if (argv[i][0] != '-' && a->png == NULL) a->png = argv[i];
        else { exchange_fail(err, "unexpected argument '%s'; %s", argv[i], USAGE); return 0; }
    }
    if (a->piece == NULL || a->name == NULL || a->png == NULL) { exchange_fail(err, USAGE); return 0; }
    if (a->label == NULL) a->label = a->name;
    const char *why = car_texture_name_refused(a->name);
    if (why != NULL) { exchange_fail(err, "'%s' cannot be a bitmap's name: %s", a->name, why); return 0; }
    if ((why = car_texture_label_refused(a->label)) != NULL) { exchange_fail(err, "'%s' cannot be a label: %s", a->label, why); return 0; }
    return 1;
}

static int add(const Args *a, ExchangeError *err)
{
    ModInfo info;
    char paths[CAR_PIECE_FILES][CAR_PIECE_PATH], cfm_path[PATH_MAX];
    if (!car_piece_card(a->piece, &info, err)) return 0;
    car_piece_paths(info.stem, paths);
    if (!fsio_find_ci(a->piece, paths[0], cfm_path, sizeof cfm_path)) { exchange_fail(err, "%s has no %s", a->piece, paths[0]); return 0; }
    size_t len = 0, out_len = 0;
    uint8_t *cfm = fsio_read_file(cfm_path, &len, err), *pixels = NULL, *out = NULL;
    const int level = a->low ? 1 : 0;
    uint16_t w = 0, h = 0;
    uint32_t ref = 0;
    int ok = cfm != NULL && (pixels = car_bank_png_pixels(a->png, cfm, len, level, &w, &h, err)) != NULL &&
             car_texture_add(cfm, len, level, a->name, a->label, pixels, w, h, &out, &out_len, &ref, err) &&
             fsio_write_atomic(cfm_path, out, out_len, err);
    if (ok) {
        printf("car texture add: %s: %s %ux%u added to the %s model's bank\n", cfm_path, a->name, w, h, a->low ? "far" : "high");
        printf("car texture add: in Blender, faces take it with the material t%u_%s (unpack the piece again to list it)\n", ref, a->label);
    }
    free(cfm), free(pixels), free(out);
    return ok;
}

int car_texture_command(int argc, char **argv, ExchangeError *err)
{
    Args a;
    return parse(argc, argv, &a, err) && add(&a, err);
}
