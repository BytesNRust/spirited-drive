/* tnfs-studio car masks (studio/car_masks.h). */
#include "studio/car_masks.h"

#include "formats/car_mask_codec.h"
#include "formats/mod_ini.h"
#include "formats/png.h"
#include "studio/fsio.h"

#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define USAGE "usage: tnfs-studio car masks KIT_DIR [--stem STEM] [--force]"

enum { MASKS_MAX = 64, NAME_BYTES = 8 };

typedef struct {
    char name[NAME_BYTES + 1];
    uint8_t *png;
    size_t len;
    unsigned counts[256];
    uint32_t width, height;
} Made;

static void made_free(Made *m, int n)
{
    for (int i = 0; i < n; i++) png_free(m[i].png);
}

/* NAME or NAME_lo, NAME of 4 letters or digits (lower case) */
static int mask_name_ok(const char *name)
{
    const size_t n = strlen(name);
    if (n != 4 && n != 7) return 0;
    for (size_t i = 0; i < 4; i++)
        if (!islower((unsigned char)name[i]) && !isdigit((unsigned char)name[i])) return 0;
    return n == 4 || strcmp(name + 4, "_lo") == 0;
}

/* the picture `path` classed, as a grey PNG in `m` */
static int make_one(const char *path, Made *m, ExchangeError *err)
{
    size_t size = 0;
    uint8_t *file = fsio_read_file(path, &size, err);
    if (file == NULL) return 0;
    PngImage img;
    const PngResult r = png_read(file, size, &img);
    free(file);
    if (r != PNG_OK) { exchange_fail(err, "%s: %s", path, png_result_str(r)); return 0; }
    const size_t count = (size_t)img.width * img.height;
    uint8_t *classes = malloc(count);
    if (classes == NULL) { png_free(img.pixels); exchange_fail(err, "out of memory"); return 0; }
    unsigned top = 255;   /* a picture saved from GIMP has the layer's opacity in its alpha (the kit's solid texels: 117), its antialiased edges far under it: a texel is opaque when it is at least a quarter as opaque as the most opaque one */
    if (!img.indexed) {
        top = 0;
        for (size_t i = 0; i < count; i++)
            if (img.pixels[4 * i + 3] > top) top = img.pixels[4 * i + 3];
    }
    for (size_t i = 0; i < count; i++) {
        const uint8_t *c = img.indexed ? img.palette + 3 * img.pixels[i] : img.pixels + 4 * i;
        const int opaque = img.indexed || (c[3] != 0 && 4u * c[3] >= top);
        classes[i] = (uint8_t)car_mask_class_rgba(c[0], c[1], c[2], opaque ? 255 : 0);
        m->counts[classes[i]]++;
    }
    png_free(img.pixels);
    m->width = img.width, m->height = img.height;
    const int ok = car_mask_write_grey(classes, (int)img.width, (int)img.height, &m->png, &m->len);
    free(classes);
    if (!ok) exchange_fail(err, "%s: cannot write the mask", path);
    return ok;
}

static int stem_of(const char *kit, const char *given, char *out, size_t cap, ExchangeError *err)
{
    if (given != NULL) {
        if (strlen(given) == 0 || strlen(given) >= cap) { exchange_fail(err, "'%s' is not a stem", given); return 0; }
        snprintf(out, cap, "%s", given);
        return 1;
    }
    char path[PATH_MAX];
    snprintf(path, sizeof path, "%s/coche/mod.ini", kit);
    size_t size = 0;
    uint8_t *text = fsio_read_file(path, &size, err);
    if (text == NULL) return 0;
    ModInfo *info = calloc(1, sizeof *info);
    char why[200] = "";
    int ok = info != NULL && mod_ini_parse(text, size, info, why, sizeof why);
    if (!ok) exchange_fail(err, "%s: %s", path, info == NULL ? "out of memory" : why);
    else if (info->stem[0] == '\0' || strlen(info->stem) >= cap) { exchange_fail(err, "%s has no stem= (or use --stem STEM)", path), ok = 0; }
    else snprintf(out, cap, "%s", info->stem);
    free(info);
    free(text);
    return ok;
}

int car_masks_make(const char *kit, const char *stem_given, int force, FILE *log, ExchangeError *err)
{
    char stem[64], dir[PATH_MAX];
    if (!stem_of(kit, stem_given, stem, sizeof stem, err)) return -1;
    snprintf(dir, sizeof dir, "%s/mascaras", kit);
    DIR *d = opendir(dir);
    if (d == NULL) { exchange_fail(err, "%s: no such folder (the kit's colour masks)", dir); return -1; }
    static Made made[MASKS_MAX];   /* the command runs once; the editor calls it from one thread */
    memset(made, 0, sizeof made);
    int n = 0, ok = 1;
    for (struct dirent *e; ok && (e = readdir(d)) != NULL;) {
        const size_t len = strlen(e->d_name);
        if (len < 5 || strcasecmp(e->d_name + len - 4, ".png") != 0) continue;
        char name[NAME_BYTES + 1];
        if (len - 4 > NAME_BYTES) { exchange_fail(err, "%s/%s: a mask's name is 4 letters or digits, then _lo for the far model", dir, e->d_name); ok = 0; break; }
        memcpy(name, e->d_name, len - 4), name[len - 4] = '\0';
        if (!mask_name_ok(name)) { exchange_fail(err, "%s/%s: a mask's name is 4 letters or digits, then _lo for the far model", dir, e->d_name); ok = 0; break; }
        if (n == MASKS_MAX) { exchange_fail(err, "%s: more than %d masks", dir, MASKS_MAX); ok = 0; break; }
        char path[PATH_MAX];
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        memcpy(made[n].name, name, sizeof made[n].name);
        ok = make_one(path, &made[n], err);
        if (ok) n++;
    }
    closedir(d);
    int written = 0;
    for (int i = 0; ok && i < n; i++) {
        char path[PATH_MAX];
        snprintf(path, sizeof path, "%s/coche/car_masks/%s/%s_mask.png", kit, stem, made[i].name);
        if (!force && fsio_exists(path)) { exchange_fail(err, "%s: exists (use --force to overwrite)", path); ok = 0; break; }
    }
    for (int i = 0; ok && i < n; i++) {
        char path[PATH_MAX];
        snprintf(path, sizeof path, "%s/coche/car_masks/%s/%s_mask.png", kit, stem, made[i].name);
        ok = fsio_write_file(path, made[i].png, made[i].len, 1, err);
        if (ok) {
            written++;
            if (log != NULL)
                fprintf(log, "%s_mask.png: %ux%u, %u paint, %u glass, %u body, %u black, %u lights, %u matte\n", made[i].name, made[i].width, made[i].height,
                        made[i].counts[CAR_MASK_PAINT], made[i].counts[CAR_MASK_SHINE], made[i].counts[CAR_MASK_BODY], made[i].counts[CAR_MASK_BLACK],
                        made[i].counts[CAR_MASK_BRAKE] + made[i].counts[CAR_MASK_POSITION] + made[i].counts[CAR_MASK_HEADLIGHT] + made[i].counts[CAR_MASK_REVERSE],
                        made[i].counts[CAR_MASK_MATTE]);
        }
    }
    made_free(made, n);
    return ok ? written : -1;
}

int car_masks_command(int argc, char **argv, ExchangeError *err)
{
    const char *kit = NULL, *stem = NULL;
    int force = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--stem") == 0 && i + 1 < argc) stem = argv[++i];
        else if (strcmp(argv[i], "--force") == 0) force = 1;
        else if (argv[i][0] != '-' && kit == NULL) kit = argv[i];
        else { exchange_fail(err, "unexpected argument '%s'; %s", argv[i], USAGE); return 0; }
    }
    if (kit == NULL) { exchange_fail(err, USAGE); return 0; }
    const int n = car_masks_make(kit, stem, force, stdout, err);
    if (n < 0) return 0;
    printf("%d masks written\n", n);
    return 1;
}
