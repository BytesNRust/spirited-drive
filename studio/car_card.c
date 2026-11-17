#include "studio/car_card.h"

#include "formats/png.h"
#include "studio/fsio.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USAGE                                                                                                         \
    "usage: tnfs-studio car card PIECE_DIR --generic [--force] | tnfs-studio car card PIECE_DIR --tnfs BIN [--data DIR] [--track TR1] " \
    "[--node N] [--time day|dawn|dusk|night] [--force]"

enum { FRAME = 2 };
enum { INK_GROUND = 0, INK_FRAME = 1 };

static const uint8_t PALETTE[2 * 3] = { 36, 38, 42, 150, 152, 158 };

int car_card_generic_png(uint8_t **png, size_t *len, ExchangeError *err)
{
    uint8_t *px = calloc((size_t)CAR_CARD_W * CAR_CARD_H, 1), palette[768] = { 0 };
    if (px == NULL) { exchange_fail(err, "out of memory"); return 0; }
    for (int y = 0; y < CAR_CARD_H; y++)
        for (int x = 0; x < CAR_CARD_W; x++)
            if (x < FRAME || y < FRAME || x >= CAR_CARD_W - FRAME || y >= CAR_CARD_H - FRAME) px[y * CAR_CARD_W + x] = INK_FRAME;
    memcpy(palette, PALETTE, sizeof PALETTE);
    const PngResult r = png_write_indexed(px, CAR_CARD_W, CAR_CARD_H, palette, png, len);
    free(px);
    if (r != PNG_OK) { exchange_fail(err, "card.png: %s", png_result_str(r)); return 0; }
    return 1;
}

int car_card_read_info(const char *piece_dir, ModInfo *info, ExchangeError *err)
{
    char path[PATH_MAX], why[160];
    if (!fsio_find_ci(piece_dir, "mod.ini", path, sizeof path)) { exchange_fail(err, "%s has no mod.ini: not a car piece", piece_dir); return 0; }
    size_t size = 0;
    uint8_t *card = fsio_read_file(path, &size, err);
    if (card == NULL) return 0;
    const int parsed = mod_ini_parse(card, size, info, why, sizeof why);
    free(card);
    if (!parsed) { exchange_fail(err, "%s: %s", path, why); return 0; }
    if (info->type != MOD_TYPE_CAR) { exchange_fail(err, "%s: not a car piece (type=car)", path); return 0; }
    return 1;
}

int car_card_generic(const char *piece_dir, int force, ExchangeError *err)
{
    ModInfo info;
    if (!car_card_read_info(piece_dir, &info, err)) return 0;
    uint8_t *png = NULL;
    size_t len = 0;
    char path[PATH_MAX];
    if (!car_card_generic_png(&png, &len, err)) return 0;
    snprintf(path, sizeof path, "%s/card.png", piece_dir);
    const int ok = fsio_write_file(path, png, len, force, err);
    free(png);
    if (ok) printf("car card: %s, the generic card of %s\n", path, info.name);
    return ok;
}

static int time_ok(const char *name)
{
    static const char *const TIMES[] = { "day", "dawn", "dusk", "night" };
    for (size_t i = 0; i < sizeof TIMES / sizeof TIMES[0]; i++)
        if (strcmp(name, TIMES[i]) == 0) return 1;
    return 0;
}

static int parse_shot(int argc, char **argv, CarCardShot *s, int *generic, ExchangeError *err)
{
    memset(s, 0, sizeof *s);
    s->data = "original", s->track = "TR1", s->time_name = "day", s->node = 200;
    *generic = 0;
    for (int i = 2; i < argc; i++) {
        const int has_value = i + 1 < argc;
        if (strcmp(argv[i], "--generic") == 0) *generic = 1;
        else if (strcmp(argv[i], "--force") == 0) s->force = 1;
        else if (strcmp(argv[i], "--tnfs") == 0 && has_value) s->tnfs = argv[++i];
        else if (strcmp(argv[i], "--data") == 0 && has_value) s->data = argv[++i];
        else if (strcmp(argv[i], "--track") == 0 && has_value) s->track = argv[++i];
        else if (strcmp(argv[i], "--time") == 0 && has_value) s->time_name = argv[++i];
        else if (strcmp(argv[i], "--node") == 0 && has_value) {
            char *end;
            const long n = strtol(argv[++i], &end, 10);
            if (*end != '\0' || n < 0 || n > 100000) { exchange_fail(err, "--node '%s' is not a node number", argv[i]); return 0; }
            s->node = (int)n;
        } else if (argv[i][0] != '-' && s->piece == NULL) s->piece = argv[i];
        else { exchange_fail(err, "unexpected argument '%s'; %s", argv[i], USAGE); return 0; }
    }
    if (s->piece == NULL || (!*generic && s->tnfs == NULL)) { exchange_fail(err, USAGE); return 0; }
    if (!time_ok(s->time_name)) { exchange_fail(err, "--time '%s' is not day, dawn, dusk or night", s->time_name); return 0; }
    if (!*generic && !fsio_exists(s->tnfs)) { exchange_fail(err, "--tnfs %s: no such program", s->tnfs); return 0; }
    return 1;
}

int car_card_command(int argc, char **argv, ExchangeError *err)
{
    CarCardShot s;
    int generic;
    if (!parse_shot(argc, argv, &s, &generic, err)) return 0;
    return generic ? car_card_generic(s.piece, s.force, err) : car_card_shot(&s, err);
}
