/* «Show texture file» (editor/texture_file.c): the bank PNG of a texture of a car kit, found by its bank's entries.tsv (name_hex, file).
 * The kit's model/near|far/bank (formats/car_kit_paths.h) and the older layout modelo/c1|c3 both answer; anything else (no kit, no such
 * texture, a row whose file is missing, a palette) does not, so the button is not shown. Fixtures are made here. */
#include "texture_file.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char base[512];

static void put(const char *rel, const char *text)
{
    char p[1024];
    snprintf(p, sizeof p, "%s/%s", base, rel);
    for (char *s = p + strlen(base) + 1; (s = strchr(s, '/')) != NULL; s++) {
        *s = '\0';
        mkdir(p, 0777);
        *s = '/';
    }
    FILE *f = fopen(p, "wb");
    if (f != NULL) fputs(text, f), fclose(f);
}

static const char HEAD[] = "# index\tname_hex\tlabel\ttype\tblock_size\twidth\theight\tmisc0\tmisc1\tmisc2\tmisc3\tfile\ttail_hex\tpos\n";

static int found(const char *kit, int level, const char *name, const char *want_rel)
{
    char dir[1024], out[1024], want[1024];
    snprintf(dir, sizeof dir, "%s/%s", base, kit);
    memset(out, 'x', sizeof out);
    const int ok = texture_file_of(dir, level, name, out, sizeof out);
    if (want_rel == NULL) return !ok && out[0] == '\0';
    snprintf(want, sizeof want, "%s/%s/%s", base, kit, want_rel);
    if (!ok || strcmp(out, want) != 0) fprintf(stderr, "  %s %d %s: got %d '%s', want '%s'\n", kit, level, name, ok, out, want);
    return ok && strcmp(out, want) == 0;
}

int main(void)
{
    snprintf(base, sizeof base, "%s/tex_file_XXXXXX", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp");
    if (mkdtemp(base) == NULL) return 1;

    /* the new kit: near bank with a palette, frnt and the unsafe name !xxx; far bank with its own frnt */
    char rows[1024];
    snprintf(rows, sizeof rows, "%s0\t2150414c\t_PAL\t36\t0\t256\t3\t0\t0\t0\t0\t0000__PAL.bin\t\t0\n"
                                "1\t66726e74\tfrnt\t123\t0\t8\t8\t0\t0\t0\t0\t0001_frnt.png\t\t1\n"
                                "2\t21787878\t_xxx\t123\t0\t8\t8\t0\t0\t0\t0\t0002__xxx.png\t\t2\n", HEAD);
    put("kit/mod.ini", "id=k\n");
    put("kit/model/near/bank/entries.tsv", rows);
    put("kit/model/near/bank/0000__PAL.bin", "p");
    put("kit/model/near/bank/0001_frnt.png", "n");
    put("kit/model/near/bank/0002__xxx.png", "x");
    snprintf(rows, sizeof rows, "%s0\t66726e74\tfrnt\t123\t0\t4\t4\t0\t0\t0\t0\t0003_frnt.png\t\t0\n", HEAD);
    put("kit/model/far/bank/entries.tsv", rows);
    put("kit/model/far/bank/0003_frnt.png", "f");

    CHECK(found("kit", 0, "frnt", "model/near/bank/0001_frnt.png"));
    CHECK(found("kit", 1, "frnt", "model/far/bank/0003_frnt.png"));
    CHECK(found("kit", 0, "!xxx", "model/near/bank/0002__xxx.png"));   /* by its name's bytes, not its file-safe label */
    CHECK(found("kit", 0, "_xxx", NULL));                               /* the label is not the name */
    CHECK(found("kit", 0, "!PAL", NULL));                               /* a palette is no texture to paint */
    CHECK(found("kit", 0, "side", NULL));                               /* no such texture */
    CHECK(found("kit", 2, "frnt", NULL));                               /* no such level */

    /* a row whose file is gone */
    snprintf(rows, sizeof rows, "%s0\t73696465\tside\t123\t0\t8\t8\t0\t0\t0\t0\t0000_side.png\t\t0\n", HEAD);
    put("gone/model/near/bank/entries.tsv", rows);
    CHECK(found("gone", 0, "side", NULL));

    /* the older layout: modelo/c1 the near bank, c3 the far one, entries.tsv without pos */
    put("old/modelo/c1/entries.tsv", "# index\tname_hex\tlabel\ttype\tblock_size\twidth\theight\tmisc0\tmisc1\tmisc2\tmisc3\tfile\ttail_hex\n"
                                     "1\t746f7076\ttopv\t123\t0\t8\t8\t0\t0\t0\t0\t0001_topv.png\t0000002810ff\n");
    put("old/modelo/c1/0001_topv.png", "t");
    put("old/modelo/c3/entries.tsv", "# index\tname_hex\n0\t746f7076\ttopv\t123\t0\t8\t8\t0\t0\t0\t0\t0000_topv.png\t\n");
    put("old/modelo/c3/0000_topv.png", "t");
    CHECK(found("old", 0, "topv", "modelo/c1/0001_topv.png"));
    CHECK(found("old", 1, "topv", "modelo/c3/0000_topv.png"));

    /* not a kit: a piece (mod.ini, no bank), a folder that is not there */
    put("piece/mod.ini", "id=p\n");
    CHECK(found("piece", 0, "frnt", NULL));
    CHECK(found("nothing", 0, "frnt", NULL));
    char out[8];
    CHECK(!texture_file_of(NULL, 0, "frnt", out, sizeof out));
    CHECK(!texture_file_of(base, 0, NULL, out, sizeof out));

    /* a path that does not fit is refused, not cut */
    char kit[1024], tiny[16];
    snprintf(kit, sizeof kit, "%s/kit", base);
    CHECK(!texture_file_of(kit, 0, "frnt", tiny, sizeof tiny));

    char cmd[600];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", base);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", base);
    return TEST_REPORT();
}
