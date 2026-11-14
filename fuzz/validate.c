/* The mod check every mod file goes through (platform_mods_sdl3.c): the first byte picks the file's name, so its
 * extension routes the rest to the reader the check runs (TRI, FAM, CFM, BNK, PBS, PDN, QFS/FSH, INFO, SAV) */
#include "fuzz/fuzz.h"
#include "formats/validate.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static const char *const NAMES[] = { "tr1.tri", "tr1.fam", "car.cfm", "car.bnk", "car.pbs", "car.pdn",
                                         "title.qfs", "art.fsh", "tr1info", "config.dat", "paths.dat", "slot.sav",
                                         "sw.bnk", "front_mw.bnk", "nfs_fmmb.bnk", "readme.txt" };
    if (size == 0) return 0;
    Report r;
    report_init(&r);
    validate_file(data + 1, size - 1, NAMES[data[0] % (sizeof NAMES / sizeof NAMES[0])], &r);
    char text[512];
    (void)report_text(&r, VALIDATE_COMPATIBLE, text, sizeof text);
    report_free(&r);
    return 0;
}
