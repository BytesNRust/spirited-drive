/* The edits of a car's card (editor/car_edit.c): the model of the fields and colors, undo and redo, the problems by the game's rules, and Save, which writes only
 * the lines that changed. The fixture is the owner's 911 kit card. */
#include "car_edit.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char KIT[] =
    "id=carrera-84\nname=911 Carrera 3.2\nversion=0.2\ntype=car\nstem=C84\nshort_name=911 3.2\nyear=1984\npower=231 hp\nweight=1160 kg\n"
    "paints=Guards Red:c8102e,Grand Prix White:eeece2,Black:141414,Zermatt Silver:b8bcbe,Baltic Blue:1c3f7a\n"
    "sound_from=911\ndash_from=911\nauthor=Edgar\n"
    "description=The 1984 911 Carrera 3.2 coupe: air-cooled flat six and five gears\n";

static char dir[512], path[600];

static void write_card(const char *text)
{
    FILE *f = fopen(path, "wb");
    if (f != NULL) fwrite(text, 1, strlen(text), f), fclose(f);
}

static int file_is(const char *expected)
{
    static char got[MOD_INI_MAX_SIZE + 8];
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;
    const size_t n = fread(got, 1, sizeof got - 1, f);
    fclose(f);
    return n == strlen(expected) && memcmp(got, expected, n) == 0;
}

static CarEdit *open_card(const char *text)
{
    static CarEdit e;
    char why[200];
    write_card(text);
    memset(&e, 0, sizeof e);
    CHECK_EQ_INT(car_edit_open(&e, path, why, sizeof why), 1);
    return &e;
}

static int problems(const CarEdit *e, CarEditProblem *p)
{
    return car_edit_problems(e, p, CE_PROBLEMS_MAX);
}

static int has_problem(const CarEditProblem *p, int n, int field)
{
    for (int i = 0; i < n; i++)
        if (p[i].field == field) return 1;
    return 0;
}

static const CarEditProblem *problem_of(const CarEditProblem *p, int n, int field)
{
    for (int i = 0; i < n; i++)
        if (p[i].field == field) return &p[i];
    return NULL;
}

static const uint8_t RED[3] = { 200, 16, 46 }, BLUE[3] = { 0, 0, 255 };

static void check_open(void)
{
    CarEdit *e = open_card(KIT);
    CHECK(strcmp(e->now.value[CE_VERSION], "0.2") == 0 && strcmp(e->now.value[CE_YEAR], "1984") == 0);
    CHECK(strcmp(e->now.value[CE_POWER], "231 hp") == 0 && strcmp(e->now.value[CE_WEIGHT], "1160 kg") == 0);
    CHECK(strcmp(e->now.value[CE_SOUND_FROM], "911") == 0 && strcmp(e->now.value[CE_AUTHOR], "Edgar") == 0);
    CHECK(e->now.value[CE_CATEGORY][0] == '\0' && e->now.value[CE_ABS][0] == '\0' && e->now.value[CE_MAKE][0] == '\0');
    CHECK(strncmp(e->now.value[CE_DESCRIPTION], "The 1984 911", 12) == 0);
    CHECK_EQ_INT(e->now.paint_count, 5);
    CHECK(strcmp(e->now.paints[1].name, "Grand Prix White") == 0 && e->now.paints[1].rgb[0] == 0xee);
    CHECK(!car_edit_modified(e) && !car_edit_can_undo(e) && !car_edit_can_redo(e));
    CarEditProblem p[CE_PROBLEMS_MAX];
    CHECK_EQ_INT(problems(e, p), 0);
    CHECK(strcmp(car_edit_key(CE_PAINTS), "paints") == 0 && strcmp(car_edit_key(CE_ABS), "abs") == 0 && car_edit_cap(CE_YEAR) == 16 && car_edit_cap(CE_DESCRIPTION) == 400);
    car_edit_free(e);
    write_card("id=a\nname=A\nversion=1\ntype=track\nstem=TT\n");   /* not a car's card: refused */
    CarEdit bad;
    char why[200];
    memset(&bad, 0, sizeof bad);
    CHECK_EQ_INT(car_edit_open(&bad, path, why, sizeof why), 0);
    CHECK(why[0] != '\0');
    CHECK_EQ_INT(car_edit_open(&bad, "/home/p/nowhere/mod.ini", why, sizeof why), 0);
}

static void check_save_changes_only_what_changed(void)
{
    CarEdit *e = open_card(KIT);
    char why[200];
    CHECK_EQ_INT(car_edit_save(e, why, sizeof why), 1);   /* nothing changed: the file is not touched */
    CHECK(file_is(KIT));
    CHECK_EQ_INT(car_edit_set(e, CE_YEAR, "1985"), 1);
    CHECK(car_edit_modified(e));
    char out[MOD_INI_MAX_SIZE + 8];
    size_t len = 0;
    CHECK_EQ_INT(car_edit_render(e, out, sizeof out, &len, why, sizeof why), 1);
    CHECK(file_is(KIT));   /* rendering writes nothing */
    CHECK_EQ_INT(car_edit_set(e, CE_CATEGORY, "B"), 1);   /* a key the card did not give: appended */
    CHECK_EQ_INT(car_edit_set(e, CE_ABS, "yes"), 1);
    CHECK_EQ_INT(car_edit_set(e, CE_AUTHOR, ""), 1);      /* emptied: the line goes */
    CHECK_EQ_INT(car_edit_save(e, why, sizeof why), 1);
    CHECK(file_is("id=carrera-84\nname=911 Carrera 3.2\nversion=0.2\ntype=car\nstem=C84\nshort_name=911 3.2\nyear=1985\npower=231 hp\nweight=1160 kg\n"
                  "paints=Guards Red:c8102e,Grand Prix White:eeece2,Black:141414,Zermatt Silver:b8bcbe,Baltic Blue:1c3f7a\n"
                  "sound_from=911\ndash_from=911\n"
                  "description=The 1984 911 Carrera 3.2 coupe: air-cooled flat six and five gears\ncategory=B\nabs=yes\n"));
    CHECK(!car_edit_modified(e));   /* saved: it is the card now */
    CHECK(!car_edit_can_undo(e) || car_edit_modified(e) == 0);
    CarEdit again;
    memset(&again, 0, sizeof again);
    CHECK_EQ_INT(car_edit_open(&again, path, why, sizeof why), 1);
    CHECK(strcmp(again.now.value[CE_YEAR], "1985") == 0 && strcmp(again.now.value[CE_CATEGORY], "B") == 0 && again.now.value[CE_AUTHOR][0] == '\0');
    car_edit_free(&again);
    /* a second Save after more edits changes only those lines again */
    CHECK_EQ_INT(car_edit_set(e, CE_ABS, ""), 1);   /* "default": the line goes */
    CHECK_EQ_INT(car_edit_save(e, why, sizeof why), 1);
    CHECK(file_is("id=carrera-84\nname=911 Carrera 3.2\nversion=0.2\ntype=car\nstem=C84\nshort_name=911 3.2\nyear=1985\npower=231 hp\nweight=1160 kg\n"
                  "paints=Guards Red:c8102e,Grand Prix White:eeece2,Black:141414,Zermatt Silver:b8bcbe,Baltic Blue:1c3f7a\n"
                  "sound_from=911\ndash_from=911\n"
                  "description=The 1984 911 Carrera 3.2 coupe: air-cooled flat six and five gears\ncategory=B\n"));
    car_edit_free(e);
}

static void check_what_the_author_wrote_stays(void)
{
    /* comments, order, blank lines, spacing and CRLF are the author's: a change keeps them */
    const char card[] = "# my 911\r\nid=carrera-84\r\nname = 911 Carrera 3.2\r\n\r\nversion = 0.2\r\ntype=car\r\nstem=C84\r\nsound_from=911\r\nfuture_key=keep me\r\n";
    CarEdit *e = open_card(card);
    char why[200];
    CHECK_EQ_INT(car_edit_set(e, CE_VERSION, "0.3"), 1);
    CHECK_EQ_INT(car_edit_set(e, CE_YEAR, "1984"), 1);
    CHECK_EQ_INT(car_edit_save(e, why, sizeof why), 1);
    CHECK(file_is("# my 911\r\nid=carrera-84\r\nname = 911 Carrera 3.2\r\n\r\nversion = 0.3\r\ntype=car\r\nstem=C84\r\nsound_from=911\r\nfuture_key=keep me\r\nyear=1984\r\n"));
    car_edit_free(e);
    e = open_card(KIT);   /* spaces round a text are dropped by the card's reader: not an edit, not an error */
    CHECK_EQ_INT(car_edit_set(e, CE_YEAR, " 1984 "), 1);
    CHECK(!car_edit_modified(e));
    CHECK_EQ_INT(car_edit_set(e, CE_YEAR, "  1990  "), 1);
    CHECK_EQ_INT(car_edit_save(e, why, sizeof why), 1);
    CarEdit again;
    memset(&again, 0, sizeof again);
    CHECK_EQ_INT(car_edit_open(&again, path, why, sizeof why), 1);
    CHECK(strcmp(again.now.value[CE_YEAR], "1990") == 0);
    car_edit_free(&again);
    car_edit_free(e);
}

static void check_undo_and_redo(void)
{
    CarEdit *e = open_card(KIT);
    CHECK_EQ_INT(car_edit_undo(e), 0);
    CHECK_EQ_INT(car_edit_redo(e), 0);
    CHECK_EQ_INT(car_edit_set(e, CE_YEAR, "1"), 1);   /* typing in one field is one step */
    CHECK_EQ_INT(car_edit_set(e, CE_YEAR, "19"), 1);
    CHECK_EQ_INT(car_edit_set(e, CE_YEAR, "198"), 1);
    CHECK_EQ_INT(car_edit_set(e, CE_YEAR, "198"), 0);   /* the same: nothing */
    CHECK_EQ_INT(car_edit_set(e, CE_POWER, "300 hp"), 1);
    CHECK_EQ_INT(e->undo_count, 2);
    CHECK(car_edit_can_undo(e) && !car_edit_can_redo(e));
    CHECK_EQ_INT(car_edit_undo(e), 1);
    CHECK(strcmp(e->now.value[CE_POWER], "231 hp") == 0 && strcmp(e->now.value[CE_YEAR], "198") == 0);
    CHECK_EQ_INT(car_edit_undo(e), 1);
    CHECK(strcmp(e->now.value[CE_YEAR], "1984") == 0);
    CHECK(!car_edit_modified(e) && !car_edit_can_undo(e) && car_edit_can_redo(e));   /* back to the card: not modified */
    CHECK_EQ_INT(car_edit_redo(e), 1);
    CHECK(strcmp(e->now.value[CE_YEAR], "198") == 0 && car_edit_modified(e));
    CHECK_EQ_INT(car_edit_set(e, CE_WEIGHT, "1100 kg"), 1);   /* a new edit drops what could be redone */
    CHECK(!car_edit_can_redo(e));
    CHECK_EQ_INT(car_edit_undo(e), 1);
    CHECK_EQ_INT(car_edit_set(e, CE_YEAR, "5"), 1);   /* after an undo, an edit of the same field is a new step */
    CHECK_EQ_INT(car_edit_undo(e), 1);
    CHECK(strcmp(e->now.value[CE_YEAR], "198") == 0);
    for (int i = 0; i < CE_UNDO_MAX + 6; i++) {        /* the stack is bounded: the oldest falls off */
        char v[16];
        snprintf(v, sizeof v, "%d hp", i);
        car_edit_set(e, i % 2 ? CE_POWER : CE_WEIGHT, v);
        car_edit_set(e, i % 2 ? CE_WEIGHT : CE_POWER, v);
    }
    CHECK(e->undo_count <= CE_UNDO_MAX);
    int steps = 0;
    while (car_edit_undo(e)) steps++;
    CHECK(steps > 0 && steps <= CE_UNDO_MAX);
    char why[200];
    CHECK_EQ_INT(car_edit_save(e, why, sizeof why), 1);   /* an undone edit is not saved */
    car_edit_free(e);
}

static void check_colors(void)
{
    CarEdit *e = open_card(KIT);
    char why[200];
    CHECK_EQ_INT(car_edit_paint_add(e, "Signal Blue", BLUE), 1);
    CHECK_EQ_INT(e->now.paint_count, 6);
    CHECK_EQ_INT(car_edit_paint_move(e, 5, -1), 1);         /* up */
    CHECK(strcmp(e->now.paints[4].name, "Signal Blue") == 0 && strcmp(e->now.paints[5].name, "Baltic Blue") == 0);
    CHECK_EQ_INT(car_edit_paint_move(e, 0, -1), 0);         /* the first cannot go up, the last cannot go down */
    CHECK_EQ_INT(car_edit_paint_move(e, 5, 1), 0);
    CHECK_EQ_INT(car_edit_paint_move(e, 9, 1), 0);
    CHECK_EQ_INT(car_edit_paint_name(e, 0, "Guards Red 2"), 1);
    CHECK_EQ_INT(car_edit_paint_rgb(e, 0, RED), 0);          /* the same colour: nothing */
    const uint8_t PINK[3] = { 255, 100, 150 };
    CHECK_EQ_INT(car_edit_paint_rgb(e, 0, PINK), 1);
    CHECK_EQ_INT(car_edit_paint_remove(e, 2), 1);           /* Black goes */
    CHECK_EQ_INT(car_edit_paint_remove(e, 9), 0);
    CHECK_EQ_INT(e->now.paint_count, 5);
    CarEditProblem p[CE_PROBLEMS_MAX];
    CHECK_EQ_INT(problems(e, p), 0);
    CHECK_EQ_INT(car_edit_save(e, why, sizeof why), 1);
    CHECK(file_is("id=carrera-84\nname=911 Carrera 3.2\nversion=0.2\ntype=car\nstem=C84\nshort_name=911 3.2\nyear=1984\npower=231 hp\nweight=1160 kg\n"
                  "paints=Guards Red 2:ff6496,Grand Prix White:eeece2,Zermatt Silver:b8bcbe,Signal Blue:0000ff,Baltic Blue:1c3f7a\n"
                  "sound_from=911\ndash_from=911\nauthor=Edgar\n"
                  "description=The 1984 911 Carrera 3.2 coupe: air-cooled flat six and five gears\n"));
    /* undo takes the colors back one step at a time; a dragged colour is one step */
    const uint8_t A[3] = { 1, 2, 3 }, B[3] = { 4, 5, 6 };
    const int steps_before = e->undo_count;
    CHECK_EQ_INT(car_edit_paint_rgb(e, 1, A), 1);
    CHECK_EQ_INT(car_edit_paint_rgb(e, 1, B), 1);
    CHECK_EQ_INT(e->undo_count, steps_before + 1);
    CHECK_EQ_INT(car_edit_undo(e), 1);
    CHECK(e->now.paints[1].rgb[0] == 0xee);
    /* all of them gone: the line goes; sixteen is the most */
    while (e->now.paint_count > 0) car_edit_paint_remove(e, 0);
    CHECK_EQ_INT(problems(e, p), 0);
    CHECK_EQ_INT(car_edit_save(e, why, sizeof why), 1);
    CHECK(!file_is(KIT));
    CarEdit again;
    memset(&again, 0, sizeof again);
    CHECK_EQ_INT(car_edit_open(&again, path, why, sizeof why), 1);
    CHECK_EQ_INT(again.now.paint_count, 0);
    car_edit_free(&again);
    for (int i = 0; i < MOD_INI_PAINTS_MAX; i++) {
        char name[16];
        snprintf(name, sizeof name, "C%d", i);
        CHECK_EQ_INT(car_edit_paint_add(e, name, RED), 1);
    }
    CHECK_EQ_INT(car_edit_paint_add(e, "One too many", RED), 0);
    CHECK_EQ_INT(e->now.paint_count, MOD_INI_PAINTS_MAX);
    CHECK_EQ_INT(problems(e, p), 0);
    CHECK_EQ_INT(car_edit_save(e, why, sizeof why), 1);
    car_edit_free(e);
}

static void check_problems(void)
{
    CarEdit *e = open_card(KIT);
    CarEditProblem p[CE_PROBLEMS_MAX];
    char why[200];
    CHECK_EQ_INT(car_edit_set(e, CE_YEAR, "12345678901234567"), 1);   /* 17: the card keeps 16 */
    int n = problems(e, p);
    CHECK_EQ_INT(n, 1);
    const CarEditProblem *y = problem_of(p, n, CE_YEAR);
    CHECK(y != NULL && strstr(y->text, "17") != NULL && strstr(y->text, "16") != NULL && strstr(y->text, "Save") != NULL && strstr(y->text, "Year") != NULL);
    CHECK_EQ_INT(car_edit_save(e, why, sizeof why), 0);   /* refused, with the first problem; the file as it was */
    CHECK(strstr(why, "Year") != NULL && file_is(KIT));
    CHECK_EQ_INT(car_edit_set(e, CE_VERSION, ""), 1);     /* a card needs a version */
    CHECK_EQ_INT(car_edit_set(e, CE_SOUND_FROM, "ferrari"), 1);
    CHECK_EQ_INT(car_edit_set(e, CE_ABS, "maybe"), 1);
    CHECK_EQ_INT(car_edit_set(e, CE_TRACTION, "yes"), 1);
    char big[CE_VALUE_MAX];
    memset(big, 'x', 401), big[401] = '\0';
    CHECK_EQ_INT(car_edit_set(e, CE_DESCRIPTION, big), 1);
    CHECK_EQ_INT(car_edit_set(e, CE_MAKE, "Porsche\tAG"), 1);   /* a control character */
    n = problems(e, p);
    CHECK(n >= 6);
    CHECK(has_problem(p, n, CE_YEAR) && has_problem(p, n, CE_VERSION) && has_problem(p, n, CE_SOUND_FROM) && has_problem(p, n, CE_ABS));
    CHECK(has_problem(p, n, CE_DESCRIPTION) && has_problem(p, n, CE_MAKE) && !has_problem(p, n, CE_TRACTION) && !has_problem(p, n, CE_POWER));
    const CarEditProblem *d = problem_of(p, n, CE_DESCRIPTION);
    CHECK(d != NULL && strstr(d->text, "401") != NULL && strstr(d->text, "400") != NULL);
    const CarEditProblem *s = problem_of(p, n, CE_SOUND_FROM);
    CHECK(s != NULL && strstr(s->text, "ferrari") != NULL && strstr(s->text, "911") != NULL && strstr(s->text, "Save") != NULL);
    const CarEditProblem *v = problem_of(p, n, CE_VERSION);
    CHECK(v != NULL && strstr(v->text, "Save") != NULL);
    const CarEditProblem *a = problem_of(p, n, CE_ABS);
    CHECK(a != NULL && strstr(a->text, "yes") != NULL && strstr(a->text, "no") != NULL);
    for (int i = 0; i < n; i++)
        for (const char *t = p[i].text; *t != '\0'; t++) CHECK((unsigned char)*t >= 0x20);   /* a message is one line of text */
    CHECK_EQ_INT(car_edit_save(e, why, sizeof why), 0);
    CHECK(file_is(KIT));
    /* fixing them one by one clears them; undo clears them too */
    CHECK_EQ_INT(car_edit_set(e, CE_YEAR, "1984"), 1);
    CHECK_EQ_INT(car_edit_set(e, CE_VERSION, "0.3"), 1);
    CHECK_EQ_INT(car_edit_set(e, CE_SOUND_FROM, "viper"), 1);
    CHECK_EQ_INT(car_edit_set(e, CE_ABS, "no"), 1);
    CHECK_EQ_INT(car_edit_set(e, CE_DESCRIPTION, "short"), 1);
    CHECK_EQ_INT(car_edit_set(e, CE_MAKE, "Porsche"), 1);
    CHECK_EQ_INT(problems(e, p), 0);
    CHECK_EQ_INT(car_edit_save(e, why, sizeof why), 1);
    car_edit_free(e);

    /* a missing sound_from is a problem (the game refuses a car with none); the allowed list is the game's */
    e = open_card("id=a\nname=A\nversion=1\ntype=car\nstem=AB1\n");
    n = problems(e, p);
    CHECK(n == 1 && p[0].field == CE_SOUND_FROM && strstr(p[0].text, "supra") != NULL && strstr(p[0].text, "rx7") != NULL);
    CHECK_EQ_INT(car_edit_set(e, CE_SOUND_FROM, "nsx"), 1);
    CHECK_EQ_INT(problems(e, p), 0);
    CHECK_EQ_INT(car_edit_set(e, CE_SOUND_FROM, "traffic"), 1);   /* the owner added it: the traffic car's sound */
    CHECK_EQ_INT(problems(e, p), 0);
    CHECK_EQ_INT(car_edit_save(e, why, sizeof why), 1);
    {
        const char *words[16];
        const int count = car_edit_sound_words(words, 16);
        CHECK_EQ_INT(count, 9);
        CHECK(strcmp(words[0], "supra") == 0 && strcmp(words[7], "rx7") == 0 && strcmp(words[8], "traffic") == 0);
    }
    CHECK_EQ_INT(car_edit_set(e, CE_SOUND_FROM, "ferrari"), 1);
    n = problems(e, p);
    CHECK(n == 1 && strstr(p[0].text, "rx7, traffic") != NULL);
    car_edit_free(e);

    /* the colors: a name the card refuses, two alike */
    e = open_card(KIT);
    CHECK_EQ_INT(car_edit_paint_name(e, 0, ""), 1);
    n = problems(e, p);
    CHECK(n == 1 && p[0].field == CE_PAINTS && p[0].paint == 0 && strstr(p[0].text, "Save") != NULL);
    CHECK_EQ_INT(car_edit_paint_name(e, 0, "a:b"), 1);
    CHECK(problems(e, p) == 1 && p[0].paint == 0);
    CHECK_EQ_INT(car_edit_paint_name(e, 0, "Black"), 1);   /* the same as another, whatever the case */
    CHECK_EQ_INT(car_edit_paint_name(e, 2, "BLACK"), 1);
    n = problems(e, p);
    CHECK(n == 1 && p[0].field == CE_PAINTS && p[0].paint == 2 && strstr(p[0].text, "BLACK") != NULL && strstr(p[0].text, "color 1") != NULL);
    CHECK_EQ_INT(car_edit_save(e, why, sizeof why), 0);
    CHECK(file_is(KIT));
    car_edit_free(e);
}

static void check_a_card_that_is_full(void)
{
    /* a card near its cap: a longer description would take it over 4096 bytes */
    static char card[MOD_INI_MAX_SIZE + 64];
    char filler[3700];
    memset(filler, 'k', sizeof filler - 1), filler[sizeof filler - 1] = '\0';
    snprintf(card, sizeof card, "id=a\nname=A\nversion=1\ntype=car\nstem=AB1\nsound_from=911\nfuture_key=%s\ndescription=short\n", filler);
    CarEdit *e = open_card(card);
    CarEditProblem p[CE_PROBLEMS_MAX];
    char big[CE_VALUE_MAX], why[200];
    memset(big, 'd', 390), big[390] = '\0';
    CHECK_EQ_INT(car_edit_set(e, CE_DESCRIPTION, big), 1);
    const int n = problems(e, p);
    CHECK(n >= 1 && (has_problem(p, n, CE_CARD) || has_problem(p, n, CE_DESCRIPTION)));
    const CarEditProblem *c = problem_of(p, n, CE_CARD);
    CHECK(c != NULL && strstr(c->text, "4096") != NULL && strstr(c->text, "Save") != NULL);
    CHECK_EQ_INT(car_edit_save(e, why, sizeof why), 0);
    CHECK(file_is(card));
    CHECK_EQ_INT(car_edit_set(e, CE_DESCRIPTION, "short again"), 1);
    CHECK_EQ_INT(problems(e, p), 0);
    car_edit_free(e);
}

static void check_by_text(void)
{
    CarEdit *e = open_card(KIT);
    CHECK_EQ_INT(car_edit_set_by_text(e, "year=1999"), 1);
    CHECK(strcmp(e->now.value[CE_YEAR], "1999") == 0);
    CHECK_EQ_INT(car_edit_set_by_text(e, "category="), 1);
    CHECK_EQ_INT(car_edit_set_by_text(e, "abs=yes"), 1);
    CHECK_EQ_INT(car_edit_set_by_text(e, "id=other"), 0);      /* a key that is not edited here */
    CHECK_EQ_INT(car_edit_set_by_text(e, "year"), 0);
    CHECK_EQ_INT(car_edit_set_by_text(e, "=x"), 0);
    car_edit_free(e);
}

/* the version one step up, for Rehacer's "Raise version" (owner test R2): X.Y.(Z+1), missing parts as 0; nothing for a version that is not numbers */
static void check_version_next(void)
{
    static const struct { const char *from, *to; } UP[] = {
        { "0.2", "0.2.1" }, { "1", "1.0.1" }, { "0.2.1", "0.2.2" }, { "1.2.9", "1.2.10" }, { " 0.3 ", "0.3.1" }, { "01.2", "01.2.1" }, { "2.0.99", "2.0.100" },
    };
    for (size_t i = 0; i < sizeof UP / sizeof UP[0]; i++) {
        char out[MOD_INI_VERSION_MAX + 1] = "x";
        CHECK_EQ_INT(car_edit_version_next(UP[i].from, out, sizeof out), 1);
        if (strcmp(out, UP[i].to) != 0) fprintf(stderr, "version_next(%s) gave %s, expected %s\n", UP[i].from, out, UP[i].to);
        CHECK(strcmp(out, UP[i].to) == 0);
    }
    static const char *const NOT[] = { "", "  ", "v1", "1.2.3.4", "1..2", ".1", "1.", "1.2.x", "1.-2", "1 2", "12345.12345.9999", NULL };
    for (size_t i = 0; i < sizeof NOT / sizeof NOT[0]; i++) {
        char out[MOD_INI_VERSION_MAX + 1] = "x";
        CHECK_EQ_INT(car_edit_version_next(NOT[i], out, sizeof out), 0);
        CHECK(out[0] == '\0');
    }
    char small[5];
    CHECK_EQ_INT(car_edit_version_next("0.2", small, sizeof small), 0);   /* "0.2.1" does not fit in 5 bytes */
}

static void check_photos(void)
{
    CarEdit *e = open_card(KIT);
    CarEditProblem p[CE_PROBLEMS_MAX];
    CHECK(strcmp(car_edit_key(CE_PHOTOS), "photos") == 0 && car_edit_cap(CE_PHOTOS) == 200);
    CHECK(e->now.value[CE_PHOTOS][0] == '\0');   /* the kit names none */
    CHECK_EQ_INT(car_edit_set(e, CE_PHOTOS, "front.png,side.png"), 1);
    CHECK(car_edit_modified(e) && car_edit_can_undo(e) && problems(e, p) == 0);
    char out[MOD_INI_MAX_SIZE + 1], why[200];
    CHECK(car_edit_render(e, out, sizeof out, NULL, why, sizeof why) && strstr(out, "photos=front.png,side.png") != NULL);
    CHECK_EQ_INT(car_edit_save(e, why, sizeof why), 1);
    CHECK(!car_edit_modified(e));
    CHECK_EQ_INT(car_edit_set(e, CE_PHOTOS, "front.png"), 1);   /* one removed: undo brings it back */
    CHECK_EQ_INT(car_edit_undo(e), 1);
    CHECK(strcmp(e->now.value[CE_PHOTOS], "front.png,side.png") == 0 && !car_edit_modified(e));
    CHECK_EQ_INT(car_edit_set(e, CE_PHOTOS, "front.png,front.png"), 1);   /* the game's reader refuses a name twice: Save is blocked */
    CHECK(problems(e, p) == 1 && p[0].field == CE_PHOTOS);
    CHECK_EQ_INT(car_edit_set(e, CE_PHOTOS, "Front.png"), 1);
    CHECK(problems(e, p) == 1 && p[0].field == CE_PHOTOS);
    CHECK_EQ_INT(car_edit_set(e, CE_PHOTOS, "card.png"), 1);
    CHECK(problems(e, p) == 1 && p[0].field == CE_PHOTOS);
    CHECK_EQ_INT(car_edit_set(e, CE_PHOTOS, "1.png,2.png,3.png,4.png,5.png,6.png,7.png,8.png,9.png"), 1);
    CHECK(problems(e, p) >= 1 && p[0].field == CE_PHOTOS);
    car_edit_free(e);
    static CarEdit again;   /* reopened (the card as saved, not the kit): the saved line is the list */
    CHECK_EQ_INT(car_edit_open(&again, path, why, sizeof why), 1);
    e = &again;
    CHECK(strcmp(e->now.value[CE_PHOTOS], "front.png,side.png") == 0);
    car_edit_free(e);
}

int main(void)
{
    const char *tmp = getenv("TMPDIR");
    snprintf(dir, sizeof dir, "%s/tnfs-car-edit-XXXXXX", tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp");
    if (mkdtemp(dir) == NULL) {
        perror("mkdtemp");
        return 1;
    }
    snprintf(path, sizeof path, "%s/mod.ini", dir);
    check_open();
    check_save_changes_only_what_changed();
    check_what_the_author_wrote_stays();
    check_undo_and_redo();
    check_colors();
    check_problems();
    check_a_card_that_is_full();
    check_by_text();
    check_version_next();
    check_photos();
    remove(path);
    rmdir(dir);
    return TEST_REPORT();
}
