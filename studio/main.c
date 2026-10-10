/* tnfs-studio: unpacks the game's files into editable trees (PNG, OBJ+MTL, TSV, WAV) and packs them back.
 *   tnfs-studio unpack FILE DIR [--palette FILE] [--force]
 *   tnfs-studio pack   DIR OUT --from ORIGINAL [--palette FILE] [--force]
 *   tnfs-studio validate [--compat | --port-only] [--check-install] FILE_OR_DIR
 *   tnfs-studio slot list|replace|undo ...        (studio/slot.h)
 * `export` and `import` are the same two commands. Packing needs the original file: edits overlay it, and
 * an untouched file comes back byte for byte (a packed file keeps its original stream). Nothing is
 * overwritten without --force. validate checks a file or a mod directory against the limits of the original
 * executable (core/formats/limits.c): in compatible mode (the default) a file the original would mishandle
 * ends with exit 1, in port-only mode the same findings are warnings and the exit is 0.
 * Exit status 0 on success, 1 on any error with a message naming the file. */
#include "formats/exchange.h"
#include "formats/fsh.h"
#include "formats/qfs.h"
#include "formats/resource_exchange.h"
#include "formats/validate.h"
#include "studio/fsio.h"
#include "studio/slot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    const char *positional[2];
    int positionals;
    const char *from;
    const char *palette;
    int force;
    int port_only;
    int check_install;
} Args;

static void usage(FILE *to)
{
    fputs("usage:\n"
          "  tnfs-studio unpack FILE DIR [--palette FILE] [--force]\n"
          "  tnfs-studio pack   DIR OUT --from ORIGINAL [--palette FILE] [--force]\n"
          "  tnfs-studio validate [--compat | --port-only] [--check-install] FILE_OR_DIR\n"
          "  tnfs-studio slot list track|car\n"
          "  tnfs-studio slot replace track|car SLOT SOURCE_DIR --mods DIR [--data DIR] [--stem OLD] [--also REL]...\n"
          "  tnfs-studio slot undo --mods DIR [--all] [--force]\n"
          "  (export = unpack, import = pack)\n"
          "--palette FILE   a game file (FSH or QFS) whose palette colours bitmaps that carry none\n"
          "--force          overwrite existing files\n"
          "--compat         validate: refuse what the original executable would mishandle (default)\n"
          "--port-only      validate: the same findings as warnings, exit 0\n"
          "--check-install  validate: also warn when CARSPECS is writable (the original then zeroes every car's power)\n",
          to);
}

static int parse(int argc, char **argv, Args *a)
{
    memset(a, 0, sizeof *a);
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--force") == 0) a->force = 1;
        else if (strcmp(argv[i], "--compat") == 0) a->port_only = 0;
        else if (strcmp(argv[i], "--port-only") == 0) a->port_only = 1;
        else if (strcmp(argv[i], "--check-install") == 0) a->check_install = 1;
        else if (strcmp(argv[i], "--from") == 0 && i + 1 < argc) a->from = argv[++i];
        else if (strcmp(argv[i], "--palette") == 0 && i + 1 < argc) a->palette = argv[++i];
        else if (argv[i][0] == '-' && argv[i][1] == '-') { fprintf(stderr, "tnfs-studio: unknown option '%s'\n", argv[i]); return 0; }
        else if (a->positionals < 2) a->positional[a->positionals++] = argv[i];
        else { fprintf(stderr, "tnfs-studio: too many arguments\n"); return 0; }
    }
    return 1;
}

/* The first palette entry of a game file, as 768 RGB bytes. */
static int load_palette(const char *path, uint8_t rgb[768], ExchangeError *err)
{
    size_t size = 0;
    uint8_t *file = fsio_read_file(path, &size, err);
    if (file == NULL) return 0;
    uint8_t *plain = file;
    size_t plain_size = size, unpacked = 0;
    if (qfs_is_compressed(file, size) && qfs_decompressed_size(file, size, &unpacked) == QFS_OK) {
        plain = malloc(unpacked + 1);
        plain_size = unpacked;
        if (plain == NULL || qfs_decompress(file, size, plain, unpacked) != QFS_OK) {
            exchange_fail(err, "%s: QFS stream is damaged", path);
            free(plain);
            free(file);
            return 0;
        }
    }
    Fsh fsh;
    int found = 0;
    if (fsh_open(&fsh, plain, plain_size) == FSH_OK) {
        for (uint32_t i = 0; i < fsh.count && !found; ++i) {
            FshEntry entry;
            found = fsh_entry(&fsh, i, &entry) == FSH_OK && fsh_palette_rgb(&entry, rgb) == FSH_OK;
        }
    }
    if (!found) exchange_fail(err, "%s: no palette entry found", path);
    if (plain != file) free(plain);
    free(file);
    return found;
}

static const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

/* "path: message", unless the message already starts with the file's name. */
static void name_the_file(ExchangeError *err, const char *path)
{
    char tmp[sizeof err->message];
    snprintf(tmp, sizeof tmp, "%s", err->message);
    if (strncmp(tmp, base_name(path), strlen(base_name(path))) == 0) exchange_fail(err, "%s: %s", path, tmp + strlen(base_name(path)) + (tmp[strlen(base_name(path))] == ':' ? 2 : 0));
    else exchange_fail(err, "%s: %s", path, tmp);
}

static int do_unpack(const Args *a, const uint8_t *palette, ExchangeError *err)
{
    size_t size = 0;
    uint8_t *file = fsio_read_file(a->positional[0], &size, err);
    if (file == NULL) return 0;
    FileSet set;
    fileset_init(&set);
    int ok = resource_export(file, size, base_name(a->positional[0]), palette, &set, err);
    if (!ok) name_the_file(err, a->positional[0]);
    if (ok) ok = fsio_write_tree(a->positional[1], &set, a->force, err);
    if (ok) printf("%s: %zu files written to %s\n", a->positional[0], set.count, a->positional[1]);
    fileset_free(&set);
    free(file);
    return ok;
}

static int do_pack(const Args *a, const uint8_t *palette, ExchangeError *err)
{
    if (a->from == NULL) { exchange_fail(err, "pack needs --from ORIGINAL (the file the tree was unpacked from)"); return 0; }
    size_t size = 0;
    uint8_t *original = fsio_read_file(a->from, &size, err);
    if (original == NULL) return 0;
    FileSet set;
    fileset_init(&set);
    uint8_t *out = NULL;
    size_t out_len = 0;
    ImportStats stats;
    int ok = fsio_read_tree(a->positional[0], &set, err);
    if (ok) {
        ok = resource_import(original, size, base_name(a->from), palette, &set, &out, &out_len, &stats, err);
        if (!ok) name_the_file(err, a->from);
    }
    if (ok) ok = fsio_write_file(a->positional[1], out, out_len, a->force, err);
    if (ok) {
        printf("%s: %zu bytes written", a->positional[1], out_len);
        if (stats.pixels) printf("; %u pixels, %u reassigned to the palette", stats.pixels, stats.reassigned);
        printf("\n");
    }
    free(out);
    fileset_free(&set);
    free(original);
    return ok;
}

static int is_directory(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* The CARSPECS directory of an installation, writable or not (004400f4 opens By_R&T there for writing). */
static void check_carspecs(const char *root, const FileSet *set, Report *r)
{
    for (size_t i = 0; i < set->count; ++i) {
        const char *name = set->file[i].name;
        const char *dir = strstr(name, "CARSPECS/");
        if (dir == NULL) dir = strstr(name, "carspecs/");
        if (dir == NULL || (dir != name && dir[-1] != '/')) continue;
        char path[1024];
        snprintf(path, sizeof path, "%s/%.*s", root, (int)(dir - name) + 8, name);
        if (access(path, W_OK) == 0) {
            report_add(r, "MOD.CARSPECS_WRITABLE", 0, "%s is writable", path);
            return;
        }
        return;
    }
}

static int do_validate(const Args *a, ExchangeError *err)
{
    Report report;
    report_init(&report);
    const char *target = a->positional[0];
    if (is_directory(target)) {
        FileSet set;
        fileset_init(&set);
        if (!fsio_read_tree(target, &set, err)) { fileset_free(&set); report_free(&report); return 0; }
        validate_tree(&set, &report);
        if (a->check_install) check_carspecs(target, &set, &report);
        printf("%s: %zu files checked\n", target, set.count);
        fileset_free(&set);
    } else {
        size_t size = 0;
        uint8_t *file = fsio_read_file(target, &size, err);
        if (file == NULL) { report_free(&report); return 0; }
        validate_file(file, size, target, &report);
        printf("%s: checked\n", target);
        free(file);
    }
    const ValidateMode mode = a->port_only ? VALIDATE_PORT_ONLY : VALIDATE_COMPATIBLE;
    report_print(&report, mode, stdout);
    const size_t fatal = report_fatal_count(&report);
    if (report.count == 0) printf("no findings\n");
    else printf("%zu findings, %zu of them errors in compatible mode\n", report.count, fatal);
    report_free(&report);
    if (mode == VALIDATE_COMPATIBLE && fatal > 0) {
        exchange_fail(err, "%zu limits of the original executable are exceeded", fatal);
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    if (argc < 2) { usage(stderr); return 1; }
    const char *command = argv[1];
    if (strcmp(command, "slot") == 0) {
        ExchangeError slot_err = {{0}};
        if (slot_command(argc - 1, argv + 1, &slot_err)) return 0;
        fprintf(stderr, "tnfs-studio: %s\n", slot_err.message);
        return 1;
    }
    const int unpack = strcmp(command, "unpack") == 0 || strcmp(command, "export") == 0;
    const int pack = strcmp(command, "pack") == 0 || strcmp(command, "import") == 0;
    const int validate = strcmp(command, "validate") == 0;
    if (strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0 || strcmp(command, "help") == 0) { usage(stdout); return 0; }
    if (!unpack && !pack && !validate) { fprintf(stderr, "tnfs-studio: unknown command '%s'\n", command); usage(stderr); return 1; }
    Args a;
    if (!parse(argc, argv, &a) || a.positionals != (validate ? 1 : 2)) { usage(stderr); return 1; }

    ExchangeError err = {{0}};
    uint8_t palette[768];
    const uint8_t *palette_ptr = NULL;
    if (a.palette != NULL) {
        if (!load_palette(a.palette, palette, &err)) { fprintf(stderr, "tnfs-studio: %s\n", err.message); return 1; }
        palette_ptr = palette;
    }
    const int ok = validate ? do_validate(&a, &err) : unpack ? do_unpack(&a, palette_ptr, &err) : do_pack(&a, palette_ptr, &err);
    if (!ok) { fprintf(stderr, "tnfs-studio: %s\n", err.message); return 1; }
    return 0;
}
