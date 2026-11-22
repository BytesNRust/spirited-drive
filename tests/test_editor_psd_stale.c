/* Check dash (editor/psd_stale.h): a PSD newer than its PNG, a PNG missing, a PNG copied after its PSD, a backup that is not a PSD, case, the dash, no PSD folder. */
#include "psd_stale.h"
#include "test.h"

#define _XOPEN_SOURCE 700
#include <fcntl.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static char root[512];

static int drop(const char *p, const struct stat *st, int flag, struct FTW *w)
{
    (void)st, (void)flag, (void)w;
    return remove(p);
}

static void path(char *out, size_t cap, const char *a, const char *b, const char *c)
{
    snprintf(out, cap, "%s/%s%s%s%s%s", root, a, b[0] ? "/" : "", b, c[0] ? "/" : "", c);
}

static void touch(const char *rel1, const char *rel2, const char *rel3, time_t when)
{
    char p[1024];
    path(p, sizeof p, rel1, rel2, rel3);
    FILE *f = fopen(p, "wb");
    if (f != NULL) fputs("x", f), fclose(f);
    struct timespec ts[2] = { { when, 0 }, { when, 0 } };
    utimensat(AT_FDCWD, p, ts, 0);
}

static void dir(const char *a, const char *b)
{
    char p[1024];
    path(p, sizeof p, a, b, "");
    mkdir(p, 0777);
}

static const StaleItem *find(const StaleReport *r, const char *psd)
{
    for (int i = 0; i < r->count; ++i)
        if (strcmp(r->item[i].psd, psd) == 0) return &r->item[i];
    return NULL;
}

static void test_judging(void)
{
    dir("art", ""), dir("art", "P911"), dir("proj", ""), dir("proj", "car_masks"), dir("proj", "car_masks/P911"), dir("proj", "dash_masks"), dir("proj", "dash_masks/P911");
    char psds[1024];
    path(psds, sizeof psds, "art", "P911", "");
    const time_t base = 1700000000;
    touch("art/P911", "frnt.psd", "", base + 100);       /* newer than its PNG: stale */
    touch("proj/car_masks/P911", "frnt_mask.png", "", base + 50);
    touch("art/P911", "rear.psd", "", base + 100);       /* no PNG: missing */
    touch("art/P911", "side.psd", "", base + 100);       /* the PNG copied after it: fine */
    touch("proj/car_masks/P911", "SIDE_mask.png", "", base + 500);   /* and spelt in capitals */
    touch("art/P911", "frnt_lo.psd", "", base + 100);    /* the low detail bank */
    touch("proj/car_masks/P911", "frnt_lo_mask.png", "", base + 200);
    touch("art/P911", "dash.before-wipers.psd", "", base + 900);   /* a backup is nobody's PSD */
    touch("art/P911", "notes.txt", "", base + 900);
    touch("art/P911", "dash.psd", "", base + 100);
    touch("proj/dash_masks/P911", "dash_mask.png", "", base + 10);
    StaleReport r;
    char p[1024];
    snprintf(p, sizeof p, "%s/proj", root);
    CHECK(psd_stale_check(psds, p, "P911", 1, &r));
    CHECK(r.count == 5 && r.stale == 3);
    const StaleItem *i = find(&r, "frnt.psd");
    CHECK(i != NULL && i->state == STALE_NEWER && strcmp(i->png, "frnt_mask.png") == 0 && !i->dash);
    i = find(&r, "rear.psd");
    CHECK(i != NULL && i->state == STALE_MISSING);
    i = find(&r, "side.psd");
    CHECK(i != NULL && i->state == STALE_OK);
    i = find(&r, "frnt_lo.psd");
    CHECK(i != NULL && i->state == STALE_OK && strcmp(i->png, "frnt_lo_mask.png") == 0);
    i = find(&r, "dash.psd");
    CHECK(i != NULL && i->state == STALE_NEWER && i->dash && strcmp(i->png, "dash_mask.png") == 0);
    CHECK(find(&r, "dash.before-wipers.psd") == NULL && find(&r, "notes.txt") == NULL);
    char text[512];
    psd_stale_text(&r, text, sizeof text);
    CHECK(strstr(text, "dash.psd is newer than dash_mask.png") != NULL && strstr(text, "rear.psd has no rear_mask.png") != NULL && strstr(text, "side") == NULL);
    /* a car you do not drive has no dash to judge */
    CHECK(psd_stale_check(psds, p, "P911", 0, &r) && r.count == 4 && find(&r, "dash.psd") == NULL);
    /* the dash PNG now newer than its PSD, and in a folder spelt another way (dash_masks go by the stem) */
    touch("proj/dash_masks/P911", "dash_mask.png", "", base + 1000);
    CHECK(psd_stale_check(psds, p, "P911", 1, &r) && find(&r, "dash.psd")->state == STALE_OK);
    /* a dash PSD with no dash mask at all is missing */
    char gone[1024];
    snprintf(gone, sizeof gone, "%s/proj/dash_masks/P911/dash_mask.png", root);
    unlink(gone);
    CHECK(psd_stale_check(psds, p, "P911", 1, &r) && find(&r, "dash.psd")->state == STALE_MISSING);
}

static void test_edges(void)
{
    StaleReport r;
    char text[64] = "x";
    char nowhere[600];
    snprintf(nowhere, sizeof nowhere, "%s/no-such-folder", root);
    CHECK(!psd_stale_check(nowhere, root, "P911", 1, &r));
    dir("empty", "");
    char empty[600];
    snprintf(empty, sizeof empty, "%s/empty", root);
    CHECK(psd_stale_check(empty, root, "P911", 1, &r) && r.count == 0 && r.stale == 0);
    psd_stale_text(&r, text, sizeof text);
    CHECK(text[0] == '\0');
}

/* a kit (dep100): the masks flat in art-generated/car_masks and art-generated/dash_masks, no folder of the car's name below them */
static void test_flat(void)
{
    dir("kit", ""), dir("kit", "art-source"), dir("kit", "art-source/masks"), dir("kit", "art-generated"), dir("kit", "art-generated/car_masks"), dir("kit", "art-generated/dash_masks");
    const time_t base = 1700000000;
    touch("kit/art-source/masks", "frnt.psd", "", base + 100);
    touch("kit/art-generated/car_masks", "frnt_mask.png", "", base + 50);     /* older: stale */
    touch("kit/art-source/masks", "rear.psd", "", base + 100);                 /* no PNG */
    touch("kit/art-source/masks", "dash.psd", "", base + 100);
    touch("kit/art-generated/dash_masks", "dash_mask.png", "", base + 200);    /* newer than its PSD: fine */
    char psds[1024], masks[1024], dash[1024];
    path(psds, sizeof psds, "kit/art-source", "masks", "");
    path(masks, sizeof masks, "kit/art-generated", "car_masks", "");
    path(dash, sizeof dash, "kit/art-generated", "dash_masks", "");
    StaleReport r;
    CHECK(psd_stale_check_flat(psds, masks, dash, 1, &r) && r.count == 3 && r.stale == 2);
    CHECK(find(&r, "frnt.psd") != NULL && find(&r, "frnt.psd")->state == STALE_NEWER);
    CHECK(find(&r, "rear.psd") != NULL && find(&r, "rear.psd")->state == STALE_MISSING);
    CHECK(find(&r, "dash.psd") != NULL && find(&r, "dash.psd")->state == STALE_OK && find(&r, "dash.psd")->dash == 1);
    CHECK(psd_stale_check_flat(psds, masks, dash, 0, &r) && r.count == 2 && find(&r, "dash.psd") == NULL);   /* a car with no dash: its dash.psd is nobody's */
    char none[1024];
    path(none, sizeof none, "kit", "no-such", "");
    CHECK(psd_stale_check_flat(psds, none, none, 1, &r) && r.stale == 3 && find(&r, "dash.psd")->state == STALE_MISSING);   /* the output folders not made yet: all missing */
    CHECK(!psd_stale_check_flat(none, masks, dash, 1, &r));   /* no PSD folder */
}

int main(void)
{
    const char *tmp = getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp";
    snprintf(root, sizeof root, "%s/psd-stale-test-%d", tmp, (int)getpid());
    mkdir(root, 0777);
    test_judging();
    test_edges();
    test_flat();
    nftw(root, drop, 8, FTW_DEPTH | FTW_PHYS);
    return TEST_REPORT();
}
