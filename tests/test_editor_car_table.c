/* Copy to game repo's colors (editor/car_table_rewrite.h) against the repo's own core/formats/car_paint.c (argv[1] is the repo): every row read as the table has it and written back to the same bytes,
 * a changed row rewritten and nothing else touched, a format it does not know refused with nothing written, and the game's own source compiled and run with the new row. */
#include "car_table_rewrite.h"
#include "formats/car_paint.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    char *t = malloc(200000);
    *len = t != NULL ? fread(t, 1, 199999, f) : 0;
    fclose(f);
    if (t != NULL) t[*len] = '\0';
    return t;
}

/* the table's colors as a card holds them: a name over a card's 24 characters is cut (Export original does it) */
static int paints_of(int model, ModPaint *out)
{
    const int n = car_paint_count(model);
    for (int k = 0; k < n; k++) snprintf(out[k].name, sizeof out[k].name, "%s", car_paint(model, k)->name), memcpy(out[k].rgb, car_paint(model, k)->rgb, 3);
    return n;
}

static const char *const STEMS[CAR_PAINT_MODELS] = { "TSupra", "LDiabl", "P911", "CZR1", "F512TR", "DVIPER", "ANSX", "MRX7" };

static void every_row(const char *src, size_t len)
{
    char why[300];
    for (int m = 0; m < CAR_PAINT_MODELS; m++) {
        ModPaint p[CAR_TABLE_MAX_PAINTS];
        int changed = -1;
        const int n = paints_of(m, p);
        char *same = car_table_rewrite(src, len, STEMS[m], p, n, &changed, NULL, NULL, why, sizeof why);
        if (same == NULL) fprintf(stderr, "%s: %s\n", STEMS[m], why);
        CHECK(same != NULL && changed == 0 && strcmp(same, src) == 0);   /* the table's own colors: nothing to write */
        free(same);
        /* another color for the second paint, then back: the same bytes as the file started with (the row's layout is the file's) */
        ModPaint q[CAR_TABLE_MAX_PAINTS];
        memcpy(q, p, sizeof q);
        q[n > 1 ? 1 : 0].rgb[0] ^= 0x55;
        char before[CAR_TABLE_LIST], after[CAR_TABLE_LIST];
        char *edited = car_table_rewrite(src, len, STEMS[m], q, n, &changed, before, after, why, sizeof why);
        CHECK(edited != NULL && changed == 1 && strcmp(edited, src) != 0 && strcmp(before, after) != 0);
        if (edited == NULL) continue;
        CHECK(strstr(before, p[0].name) != NULL);
        char *back = car_table_rewrite(edited, strlen(edited), STEMS[m], p, n, &changed, NULL, NULL, why, sizeof why);
        CHECK(back != NULL && changed == 1 && strcmp(back, src) == 0);
        /* a second time with the colors already in: idempotent */
        char *again = car_table_rewrite(edited, strlen(edited), STEMS[m], q, n, &changed, NULL, NULL, why, sizeof why);
        CHECK(again != NULL && changed == 0 && strcmp(again, edited) == 0);
        /* the other rows, the head and the tail are the file's own: the texts differ only inside the row */
        size_t pre = 0;
        while (src[pre] != '\0' && src[pre] == edited[pre]) pre++;
        size_t suf = 0;
        while (suf < len && suf < strlen(edited) && src[len - 1 - suf] == edited[strlen(edited) - 1 - suf]) suf++;
        const char *row = strstr(src, STEMS[m]);
        CHECK(row != NULL && pre + 1 >= (size_t)(row - src) - 8);   /* the first byte that differs is in this row's lines */
        CHECK(suf > 100);
        free(edited), free(back), free(again);
    }
}

static char *replace_once(const char *src, const char *from, const char *to)
{
    const char *at = strstr(src, from);
    if (at == NULL) return NULL;
    char *out = malloc(strlen(src) + strlen(to) + 1);
    if (out == NULL) return NULL;
    memcpy(out, src, (size_t)(at - src));
    strcpy(out + (at - src), to);
    strcpy(out + (at - src) + strlen(to), at + strlen(from));
    return out;
}

static void count_and_names(const char *src, size_t len)
{
    char why[300];
    ModPaint p[CAR_TABLE_MAX_PAINTS];
    int changed;
    const int n = paints_of(2, p);   /* the 911 */
    CHECK(n == 4);
    /* fewer and then more paints, a long name and a short one: each reads back as written and a write back to the first set is the file's bytes */
    for (int count = 1; count <= 4; count++) {
        ModPaint q[CAR_TABLE_MAX_PAINTS];
        memcpy(q, p, sizeof q);
        snprintf(q[0].name, sizeof q[0].name, "%s", "A name of exactly 24 chr");
        q[0].rgb[2] = (uint8_t)(count * 9);
        char *out = car_table_rewrite(src, len, "P911", q, count, &changed, NULL, NULL, why, sizeof why);
        CHECK(out != NULL && changed == 1);
        if (out == NULL) continue;
        int again_changed = -1;
        char *twice = car_table_rewrite(out, strlen(out), "P911", q, count, &again_changed, NULL, NULL, why, sizeof why);
        CHECK(twice != NULL && again_changed == 0);
        char *back = car_table_rewrite(out, strlen(out), "P911", p, n, &changed, NULL, NULL, why, sizeof why);
        CHECK(back != NULL && strcmp(back, src) == 0);
        free(out), free(twice), free(back);
    }
    ModPaint five[5];
    memcpy(five, p, sizeof p);
    memset(&five[4], 0, sizeof five[4]);
    CHECK(car_table_rewrite(src, len, "P911", five, 5, &changed, NULL, NULL, why, sizeof why) == NULL && strstr(why, "up to 4") != NULL);
    CHECK(car_table_rewrite(src, len, "P911", p, 0, &changed, NULL, NULL, why, sizeof why) == NULL && strstr(why, "no colors") != NULL);
    ModPaint bad[1] = { { "Quote\"d", { 1, 2, 3 } } };
    CHECK(car_table_rewrite(src, len, "P911", bad, 1, &changed, NULL, NULL, why, sizeof why) == NULL && strstr(why, "cannot be written") != NULL);
    ModPaint slash[1] = { { "Back\\slash", { 1, 2, 3 } } };
    CHECK(car_table_rewrite(src, len, "P911", slash, 1, &changed, NULL, NULL, why, sizeof why) == NULL);
}

static void refusals(const char *src, size_t len)
{
    char why[300];
    ModPaint p[CAR_TABLE_MAX_PAINTS];
    int changed = -1;
    const int n = paints_of(2, p);
    p[0].rgb[0] ^= 1;
    CHECK(car_table_rewrite(src, len, "TRAFFC", p, n, &changed, NULL, NULL, why, sizeof why) == NULL && strstr(why, "no row for TRAFFC") != NULL);   /* a car without a row */
    struct { const char *from, *to; const char *word; } odd[] = {
        { "static const CarPaints TABLE[CAR_PAINT_MODELS] = {", "static const CarPaints TABLE2[CAR_PAINT_MODELS] = {", "no `static const" },
        { "{ \"P911\", 4, { 16, 56, 72 },", "{ \"P911\", 4, { 16, 56 },", "row of P911 is not laid out" },
        { "{ \"P911\", 4, { 16, 56, 72 },", "{ \"P911\", 3, { 16, 56, 72 },", "count" },
        { "{ \"Black\", { 14, 14, 16 } }", "{ \"Black\", { 14, 14, 16, 1 } }", "paint of P911 is not laid out" },
        { "{ \"Black\", { 14, 14, 16 } }", "{ \"Bl\\\"ack\", { 14, 14, 16 } }", "paint of P911 is not laid out" },
        { "{ \"Speed Yellow\", { 242, 197, 0 } } } },", "{ \"Speed Yellow\", { 242, 197, 0 } } }", "does not end" },
        { "{ \"Guards Red\", { 179, 18, 30 } }", "{ \"Guards Red\", { 179, 18, 300 } }", "paint of P911 is not laid out" },
    };
    for (size_t i = 0; i < sizeof odd / sizeof odd[0]; i++) {
        char *weird = replace_once(src, odd[i].from, odd[i].to);
        CHECK(weird != NULL);
        if (weird == NULL) continue;
        why[0] = '\0';
        char *out = car_table_rewrite(weird, strlen(weird), "P911", p, n, &changed, NULL, NULL, why, sizeof why);
        CHECK(out == NULL && strstr(why, odd[i].word) != NULL);
        if (out == NULL && strstr(why, odd[i].word) == NULL) fprintf(stderr, "case %zu: why = %s\n", i, why);
        free(out), free(weird);
    }
}

/* the game's own source with the new row compiles and offers the new colors (needs a C compiler on the PATH; without one this is skipped) */
static void compiles(const char *repo, const char *src, size_t len)
{
    if (system("cc --version >/dev/null 2>&1") != 0) {
        printf("skip: no cc\n");
        return;
    }
    char dir[400], cmd[1600], why[300];
    snprintf(dir, sizeof dir, "%s/car_table_XXXXXX", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp");
    if (mkdtemp(dir) == NULL) return;
    ModPaint p[CAR_TABLE_MAX_PAINTS];
    int changed = 0;
    const int n = paints_of(2, p);
    snprintf(p[2].name, sizeof p[2].name, "%s", "Midnight Purple");
    p[2].rgb[0] = 90, p[2].rgb[1] = 20, p[2].rgb[2] = 120;
    char *out = car_table_rewrite(src, len, "P911", p, n - 1, &changed, NULL, NULL, why, sizeof why);
    CHECK(out != NULL && changed == 1);
    if (out != NULL) {
        char file[500];
        snprintf(file, sizeof file, "%s/car_paint.c", dir);
        FILE *f = fopen(file, "wb");
        if (f != NULL) fputs(out, f), fclose(f);
        snprintf(file, sizeof file, "%s/main.c", dir);
        f = fopen(file, "wb");
        if (f != NULL) {
            fputs("#include \"formats/car_paint.h\"\n#include <stdio.h>\nint main(void)\n{\n    for (int k = 0; k < car_paint_count(2); k++) printf(\"%s %d,%d,%d\\n\", car_paint(2, k)->name, car_paint(2, k)->rgb[0], car_paint(2, k)->rgb[1], car_paint(2, k)->rgb[2]);\n    printf(\"count %d\\n\", car_paint_count(0));\n    return 0;\n}\n", f);
            fclose(f);
        }
        snprintf(cmd, sizeof cmd, "cc -std=c99 -Wall -Werror -I'%s/core' -o '%s/prog' '%s/car_paint.c' '%s/main.c' -lm >'%s/cc.log' 2>&1 && '%s/prog' >'%s/run.txt'", repo, dir, dir, dir, dir, dir, dir);
        CHECK(system(cmd) == 0);
        snprintf(file, sizeof file, "%s/run.txt", dir);
        size_t got = 0;
        char *run = slurp(file, &got);
        CHECK(run != NULL && strstr(run, "Grand Prix White 241,241,238\n") != NULL && strstr(run, "Black 14,14,16\n") != NULL && strstr(run, "Midnight Purple 90,20,120\n") != NULL);
        CHECK(run != NULL && strstr(run, "Speed Yellow") == NULL && strstr(run, "count 3\n") != NULL);   /* the 911 has three, the Supra (another row) its three */
        free(run);
    }
    free(out);
    snprintf(cmd, sizeof cmd, "rm -r '%s'", dir);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", dir);
}

int main(int argc, char **argv)
{
    if (argc < 2) return fprintf(stderr, "usage: test_editor_car_table REPO\n"), 1;
    char path[900];
    size_t len = 0;
    snprintf(path, sizeof path, "%s/core/formats/car_paint.c", argv[1]);
    char *src = slurp(path, &len);
    if (src == NULL) return fprintf(stderr, "cannot read %s\n", path), 1;
    every_row(src, len);
    count_and_names(src, len);
    refusals(src, len);
    compiles(argv[1], src, len);
    free(src);
    return TEST_REPORT();
}
