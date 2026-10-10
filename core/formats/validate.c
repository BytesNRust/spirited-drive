#include "formats/validate.h"

#include "formats/le.h"
#include "formats/pbs.h"
#include "formats/pdn.h"
#include "formats/qfs.h"
#include "formats/validate_internal.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

enum { PATHS_DAT_SIZE = 0x5f0, CONFIG_DAT_SIZE = 24402, SAVE_SIZE = 0x5b6, NAME_MAX_BYTES = 127, SLACK = 0x400 };

void report_init(Report *r) { memset(r, 0, sizeof *r); }

void report_free(Report *r)
{
    free(r->item);
    memset(r, 0, sizeof *r);
}

void report_add(Report *r, const char *limit_id, int64_t value, const char *format, ...)
{
    const Limit *limit = limit_find(limit_id);
    if (limit == NULL) return;   /* a typo here is caught by the test that visits every id */
    if (r->count == r->cap) {
        const size_t cap = r->cap ? r->cap * 2 : 16;
        Finding *grown = realloc(r->item, cap * sizeof *grown);
        if (grown == NULL) return;
        r->item = grown;
        r->cap = cap;
    }
    Finding *f = &r->item[r->count++];
    f->limit = limit;
    f->value = value;
    va_list args;
    va_start(args, format);
    vsnprintf(f->item, sizeof f->item, format, args);
    va_end(args);
}

size_t report_fatal_count(const Report *r)
{
    size_t n = 0;
    for (size_t i = 0; i < r->count; ++i) n += r->item[i].limit->fatal != 0;
    return n;
}

void report_print(const Report *r, ValidateMode mode, FILE *out)
{
    for (size_t i = 0; i < r->count; ++i) {
        const Finding *f = &r->item[i];
        const char *level = (mode == VALIDATE_COMPATIBLE && f->limit->fatal) ? "error" : "warning";
        fprintf(out, "%s %s: %s: %s", level, f->limit->id, f->item, f->limit->what);
        if (f->limit->max != 0) fprintf(out, " [value %lld, maximum %lld]", (long long)f->value, (long long)f->limit->max);
        else if (f->value != 0) fprintf(out, " [value %lld]", (long long)f->value);
        fprintf(out, " (EXE %s%s)\n", f->limit->exe, f->limit->inferred ? "; inferred from the memory layout" : "");
    }
}

int validate_has_extension(const char *name, const char *ext)
{
    const size_t n = strlen(name), e = strlen(ext);
    if (n < e) return 0;
    for (size_t i = 0; i < e; ++i)
        if (tolower((unsigned char)name[n - e + i]) != ext[i]) return 0;
    return 1;
}

const char *validate_base_name(const char *name)
{
    const char *slash = strrchr(name, '/');
    return slash ? slash + 1 : name;
}

static int same_name(const char *a, const char *b)
{
    return strlen(a) == strlen(b) && validate_has_extension(a, b);
}

/* The codec byte values 004a261c accepts. */
static int known_codec(uint8_t first)
{
    const int c = first & 0xFE;
    return c == 0x10 || c == 0x30 || c == 0x32 || c == 0x34 || c == 0x46 || c == 0x4a;
}

static void check_pbs(const uint8_t *d, size_t n, const char *name, Report *r)
{
    if (n != PBS_SIZE) { report_add(r, "PBS.SIZE", (int64_t)n, "%s: unpacks to %zu bytes", name, n); return; }
    if (!pbs_checksum_ok(d))
        report_add(r, "PBS.CHECKSUM", pbs_checksum(d), "%s: the sum is %d, the word at offset 0x%x holds %d", name, pbs_checksum(d),
                   PBS_CHECKSUM_OFFSET, (int32_t)le_u32(d + PBS_CHECKSUM_OFFSET));
}

void validate_file(const uint8_t *file, size_t size, const char *name, Report *r)
{
    const char *base = validate_base_name(name);
    if (strlen(base) > NAME_MAX_BYTES) report_add(r, "FILE.NAME_LENGTH", (int64_t)strlen(base), "%s", name);
    if (same_name(base, "paths.dat") && size != PATHS_DAT_SIZE) report_add(r, "FILE.FIXED_SIZE", (int64_t)size, "%s: %zu bytes, the EXE reads %d", name, size, PATHS_DAT_SIZE);
    if (same_name(base, "config.dat") && size != CONFIG_DAT_SIZE) report_add(r, "FILE.FIXED_SIZE", (int64_t)size, "%s: %zu bytes, the EXE reads %d", name, size, CONFIG_DAT_SIZE);
    if (validate_has_extension(base, ".sav") && size != SAVE_SIZE) report_add(r, "FILE.FIXED_SIZE", (int64_t)size, "%s: %zu bytes, the EXE loads %d", name, size, SAVE_SIZE);

    const int shape_file = validate_has_extension(base, ".qfs") || validate_has_extension(base, ".fsh");
    if (shape_file && size >= 2 && (file[1] == 0xfb || file[1] == 0x32) && !known_codec(file[0]))
        report_add(r, "QFS.PACK_CODE", file[0], "%s: first byte 0x%02x at offset 0", name, file[0]);

    const uint8_t *payload = file;
    size_t n = size;
    uint8_t *unpacked = NULL;
    size_t unpacked_size = 0;
    if (qfs_is_compressed(file, size) && qfs_decompressed_size(file, size, &unpacked_size) == QFS_OK) {
        if (size > unpacked_size + SLACK)
            report_add(r, "QFS.INPLACE_SIZE", (int64_t)(size - unpacked_size), "%s: packed %zu bytes, unpacked %zu: %zu over", name, size, unpacked_size, size - unpacked_size);
        unpacked = malloc(unpacked_size ? unpacked_size : 1);
        size_t at = 0;
        QfsResult q = unpacked ? qfs_decompress_at(file, size, unpacked, unpacked_size, &at) : QFS_ERR_OVERFLOW;
        if (q != QFS_OK) {
            report_add(r, "FILE.READABLE", 0, "%s: damaged QFS stream: %s at offset 0x%zx", name, qfs_result_str(q), at);
            free(unpacked);
            return;
        }
        if ((file[0] & 0xFE) == 0x10 && size <= unpacked_size + SLACK && !qfs_inplace_ok(file, size, unpacked_size))
            report_add(r, "QFS.INPLACE_ORDER", 0, "%s: the unpacked bytes would overtake the packed ones", name);
        payload = unpacked;
        n = unpacked_size;
    }

    if (validate_has_extension(base, ".pbs")) check_pbs(payload, n, name, r);
    else if (validate_has_extension(base, ".pdn")) { if (n != PDN_SIZE) report_add(r, "PDN.SIZE", (int64_t)n, "%s: unpacks to %zu bytes", name, n); }
    else if (validate_has_extension(base, ".tri")) validate_tri(payload, n, name, r);
    else if (validate_has_extension(base, ".bnk")) validate_bnk(payload, n, name, r);
    else if (validate_has_extension(base, ".cfm")) validate_car_cfm(payload, n, name, r);
    else if (validate_has_extension(base, ".fam")) validate_track_fam(payload, n, name, r);
    else if (validate_has_extension(base, ".fsh") || validate_has_extension(base, ".qfs")) { if (n >= 4 && memcmp(payload, "SHPI", 4) == 0) validate_shpi(payload, n, name, r); }
    else if (strlen(base) > 4 && validate_has_extension(base, "info")) validate_info(payload, n, name, r);
    free(unpacked);
}

/* ------------------------------------------------------------------ across files */

typedef enum { ROLE_NONE, ROLE_PLAYER, ROLE_OPPONENT, ROLE_COLLISION, ROLE_FRONT, ROLE_FM } BankRole;

/* Which of the EXE's load groups a bank belongs to (inventory §6.5, STATE: 00443924, 004648dc). */
static BankRole bank_role(const char *base)
{
    if (!validate_has_extension(base, ".bnk")) return ROLE_NONE;
    char stem[32];
    size_t i = 0;
    for (; base[i] && base[i] != '.' && i < sizeof stem - 1; ++i) stem[i] = (char)tolower((unsigned char)base[i]);
    stem[i] = '\0';
    static const char *const player[] = { "supra_sw", "diablosw", "911_sw", "zr1_sw", "tr512_sw", "vipersw", "nsx_sw", "rx7_sw", "traffc" };
    static const char *const opponent[] = { "osupmb", "odiamb", "o911mb", "ozr1mb", "o512mb", "ovipmb", "onsxmb", "orx7mb", "traffco" };
    static const char *const collision[] = { "collswwt", "collsbwt", "collswmt", "collsbmt", "colswwt3", "colswmt3" };
    for (size_t k = 0; k < sizeof player / sizeof player[0]; ++k) if (strcmp(stem, player[k]) == 0) return ROLE_PLAYER;
    for (size_t k = 0; k < sizeof opponent / sizeof opponent[0]; ++k) if (strcmp(stem, opponent[k]) == 0) return ROLE_OPPONENT;
    for (size_t k = 0; k < sizeof collision / sizeof collision[0]; ++k) if (strcmp(stem, collision[k]) == 0) return ROLE_COLLISION;
    if (strcmp(stem, "front_mw") == 0) return ROLE_FRONT;
    if (strcmp(stem, "nfs_fmmb") == 0) return ROLE_FM;
    return ROLE_NONE;
}

static int stem_in(const char *stem, const char *const *list, size_t n)
{
    for (size_t k = 0; k < n; ++k)
        if (strcmp(stem, list[k]) == 0) return 1;
    return 0;
}

/* Built-in INFO rows 0043a370 picks by stem: cl1..cy3 (either case) and tr1..tr7. */
static int has_builtin_info(const char *stem)
{
    static const char *const stems[] = { "cl1", "cl2", "cl3", "cy1", "cy2", "cy3", "tr1", "tr2", "tr3", "tr4", "tr5", "tr6", "tr7" };
    return stem_in(stem, stems, sizeof stems / sizeof stems[0]);
}

/* The TRI stems the EXE can open (004af2fc..004af3dc as the CD uses them); AL22.TRI and the like are never loaded. */
static int track_is_loadable(const char *stem)
{
    static const char *const stems[] = { "al1", "al2", "al3", "cl1", "cl2", "cl3", "cy1", "cy2", "cy3", "tr1", "tr2", "tr3", "tr4", "tr5", "tr6", "tr7" };
    return stem_in(stem, stems, sizeof stems / sizeof stems[0]);
}

void validate_tree(const FileSet *files, Report *r)
{
    for (size_t i = 0; i < files->count; ++i) validate_file(files->file[i].data, files->file[i].size, files->file[i].name, r);

    /* sound numbers: banks of different load groups are loaded together and must not share a number */
    for (size_t a = 0; a < files->count; ++a) {
        const BankRole ra = bank_role(validate_base_name(files->file[a].name));
        if (ra == ROLE_NONE || files->file[a].size < 512) continue;
        for (size_t b = a + 1; b < files->count; ++b) {
            const BankRole rb = bank_role(validate_base_name(files->file[b].name));
            if (rb == ROLE_NONE || rb == ra || files->file[b].size < 512) continue;
            for (int slot = 0; slot < 128; ++slot)
                if (le_u32(files->file[a].data + 4 * slot) != 0 && le_u32(files->file[b].data + 4 * slot) != 0)
                    report_add(r, "BNK.SOUND_NUMBERS", slot, "%s and %s both hold sound number %d", files->file[a].name, files->file[b].name, slot);
        }
    }

    /* a track the EXE has no built-in INFO row for needs its INFO file next to the TRI */
    for (size_t i = 0; i < files->count; ++i) {
        const char *name = files->file[i].name;
        const char *base = validate_base_name(name);
        if (!validate_has_extension(base, ".tri") || strlen(base) > 24) continue;
        char stem[32] = {0}, info[48];
        size_t k = 0;
        for (; base[k] != '.'; ++k) stem[k] = (char)tolower((unsigned char)base[k]);
        if (!track_is_loadable(stem) || has_builtin_info(stem)) continue;
        int found = 0;
        for (size_t j = 0; j < files->count && !found; ++j) {
            const char *other = validate_base_name(files->file[j].name);
            snprintf(info, sizeof info, "%.*sinfo", (int)k, stem);
            found = strlen(other) == strlen(info) && validate_has_extension(other, info);
        }
        if (!found) report_add(r, "INFO.REQUIRED", 0, "%s: no %.*sINFO file next to it", name, (int)k, base);
    }
}
