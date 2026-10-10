/* validate for bitmaps (SHPI), sound banks (BNK) and the track INFO text files. */
#include "formats/bnk.h"
#include "formats/fsh.h"
#include "formats/le.h"
#include "formats/validate_internal.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Types 00496f38 maps to a bit depth, with 0x68..0x6f aliasing 0x78..0x7f, plus the two palettes. */
static int shape_type_ok(uint8_t t)
{
    if (t >= 0x68 && t <= 0x6f) t = (uint8_t)(t + 0x10);
    return t == 0x7a || t == 0x7b || t == 0x78 || t == 0x7e || t == 0x7f || t == 0x7d || t == FSH_TYPE_PALETTE_6BIT || t == FSH_TYPE_PALETTE_8BIT;
}

void validate_shpi(const uint8_t *d, size_t n, const char *name, Report *r)
{
    Fsh fsh;
    if (fsh_open(&fsh, d, n) != FSH_OK) {
        report_add(r, "FILE.READABLE", 0, "%s: not a readable SHPI directory (file is %zu bytes)", name, n);
        return;
    }
    for (uint32_t i = 0; i < fsh.count; ++i) {
        FshEntry e;
        if (fsh_entry(&fsh, i, &e) != FSH_OK) {
            report_add(r, "FILE.READABLE", 0, "%s: entry %u lies outside the file (directory offset 0x%x)", name, i, 0x14 + 8 * i);
            continue;
        }
        if (!shape_type_ok(e.type))
            report_add(r, "SHPI.SHAPE_TYPE", e.type, "%s: entry %u '%.4s' has type 0x%02x (offset 0x%x)", name, i, e.name, e.type, (unsigned)le_u32(d + 0x14 + 8 * i));
    }
}

/* Banks the EXE opens by name (inventory §6.5); the others are never loaded, so their contents cannot matter. */
static int bank_is_opened(const char *base)
{
    static const char *const names[] = {
        "supra_sw", "diablosw", "911_sw", "zr1_sw", "tr512_sw", "vipersw", "nsx_sw", "rx7_sw", "traffc",
        "osupmb", "odiamb", "o911mb", "ozr1mb", "o512mb", "ovipmb", "onsxmb", "orx7mb", "traffco",
        "collswwt", "collsbwt", "collswmt", "collsbmt", "colswwt3", "colswmt3", "front_mw", "nfs_fmmb"
    };
    char stem[32];
    size_t i = 0;
    for (; base[i] && base[i] != '.' && i < sizeof stem - 1; ++i) stem[i] = (char)tolower((unsigned char)base[i]);
    stem[i] = '\0';
    for (size_t k = 0; k < sizeof names / sizeof names[0]; ++k)
        if (strcmp(stem, names[k]) == 0) return 1;
    return 0;
}

void validate_bnk(const uint8_t *d, size_t n, const char *name, Report *r)
{
    Bnk bnk;
    if (bnk_open(&bnk, d, n) != BNK_OK) {
        report_add(r, "FILE.READABLE", 0, "%s: shorter than the 512-byte offset table (file is %zu bytes)", name, n);
        return;
    }
    const int opened = bank_is_opened(validate_base_name(name));
    for (int slot = 0; slot < BNK_SLOTS; ++slot) {
        BnkSample s;
        const BnkResult br = bnk_sample(&bnk, slot, &s);
        if (br == BNK_ABSENT) continue;
        if (br == BNK_ERR_DATA) {
            if (opened) report_add(r, "BNK.DATA_BOUNDS", 0, "%s: slot %d claims %u frames at offset 0x%x, past the end of the file (%zu bytes)", name, slot, s.length, s.data_offset, n);
        } else if (br != BNK_OK) {
            report_add(r, "FILE.READABLE", 0, "%s: slot %d: %s (offset table entry 0x%x)", name, slot, bnk_result_str(br), 4 * slot);
        }
    }
}

/* The track INFO text (0043a370 through the line reader 00429020): fgets of 0x7f bytes, lines starting with '#',
 * LF or CR skipped, 29 data lines of which lines 9 and 10 are the sky colours, three integers each. */
void validate_info(const uint8_t *d, size_t n, const char *name, Report *r)
{
    enum { DATA_LINES = 29, COLOUR_TOP = 9, COLOUR_BOTTOM = 10, LINE_MAX = 126 };
    int data = 0;
    size_t at = 0;
    while (at < n) {
        const uint8_t *line = d + at;
        const uint8_t *nl = memchr(line, '\n', n - at);
        const size_t len = nl ? (size_t)(nl - line) + 1 : n - at;   /* with its terminator: what fgets sees */
        if (len > LINE_MAX) report_add(r, "INFO.LINE_LENGTH", (int64_t)len, "%s: line at offset 0x%zx is %zu bytes with its terminator", name, at, len);
        if (line[0] != '#' && line[0] != '\n' && line[0] != '\r') {
            ++data;
            if (data == COLOUR_TOP || data == COLOUR_BOTTOM) {
                char text[LINE_MAX + 8];
                const size_t copy = len < sizeof text - 1 ? len : sizeof text - 1;
                memcpy(text, line, copy);
                text[copy] = '\0';
                char *p = text;
                for (int k = 0; k < 3; ++k) {
                    char *end;
                    const long v = strtol(p, &end, 10);
                    if (end == p) break;
                    if (v < 0 || v > 31) report_add(r, "INFO.COLOUR", v, "%s: data line %d (offset 0x%zx): value %ld", name, data, at, v);
                    p = end;
                }
            }
        }
        at += len;
    }
    if (data < DATA_LINES) report_add(r, "INFO.DATA_LINES", data, "%s: %d data lines", name, data);
}
