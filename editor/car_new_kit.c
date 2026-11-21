/* See car_new_kit.h. */
#include "car_new_kit.h"

#include "formats/car_piece.h"
#include "formats/exchange.h"
#include "formats/mod_ini.h"
#include "studio/car_kit.h"
#include "studio/car_new.h"
#include "studio/fsio.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

enum { PATH = 1024, STEM_DIGITS = 2 };

static int refuse(char *why, size_t cap, const char *text, const char *detail)
{
    snprintf(why, cap, "%s%s%s", text, detail[0] != '\0' ? ": " : "", detail);
    return 0;
}

/* 1 when the kit folder `dir` has a card that says stem=`stem` (any case) */
static int kit_has_stem(const char *dir, const char *stem)
{
    char path[PATH], line[160];
    snprintf(path, sizeof path, "%s/mod.ini", dir);
    FILE *f = fopen(path, "r");
    if (f == NULL) return 0;
    int found = 0;
    while (!found && fgets(line, sizeof line, f) != NULL)
        found = strncmp(line, "stem=", 5) == 0 && strncasecmp(line + 5, stem, strlen(stem)) == 0 && (line[5 + strlen(stem)] == '\n' || line[5 + strlen(stem)] == '\r' || line[5 + strlen(stem)] == '\0');
    fclose(f);
    return found;
}

static int stem_taken(const char *cars_dir, const char *stem)
{
    DIR *d = opendir(cars_dir);
    int taken = 0;
    for (struct dirent *e = d != NULL ? readdir(d) : NULL; !taken && e != NULL; e = readdir(d)) {
        char dir[PATH];
        if (e->d_name[0] == '.') continue;
        snprintf(dir, sizeof dir, "%s/%s", cars_dir, e->d_name);
        taken = kit_has_stem(dir, stem);
    }
    if (d != NULL) closedir(d);
    return taken;
}

int car_new_stem(const char *cars_dir, const char *id, char *stem, size_t cap)
{
    char base[MOD_INI_STEM_MAX + 1];
    size_t n = 0;
    for (const char *c = id; *c != '\0' && n < MOD_INI_STEM_MAX; ++c)
        if (isalnum((unsigned char)*c)) base[n++] = (char)toupper((unsigned char)*c);
    base[n] = '\0';
    if (!car_piece_stem_ok(base)) {   // empty, or a car of the game's: an X in front makes it ours
        char with_x[MOD_INI_STEM_MAX + 1];
        snprintf(with_x, sizeof with_x, "X%.*s", MOD_INI_STEM_MAX - 1, base);
        snprintf(base, sizeof base, "%s", with_x);
    }
    if (!car_piece_stem_ok(base)) return 0;
    char cand[MOD_INI_STEM_MAX + 1];
    snprintf(cand, sizeof cand, "%s", base);
    for (int k = 1; stem_taken(cars_dir, cand); ++k) {
        if (k > 99) return 0;
        snprintf(cand, sizeof cand, "%.*s%0*d", MOD_INI_STEM_MAX - STEM_DIGITS, base, STEM_DIGITS, k);
    }
    if (!car_piece_stem_ok(cand) || strlen(cand) >= cap) return 0;
    snprintf(stem, cap, "%s", cand);
    return 1;
}

/* `car new` through the command the tool has: its argument list built here */
static int run_car_new(const char *cd, const char *port, const char *template_word, const char *name, const char *id, const char *stem, const char *out, char *why, size_t cap)
{
    char t[64], i[64], s[16], o[PATH], n[160], d[PATH], p[PATH];
    snprintf(t, sizeof t, "%s", template_word), snprintf(i, sizeof i, "%s", id), snprintf(s, sizeof s, "%s", stem);
    snprintf(o, sizeof o, "%s", out), snprintf(n, sizeof n, "%s", name[0] != '\0' ? name : id), snprintf(d, sizeof d, "%s", cd), snprintf(p, sizeof p, "%s", port);
    char *argv[] = { "car", "new", "--template", t, "--id", i, "--stem", s, "--out", o, "--name", n, "--data", d, "--port-data", p };
    ExchangeError err = { { 0 } };
    if (car_command((int)(sizeof argv / sizeof argv[0]), argv, &err)) return 1;
    return refuse(why, cap, "car new", err.message);
}

int car_new_kit(const char *cd, const char *port, const char *cars_dir, const char *template_word, const char *name, const char *id, char *kit, size_t kit_cap, char *why, size_t why_cap)
{
    why[0] = '\0';
    if (car_template_model(template_word) < 0) return refuse(why, why_cap, "not one of the eight cars you drive", template_word);
    if (!mod_ini_id_ok(id)) return refuse(why, why_cap, "the id may hold only a-z, 0-9 and '-' (not first)", "");
    char stem[MOD_INI_STEM_MAX + 1], piece[PATH], tmp[PATH];
    if (snprintf(kit, kit_cap, "%s/%s", cars_dir, id) >= (int)kit_cap || snprintf(piece, sizeof piece, "%s.piece", kit) >= (int)sizeof piece ||
        snprintf(tmp, sizeof tmp, "%s.making", kit) >= (int)sizeof tmp)
        return refuse(why, why_cap, "the path is too long", kit);
    struct stat st;
    if (lstat(kit, &st) == 0) return refuse(why, why_cap, id, "is already in the workspace's cars: choose another name or id");
    if (cd[0] == '\0' || !fsio_exists(cd)) return refuse(why, why_cap, "the game's files are not found", "New car copies one of its cars");
    if (!car_new_stem(cars_dir, id, stem, sizeof stem)) return refuse(why, why_cap, "no stem is free for", id);
    ExchangeError err = { { 0 } };
    int ok = run_car_new(cd, port, template_word, name, id, stem, piece, why, why_cap);
    ok = ok && car_kit_from_piece(piece, tmp, &err);
    if (!ok && why[0] == '\0') refuse(why, why_cap, "car kit", err.message);
    if (ok && rename(tmp, kit) != 0) ok = refuse(why, why_cap, "cannot make the kit", strerror(errno));
    fsio_remove_tree(piece);
    if (!ok) fsio_remove_tree(tmp);
    return ok;
}
