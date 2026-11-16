/* dep59 step 4 (Task 9): Game Mode's selector choices (game/frontend/mode_choices.h): the three modes, then the editions in editions/ by
 * id; an edition that cannot be used shown with its reason and never applied; applying one selects its mode and sets Restomod's
 * edition=; a name too wide or too long for the selector refused. The player folder is a scratch one (TNFS_PREF_DIR). */
#include "game/frontend/mode_choices.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char base[256], pref[300];

static void make_dirs(const char *path)
{
    char buf[512];
    snprintf(buf, sizeof buf, "%s", path);
    for (char *p = buf + 1; *p; p++)
        if (*p == '/') { *p = '\0'; mkdir(buf, 0777); *p = '/'; }
    mkdir(buf, 0777);
}

static void write_file(const char *rel, const char *text)
{
    char path[600], dir[600];
    snprintf(path, sizeof path, "%s/%s", pref, rel);
    snprintf(dir, sizeof dir, "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash != NULL) *slash = '\0';
    make_dirs(dir);
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f != NULL) fputs(text, f), fclose(f);
}

static ModeChoice get(int i)
{
    ModeChoice c;
    memset(&c, 0, sizeof c);
    CHECK(mode_choices_get(i, &c));
    return c;
}

/* Restomod's edition= copied out: the settings are a temporary, so a pointer into them would dangle */
static const char *restomod_edition(void)
{
    static char edition[sizeof port_settings_of_profile(PROFILE_RESTOMOD).edition];
    const PortSettings s = port_settings_of_profile(PROFILE_RESTOMOD);
    snprintf(edition, sizeof edition, "%s", s.edition);
    return edition;
}

static void test_the_modes_alone(void)
{
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    CHECK_EQ_INT(mode_choices_count(), 3);
    for (int p = 0; p < 3; p++) {
        const ModeChoice c = get(p);
        CHECK(strcmp(c.title, port_profile_title((Profile)p)) == 0);
        CHECK(strcmp(c.line1, port_profile_line1((Profile)p)) == 0);
        CHECK(strcmp(c.line2, port_profile_line2((Profile)p)) == 0);
        CHECK_EQ_INT(c.profile, p);
        CHECK(c.usable && c.id[0] == 0);
    }
    ModeChoice c;
    CHECK(!mode_choices_get(3, &c));
    CHECK(!mode_choices_get(-1, &c));
    CHECK_EQ_INT(mode_choices_in_use(), 2);
    CHECK(port_profile_select(PROFILE_STOCK));
    CHECK_EQ_INT(mode_choices_in_use(), 0);
}

static void test_the_editions(void)
{
    write_file("editions/europa.ini", "name=Europa 80\ncars=supra,rx7,gone-car\ntracks=city\n");
    write_file("editions/moon.ini", "name=Moon\ncars=rx7\ntracks=hidden\n");
    write_file("editions/zed.ini", "name=Thirteen chrs\n");
    write_file("editions/Not An Id.ini", "name=Skipped\ncars=rx7\ntracks=city\n");
    CHECK_EQ_INT(mode_choices_count(), 6);   /* the three, then europa, moon, zed by id */
    ModeChoice c = get(3);
    CHECK(strcmp(c.id, "europa") == 0 && strcmp(c.title, "Europa 80") == 0);
    CHECK(strcmp(c.line1, "A Restomod '26 edition - 2 cars - 1 track") == 0);   /* the car that is not there not counted */
    CHECK(c.line2[0] == 0 && c.usable && c.profile == PROFILE_RESTOMOD);
    c = get(4);   /* no track that can be raced: its reason, not usable */
    CHECK(strcmp(c.title, "Moon") == 0 && !c.usable && c.line1[0] != 0);
    c = get(5);   /* damaged: its id for a title, the parse's reason */
    CHECK(strcmp(c.title, "zed") == 0 && !c.usable && strstr(c.line1, "12") != NULL);
}

static void test_applying(void)
{
    CHECK(port_profile_select(PROFILE_STOCK));
    CHECK(mode_choices_apply(3));   /* an edition: Restomod, edition=europa */
    CHECK_EQ_INT(port_settings_load().profile, PROFILE_RESTOMOD);
    CHECK(strcmp(restomod_edition(), "europa") == 0);
    CHECK_EQ_INT(mode_choices_in_use(), 3);
    CHECK(!mode_choices_apply(4));   /* not usable: nothing changes */
    CHECK(!mode_choices_apply(5));
    CHECK(!mode_choices_apply(6));
    CHECK(strcmp(restomod_edition(), "europa") == 0);
    CHECK_EQ_INT(mode_choices_in_use(), 3);
    CHECK(mode_choices_apply(1));   /* Restored: Restomod's edition= left as it is */
    CHECK_EQ_INT(port_settings_load().profile, PROFILE_RESTORED);
    CHECK_EQ_INT(mode_choices_in_use(), 1);
    CHECK(strcmp(restomod_edition(), "europa") == 0);
    CHECK(mode_choices_apply(2));   /* Restomod '26: the standard edition, no edition= */
    CHECK_EQ_INT(port_settings_load().profile, PROFILE_RESTOMOD);
    CHECK(restomod_edition()[0] == 0);
    CHECK_EQ_INT(mode_choices_in_use(), 2);
    CHECK(mode_choices_apply(3));
    char gone[700];
    snprintf(gone, sizeof gone, "%s/editions/europa.ini", pref);
    unlink(gone);   /* its file gone: the standard edition is in use */
    CHECK_EQ_INT(mode_choices_in_use(), 2);
}

/* a mode that cannot be saved: the edition stays as it was and no mark:edition= is said (a walk WAITing on it would go on with the old
 * mode). port.cfg made a folder, the mode's save cannot replace it; the mode read before is kept */
static void test_a_mode_not_saved_changes_no_edition(void)
{
    CHECK(mode_choices_apply(2));   /* Restomod '26, the standard edition */
    CHECK(port_profile_select(PROFILE_STOCK));
    char cfg[400];
    snprintf(cfg, sizeof cfg, "%s/port.cfg", pref);
    CHECK_EQ_INT(unlink(cfg), 0);
    CHECK_EQ_INT(mkdir(cfg, 0777), 0);
    write_file("editions/europa.ini", "name=Europa 80\ncars=supra,rx7,gone-car\ntracks=city\n");   /* test_applying removed it */
    port_log_watch("mark:edition=");
    CHECK(!mode_choices_apply(3));
    CHECK(!port_log_watch_hit());
    CHECK(restomod_edition()[0] == 0);
    CHECK_EQ_INT(rmdir(cfg), 0);
}

static int wide(const char *text, int num, int den) { (void)num, (void)den; return (int)strlen(text) * 40; }

static void test_the_name_fits(void)
{
    CHECK(mode_choices_name_fits_width("Europa 80", 200));
    CHECK(mode_choices_name_fits_width("WWWWWWWWWWWW", 319));
    CHECK(!mode_choices_name_fits_width("WWWWWWWWWWWW", 320));   /* 320 px or more */
    CHECK(!mode_choices_name_fits_width("Thirteen chrs", 100));   /* 13 characters */
    CHECK(!mode_choices_name_fits_width("", 0));
    CHECK(mode_choices_name_fits("Europa 80"));   /* not measured: the characters alone */
    mode_choices_measure(wide);
    CHECK(!mode_choices_name_fits("Europa 80"));   /* measured: 9 x 40 px is too wide */
    CHECK(mode_choices_name_fits("Rally"));
    write_file("editions/wide.ini", "name=Wide name\ncars=rx7\ntracks=city\n");   /* an edition whose name does not fit: its id, the reason */
    int found = 0;
    for (int i = 3; i < mode_choices_count(); i++) {
        const ModeChoice c = get(i);
        if (strcmp(c.id, "wide") != 0) continue;
        found = 1;
        CHECK(strcmp(c.title, "wide") == 0 && !c.usable && strstr(c.line1, "too wide") != NULL);
    }
    CHECK(found);
    mode_choices_measure(NULL);
}

int main(void)
{
    snprintf(base, sizeof base, "%s/tnfs_mode_choices_test_%d", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
    snprintf(pref, sizeof pref, "%s/pref", base);
    make_dirs(pref);
    setenv("TNFS_PREF_DIR", pref, 1);
    test_the_modes_alone();
    test_the_editions();
    test_applying();
    test_a_mode_not_saved_changes_no_edition();
    test_the_name_fits();
    char cmd[400];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", base);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", base);
    return TEST_REPORT();
}
