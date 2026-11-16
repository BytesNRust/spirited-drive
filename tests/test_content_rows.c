/* dep59 step 4b (T4): the CONTENT screen's rows (game/frontend/content_rows.h): per tab, an own edition's pieces in its order and on, then
 * the stock pieces, the installed mods and the removed ones it does not name, off; the standard edition's stock pieces on and locked;
 * "missing" and "(removed)"; the All row's three states; the box, All and the arrows as new editions; at least one car and one track. */
#include "game/frontend/content_rows.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

/* two strings the same; the pair printed when not */
#define CHECK_EQ_STR(actual, expected) do { \
        const char *a_ = (actual), *e_ = (expected); \
        if (strcmp(a_, e_) != 0) fprintf(stderr, "%s:%d: \"%s\", expected \"%s\"\n", __FILE__, __LINE__, a_, e_); \
        CHECK(strcmp(a_, e_) == 0); \
    } while (0)

static ModInfo mod(const char *id, const char *name, const char *version, int type)
{
    ModInfo m;
    memset(&m, 0, sizeof m);
    snprintf(m.id, sizeof m.id, "%s", id);
    snprintf(m.name, sizeof m.name, "%s", name);
    snprintf(m.version, sizeof m.version, "%s", version);
    m.type = type;
    return m;
}

/* installed: two cars (the second with a backup), a track, a tweak; removed: a car and a tweak */
static ModInfo installed[4], removed[2];
static int backups[4] = { 0, 1, 0, 0 };
static ContentMods mods;

static void setup(void)
{
    installed[0] = mod("porsche-993", "Porsche 993", "1.2", MOD_TYPE_CAR);
    installed[1] = mod("test-car", "Test Car", "0.9", MOD_TYPE_CAR);
    installed[2] = mod("night-ring", "Night Ring", "2.0", MOD_TYPE_TRACK);
    installed[3] = mod("grippy", "Grippy Tyres", "1.0", MOD_TYPE_TWEAK);
    removed[0] = mod("old-car", "Old Car", "0.1", MOD_TYPE_CAR);
    removed[1] = mod("old-tweak", "Old Tweak", "3.0", MOD_TYPE_TWEAK);
    mods = (ContentMods){ installed, backups, 4, removed, 2 };
}

static Edition edition(const char *cars, const char *tracks, const char *tweaks)
{
    char text[1024];
    snprintf(text, sizeof text, "name=Mine\ncars=%s\ntracks=%s\ntweaks=%s\n", cars, tracks, tweaks);
    Edition e;
    char why[128];
    CHECK(edition_parse((const uint8_t *)text, strlen(text), &e, why, sizeof why));
    return e;
}

static const ContentRow *row_of(const ContentRows *r, const char *id)
{
    for (int i = 0; i < r->count; i++)
        if (strcmp(r->row[i].id, id) == 0) return &r->row[i];
    return NULL;
}

static int index_of(const ContentRows *r, const char *id)
{
    for (int i = 0; i < r->count; i++)
        if (strcmp(r->row[i].id, id) == 0) return i;
    return -1;
}

/* the rows' ids joined by spaces */
static const char *ids(const ContentRows *r)
{
    static char out[2048];
    out[0] = '\0';
    for (int i = 0; i < r->count; i++)
        snprintf(out + strlen(out), sizeof out - strlen(out), "%s%s", i > 0 ? " " : "", r->row[i].id);
    return out;
}

/* a list of `e` joined by commas */
static const char *list(const Edition *e, int tab)
{
    static char out[2048];
    out[0] = '\0';
    const int n = tab == CONTENT_CARS ? e->cars : tab == CONTENT_TRACKS ? e->tracks : e->tweaks;
    for (int i = 0; i < n; i++) {
        const char *id = tab == CONTENT_CARS ? e->car[i] : tab == CONTENT_TRACKS ? e->track[i] : e->tweak[i];
        snprintf(out + strlen(out), sizeof out - strlen(out), "%s%s", i > 0 ? "," : "", id);
    }
    return out;
}

static void test_the_standard_edition(void)
{
    ContentRows r;
    content_rows_build(&r, NULL, CONTENT_CARS, &mods);
    CHECK_EQ_INT(r.locked, 1);
    CHECK_EQ_STR(ids(&r), "rx7 nsx supra 911 zr1 viper 512tr diablo warrior porsche-993 test-car old-car");
    for (int i = 0; i < r.count; i++) {
        CHECK_EQ_INT(r.row[i].switchable, 0);
        CHECK_EQ_INT(r.row[i].movable, 0);
        CHECK_EQ_INT(r.row[i].on, r.row[i].kind == CONTENT_ROW_GAME);
    }
    CHECK_EQ_STR(row_of(&r, "diablo")->name, "Lamborghini Diablo VT");
    CHECK_EQ_STR(row_of(&r, "diablo")->version, "");
    CHECK_EQ_INT(row_of(&r, "porsche-993")->kind, CONTENT_ROW_MOD);
    CHECK_EQ_STR(row_of(&r, "porsche-993")->version, "1.2");
    CHECK_EQ_INT(row_of(&r, "old-car")->kind, CONTENT_ROW_REMOVED);
    content_rows_build(&r, NULL, CONTENT_TRACKS, &mods);
    CHECK_EQ_STR(ids(&r), "city coastal alpine rusty autumn burnt vertigo transtropolis vegas night-ring");   /* never the hidden one */
    CHECK_EQ_STR(row_of(&r, "rusty")->name, "Rusty Springs");
    content_rows_build(&r, NULL, CONTENT_TWEAKS, &mods);
    CHECK_EQ_STR(ids(&r), "grippy old-tweak");   /* no stock tweak */
    CHECK_EQ_INT(row_of(&r, "grippy")->on, 0);
    /* nothing changes it */
    Edition out;
    content_rows_build(&r, NULL, CONTENT_CARS, &mods);
    CHECK_EQ_INT(content_rows_switch(NULL, &out, CONTENT_CARS, &r, 0), 0);
    CHECK_EQ_INT(content_rows_switch_all(NULL, &out, CONTENT_CARS, &r), 0);
    CHECK_EQ_INT(content_rows_move(NULL, &out, CONTENT_CARS, &r, 1, -1), 0);
    CHECK_EQ_INT(content_rules_ok(NULL, &mods), CONTENT_RULES_OK);
}

static void test_an_own_editions_rows(void)
{
    const Edition e = edition("test-car,diablo,gone-car,rx7", "vegas,hidden,city,lost-track", "grippy,old-tweak");
    ContentRows r;
    content_rows_build(&r, &e, CONTENT_CARS, &mods);
    CHECK_EQ_INT(r.locked, 0);
    /* its own first, in its order and on; then the rest off: stock, installed, removed */
    CHECK_EQ_STR(ids(&r), "test-car diablo gone-car rx7 nsx supra 911 zr1 viper 512tr warrior porsche-993 old-car");
    const ContentRow *gone = row_of(&r, "gone-car");
    CHECK_EQ_INT(gone->kind, CONTENT_ROW_MISSING);
    CHECK_EQ_STR(gone->name, "gone-car");
    CHECK_EQ_INT(gone->on, 1);
    CHECK_EQ_INT(gone->switchable, 1);   /* switched off it leaves the list */
    CHECK_EQ_INT(gone->movable, 0);   /* no arrows on a missing one (the mock-up's); the others still move past it */
    CHECK_EQ_INT(row_of(&r, "test-car")->backup, 1);
    CHECK_EQ_INT(row_of(&r, "porsche-993")->backup, 0);
    CHECK_EQ_INT(row_of(&r, "nsx")->on, 0);
    CHECK_EQ_INT(row_of(&r, "nsx")->switchable, 1);
    CHECK_EQ_INT(row_of(&r, "nsx")->movable, 0);   /* off: no place in the order */
    const ContentRow *old = row_of(&r, "old-car");
    CHECK_EQ_INT(old->kind, CONTENT_ROW_REMOVED);
    CHECK_EQ_INT(old->on, 0);
    CHECK_EQ_INT(old->switchable, 0);
    CHECK_EQ_INT(old->backup, 1);
    CHECK_EQ_STR(old->version, "0.1");
    content_rows_build(&r, &e, CONTENT_TRACKS, &mods);
    CHECK_EQ_STR(ids(&r), "vegas city lost-track coastal alpine rusty autumn burnt vertigo transtropolis night-ring");
    CHECK_EQ_INT(row_of(&r, "lost-track")->kind, CONTENT_ROW_MISSING);
    content_rows_build(&r, &e, CONTENT_TWEAKS, &mods);
    /* a removed mod the edition still names: missing (on, its box takes it off), its backup offered */
    CHECK_EQ_STR(ids(&r), "grippy old-tweak");
    CHECK_EQ_INT(row_of(&r, "old-tweak")->kind, CONTENT_ROW_MISSING);
    CHECK_EQ_INT(row_of(&r, "old-tweak")->on, 1);
    CHECK_EQ_INT(row_of(&r, "old-tweak")->backup, 1);
    /* a mod of another type named in a list is not that list's piece */
    const Edition f = edition("night-ring,rx7", "porsche-993,city", "");
    content_rows_build(&r, &f, CONTENT_CARS, &mods);
    CHECK_EQ_INT(row_of(&r, "night-ring")->kind, CONTENT_ROW_MISSING);
    content_rows_build(&r, &f, CONTENT_TRACKS, &mods);
    CHECK_EQ_INT(row_of(&r, "porsche-993")->kind, CONTENT_ROW_MISSING);
    CHECK_EQ_INT(row_of(&r, "night-ring")->on, 0);
}

static void test_the_all_row(void)
{
    ContentRows r;
    Edition e = edition("rx7", "city", ""), out;
    content_rows_build(&r, &e, CONTENT_TWEAKS, &mods);
    CHECK_EQ_INT(content_rows_all(&r), CONTENT_ALL_OFF);   /* old-tweak is not switchable: grippy alone counts */
    CHECK_EQ_INT(content_rows_switch_all(&e, &out, CONTENT_TWEAKS, &r), 1);
    CHECK_EQ_STR(list(&out, CONTENT_TWEAKS), "grippy");
    content_rows_build(&r, &out, CONTENT_TWEAKS, &mods);
    CHECK_EQ_INT(content_rows_all(&r), CONTENT_ALL_ON);
    content_rows_build(&r, &e, CONTENT_CARS, &mods);
    CHECK_EQ_INT(content_rows_all(&r), CONTENT_ALL_MIXED);
    CHECK_EQ_INT(content_rows_switch_all(&e, &out, CONTENT_CARS, &r), 1);   /* mixed: all on, the ones it had first */
    CHECK_EQ_STR(list(&out, CONTENT_CARS), "rx7,nsx,supra,911,zr1,viper,512tr,diablo,warrior,porsche-993,test-car");
    CHECK_EQ_STR(list(&e, CONTENT_CARS), "rx7");   /* the one given is left as it was */
    Edition all = out;
    content_rows_build(&r, &all, CONTENT_CARS, &mods);
    CHECK_EQ_INT(content_rows_all(&r), CONTENT_ALL_ON);
    CHECK_EQ_INT(content_rows_switch_all(&all, &out, CONTENT_CARS, &r), 1);   /* all on: all off (the rule is the screen's way out) */
    CHECK_EQ_STR(list(&out, CONTENT_CARS), "");
    content_rows_build(&r, &out, CONTENT_CARS, &mods);
    CHECK_EQ_INT(content_rows_all(&r), CONTENT_ALL_OFF);
    /* a missing one counts as on: switched off by All like any other */
    Edition g = edition("gone-car", "city", "");
    content_rows_build(&r, &g, CONTENT_CARS, &mods);
    CHECK_EQ_INT(content_rows_all(&r), CONTENT_ALL_MIXED);
    content_rows_switch_all(&g, &out, CONTENT_CARS, &r);
    CHECK_EQ_STR(list(&out, CONTENT_CARS), "gone-car,rx7,nsx,supra,911,zr1,viper,512tr,diablo,warrior,porsche-993,test-car");
    /* other lists untouched */
    CHECK_EQ_STR(list(&out, CONTENT_TRACKS), "city");
}

static void test_a_box(void)
{
    ContentRows r;
    Edition e = edition("test-car,diablo,gone-car", "city", "grippy"), out;
    content_rows_build(&r, &e, CONTENT_CARS, &mods);
    CHECK_EQ_INT(content_rows_switch(&e, &out, CONTENT_CARS, &r, index_of(&r, "diablo")), 1);   /* off: out of the list */
    CHECK_EQ_STR(list(&out, CONTENT_CARS), "test-car,gone-car");
    CHECK_EQ_INT(content_rows_switch(&e, &out, CONTENT_CARS, &r, index_of(&r, "nsx")), 1);   /* on: at the end */
    CHECK_EQ_STR(list(&out, CONTENT_CARS), "test-car,diablo,gone-car,nsx");
    CHECK_EQ_INT(content_rows_switch(&e, &out, CONTENT_CARS, &r, index_of(&r, "gone-car")), 1);
    CHECK_EQ_STR(list(&out, CONTENT_CARS), "test-car,diablo");
    CHECK_EQ_INT(content_rows_switch(&e, &out, CONTENT_CARS, &r, index_of(&r, "old-car")), 0);   /* removed: no box */
    CHECK_EQ_STR(list(&out, CONTENT_CARS), "test-car,diablo,gone-car");
    CHECK_EQ_INT(content_rows_switch(&e, &out, CONTENT_CARS, &r, -1), 0);
    CHECK_EQ_INT(content_rows_switch(&e, &out, CONTENT_CARS, &r, r.count), 0);
    content_rows_build(&r, &e, CONTENT_TRACKS, &mods);
    CHECK_EQ_INT(content_rows_switch(&e, &out, CONTENT_TRACKS, &r, index_of(&r, "night-ring")), 1);
    CHECK_EQ_STR(list(&out, CONTENT_TRACKS), "city,night-ring");
    CHECK_EQ_STR(list(&out, CONTENT_CARS), "test-car,diablo,gone-car");
    CHECK_EQ_STR(out.name, "Mine");
}

/* a list at its most takes no more */
static void test_a_full_list(void)
{
    Edition e = edition("rx7", "city,coastal,alpine,rusty,autumn,burnt,vertigo,transtropolis", ""), out;
    for (int i = e.tracks; i < EDITION_TRACKS_MAX; i++) snprintf(e.track[e.tracks++], sizeof e.track[0], "track-%d", i);
    ContentRows r;
    content_rows_build(&r, &e, CONTENT_TRACKS, &mods);
    CHECK_EQ_INT(content_rows_switch(&e, &out, CONTENT_TRACKS, &r, index_of(&r, "vegas")), 0);
    CHECK_EQ_INT(out.tracks, EDITION_TRACKS_MAX);
}

static void test_the_arrows(void)
{
    ContentRows r;
    Edition e = edition("test-car,diablo,gone-car,rx7", "city", ""), out;
    content_rows_build(&r, &e, CONTENT_CARS, &mods);
    CHECK_EQ_INT(content_rows_move(&e, &out, CONTENT_CARS, &r, index_of(&r, "diablo"), -1), 1);
    CHECK_EQ_STR(list(&out, CONTENT_CARS), "diablo,test-car,gone-car,rx7");
    CHECK_EQ_INT(content_rows_move(&e, &out, CONTENT_CARS, &r, index_of(&r, "gone-car"), 1), 0);   /* missing: no arrows */
    CHECK_EQ_INT(content_rows_move(&e, &out, CONTENT_CARS, &r, index_of(&r, "rx7"), -1), 1);       /* past the missing one */
    CHECK_EQ_STR(list(&out, CONTENT_CARS), "test-car,diablo,rx7,gone-car");
    CHECK_EQ_INT(content_rows_move(&e, &out, CONTENT_CARS, &r, index_of(&r, "test-car"), -1), 0);   /* the first: no further up */
    CHECK_EQ_INT(content_rows_move(&e, &out, CONTENT_CARS, &r, index_of(&r, "rx7"), 1), 0);         /* the last on: no further down */
    CHECK_EQ_INT(content_rows_move(&e, &out, CONTENT_CARS, &r, index_of(&r, "nsx"), -1), 0);        /* off: not in the order */
    CHECK_EQ_STR(list(&out, CONTENT_CARS), "test-car,diablo,gone-car,rx7");
    /* the hidden track an edition names is not a row: the arrows step over it, the file keeps it */
    Edition t = edition("rx7", "vegas,hidden,city", "");
    content_rows_build(&r, &t, CONTENT_TRACKS, &mods);
    CHECK_EQ_INT(content_rows_move(&t, &out, CONTENT_TRACKS, &r, index_of(&r, "city"), -1), 1);
    CHECK_EQ_STR(list(&out, CONTENT_TRACKS), "city,hidden,vegas");
    CHECK_EQ_INT(content_rows_move(&t, &out, CONTENT_TRACKS, &r, index_of(&r, "city"), 1), 0);
    content_rows_build(&r, &t, CONTENT_TRACKS, &mods);
    CHECK_EQ_INT(content_rows_switch(&t, &out, CONTENT_TRACKS, &r, index_of(&r, "vegas")), 1);
    CHECK_EQ_STR(list(&out, CONTENT_TRACKS), "hidden,city");
}

static void test_the_rule(void)
{
    Edition e = edition("rx7", "city", "");
    CHECK_EQ_INT(content_rules_ok(&e, &mods), CONTENT_RULES_OK);
    e = edition("test-car", "night-ring", "");   /* installed mods count */
    CHECK_EQ_INT(content_rules_ok(&e, &mods), CONTENT_RULES_OK);
    e = edition("gone-car,old-car", "city", "");   /* missing ones do not */
    CHECK_EQ_INT(content_rules_ok(&e, &mods), CONTENT_RULES_NO_CAR);
    e = edition("rx7", "hidden,lost-track,porsche-993", "");   /* nor the hidden track, nor a car named as a track */
    CHECK_EQ_INT(content_rules_ok(&e, &mods), CONTENT_RULES_NO_TRACK);
    e = edition("", "", "grippy");
    CHECK_EQ_INT(content_rules_ok(&e, &mods), CONTENT_RULES_NO_CAR | CONTENT_RULES_NO_TRACK);
}

int main(void)
{
    setup();
    test_the_standard_edition();
    test_an_own_editions_rows();
    test_the_all_row();
    test_a_box();
    test_a_full_list();
    test_the_arrows();
    test_the_rule();
    return TEST_REPORT();
}
