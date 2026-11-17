/* dep59 step 4, T7: an edition's rules (game/frontend/edition_rules.h). The standard edition (none) keeps today's game; an edition of
 * the player's own unlocks everything but the Warrior (only when cars= lists it) and the hidden track (only when tracks= does), has the
 * tournament only with the stock cars and tracks, never the network, and Lunar Springs only with lunar in tracks=. Its unlock level is
 * 2 and is never written; the variants and the Warrior settle to what it gives (extras_apply.h). On a scratch player folder. */
#include "game/frontend/config_dat.h"
#include "game/frontend/edition_rules.h"
#include "game/frontend/extras_apply.h"
#include "game/frontend/unlock_level.h"
#include "game/sys/edition_file.h"
#include "game/sys/port_extras.h"
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
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", path);
    for (char *p = buf + 1; *p; ++p)
        if (*p == '/') { *p = '\0'; mkdir(buf, 0777); *p = '/'; }
    mkdir(buf, 0777);
}

static void write_file(const char *rel, const char *text)
{
    char path[1100], dir[1100];
    snprintf(path, sizeof path, "%s/%s", pref, rel);
    snprintf(dir, sizeof dir, "%s", path);
    *strrchr(dir, '/') = '\0';
    make_dirs(dir);
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f != NULL) fputs(text, f), fclose(f);
}

static Edition edition(const char *text)
{
    Edition e;
    char why[160];
    CHECK(edition_parse((const uint8_t *)text, strlen(text), &e, why, sizeof why));
    return e;
}

static const char STOCK_CARS[] = "cars=diablo,rx7,nsx,supra,911,zr1,viper,512tr\n";
static const char STOCK_TRACKS[] = "tracks=transtropolis,city,coastal,alpine,rusty,autumn,burnt,vertigo\n";

/* the standard edition: all open; one of the player's own: the network never, the tournament with the stock sets only */
static void test_the_gates(void)
{
    const EditionGate none = edition_gate(NULL);
    CHECK(none.tournament && none.network && none.tournament_why == NULL && none.network_why == NULL);
    const Edition two = edition("name=Two\ncars=rx7\ntracks=city,coastal\n");
    const EditionGate g = edition_gate(&two);
    CHECK(!g.tournament && !g.network);
    CHECK(g.tournament_why != NULL && strcmp(g.tournament_why, "Tournament needs the stock cars and tracks") == 0);
    CHECK(g.network_why != NULL && strcmp(g.network_why, "Multiplayer needs the standard edition") == 0);
    char text[512];
    snprintf(text, sizeof text, "name=Stock\n%s%s", STOCK_CARS, STOCK_TRACKS);
    const Edition stock = edition(text);
    const EditionGate s = edition_gate(&stock);
    CHECK(s.tournament && s.tournament_why == NULL);
    CHECK(!s.network && s.network_why != NULL);
}

/* the stock sets in any order; the Warrior, Lost Vegas and the hidden track may be there too; a car or track missing, or one added, is not */
static void test_what_is_stock(void)
{
    char text[512];
    snprintf(text, sizeof text, "name=A\n%s%s", STOCK_CARS, STOCK_TRACKS);
    CHECK(edition_is_stock(&(Edition){ 0 }) == 0);
    Edition e = edition(text);
    CHECK(edition_is_stock(&e));
    snprintf(text, sizeof text, "name=A\ncars=warrior,diablo,rx7,nsx,supra,911,zr1,viper,512tr\ntracks=vegas,hidden,%s", STOCK_TRACKS + 7);
    e = edition(text);
    CHECK(edition_is_stock(&e));
    snprintf(text, sizeof text, "name=A\ncars=rx7,nsx,supra,911,zr1,viper,512tr\n%s", STOCK_TRACKS);   /* no Diablo */
    e = edition(text);
    CHECK(!edition_is_stock(&e));
    snprintf(text, sizeof text, "name=A\ncars=test-car,diablo,rx7,nsx,supra,911,zr1,viper,512tr\n%s", STOCK_TRACKS);   /* an added car */
    e = edition(text);
    CHECK(!edition_is_stock(&e));
    snprintf(text, sizeof text, "name=A\n%stracks=city,coastal,alpine,rusty,autumn,burnt,vertigo\n", STOCK_CARS);   /* no Transtropolis */
    e = edition(text);
    CHECK(!edition_is_stock(&e));
    CHECK(!edition_is_stock(NULL));
}

/* an edition unlocks everything; the Warrior by cars=, the hidden track by tracks= */
static void test_the_unlocks(void)
{
    Edition e = edition("name=A\ncars=rx7\ntracks=city\n");
    Unlocks u = edition_unlocks(&e);
    CHECK(u.variant_m && u.variant_r && u.lost_vegas && !u.warrior && !u.hidden);
    e = edition("name=A\ncars=rx7,warrior\ntracks=city,hidden\n");
    u = edition_unlocks(&e);
    CHECK(u.variant_m && u.variant_r && u.lost_vegas && u.warrior && u.hidden);
}

/* dep59 Lunar: Lunar Springs (its entry, and Lunar Gravity's "Lunar Springs") only with lunar listed; rusty alone is the desert */
static void test_lunar_springs_needs_lunar(void)
{
    Edition e = edition("name=A\ncars=rx7\ntracks=city,rusty\n");
    CHECK(!edition_lunar_springs(&e));
    PortExtras x = port_extras_defaults();
    x.lunar_springs = 1, x.warrior = 1;
    PortExtras g = edition_extras(&e, x);
    CHECK_EQ_INT(g.lunar_springs, 0);
    CHECK_EQ_INT(g.warrior, 0);   /* the Warrior switch gives it only when listed */
    x.warrior_rival = 1;
    CHECK_EQ_INT(edition_extras(&e, x).warrior_rival, 0);   /* and the rival's */
    x.warrior_rival = 0;
    e = edition("name=A\ncars=warrior\ntracks=lunar\n");
    CHECK(edition_lunar_springs(&e));
    g = edition_extras(&e, x);
    CHECK_EQ_INT(g.lunar_springs, 1);
    CHECK_EQ_INT(g.warrior, 1);
    x.lunar_springs = 0, x.warrior = 0;
    g = edition_extras(&e, x);
    CHECK_EQ_INT(g.lunar_springs, 1);   /* listed: always there, like the Warrior */
    CHECK_EQ_INT(g.warrior, 1);   /* listed: always there */
    const ExtrasExpect x1 = extras_expect_level(1), x2 = edition_expect(&e);
    CHECK(x1.m && !x1.r && x1.warrior);
    CHECK(x2.m && x2.r && x2.warrior);
}

/* the variants and the Warrior settle to what an edition gives: the level's Warrior goes when the edition does not list it, Lost Vegas
 * stays, and a listed Warrior comes back */
static void test_settle_to_the_edition(void)
{
    int32_t m = 0, r = 0, vm = 0, vr = 0, explode = 0, nitro = 0, vegas = 0, lost = 1;
    int16_t warrior = 0;
    uint32_t track_flags = 0;
    const ExtrasFlags f = { &m, &r, &vm, &vr, &explode, &nitro, &vegas, &lost, &warrior, &track_flags };
    ExtrasSession s;
    memset(&s, 0, sizeof s);
    const PortExtras x = port_extras_defaults();
    extras_settle_to(&x, extras_expect_level(1), &f, &s);
    CHECK(m == 1 && r == 0 && warrior == 1);
    const Edition e = edition("name=A\ncars=rx7\ntracks=city\n");
    extras_settle_to(&x, edition_expect(&e), &f, &s);
    CHECK(m == 1 && r == 1 && lost == 1);
    CHECK_EQ_INT(warrior, 0);
    const Edition w = edition("name=A\ncars=rx7,warrior\ntracks=city\n");
    extras_settle_to(&x, edition_expect(&w), &f, &s);
    CHECK_EQ_INT(warrior, 1);
}

/* a change of edition starts a new session (extras_session_restart): CONCEPTCAR typed in the standard edition does not carry into an edition
 * that does not list the Warrior, nor the edition's Warrior into the standard edition at level 0 */
static void test_a_new_edition_is_a_new_session(void)
{
    int32_t m = 0, r = 0, vm = 0, vr = 0, explode = 0, nitro = 0, vegas = 0, lost = 0;
    int16_t warrior = 0;
    uint32_t track_flags = 0;
    const ExtrasFlags f = { &m, &r, &vm, &vr, &explode, &nitro, &vegas, &lost, &warrior, &track_flags };
    ExtrasSession s;
    memset(&s, 0, sizeof s);
    const PortExtras x = port_extras_defaults();
    extras_settle(&x, 0, &f, &s);
    warrior = 1;   /* CONCEPTCAR */
    extras_settle(&x, 0, &f, &s);
    CHECK_EQ_INT(warrior, 1);   /* typed: kept for the session */
    const Edition e = edition("name=A\ncars=rx7\ntracks=city\n");
    extras_session_restart(&f, &s);
    extras_settle_to(&x, edition_expect(&e), &f, &s);
    CHECK_EQ_INT(warrior, 0);
    const Edition w = edition("name=A\ncars=rx7,warrior\ntracks=city\n");
    extras_session_restart(&f, &s);
    extras_settle_to(&x, edition_expect(&w), &f, &s);
    CHECK_EQ_INT(warrior, 1);
    CHECK(m == 1 && r == 1);
    extras_session_restart(&f, &s);
    extras_settle(&x, 0, &f, &s);   /* back in the standard edition at level 0 */
    CHECK(warrior == 0 && m == 0 && r == 0);
}

static void use(const char *id)
{
    PortSettings s = port_settings_of_profile(PROFILE_RESTOMOD);
    const PortExtras e = port_extras_of_profile(PROFILE_RESTOMOD);
    snprintf(s.edition, sizeof s.edition, "%s", id);
    CHECK(port_mode_save(&s, &e));
}

/* in an edition the level is 2, a put writes nothing, and Lunar Springs follows lunar in tracks=, not the level */
static void test_the_level_in_an_edition(void)
{
    static ConfigDat c;
    memset(&c, 0, sizeof c);
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    unlock_level_put(&c, 0);   /* the standard edition's own level: 0 */
    CHECK_EQ_INT(unlock_level_get(&c), 0);
    PortExtras x = port_extras_load();
    x.lunar_springs = 1;
    CHECK(port_extras_save(&x));
    write_file("editions/own.ini", "name=Own\ncars=rx7\ntracks=city,coastal\n");
    write_file("editions/moon.ini", "name=Moon\ncars=rx7\ntracks=rusty,lunar\n");
    use("own");
    CHECK_EQ_INT(unlock_level_get(&c), 2);
    unlock_level_put(&c, 1);   /* ROCKYROAD or a tournament won: nothing written */
    CHECK_EQ_INT(unlock_level_get(&c), 2);
    CHECK_EQ_INT(port_extras_load().unlock_level, 0);
    CHECK_EQ_INT(config_dat_unlock_level(&c), 0);
    CHECK_EQ_INT(unlock_level_extras(&c).lunar_springs, 0);   /* no lunar */
    use("moon");
    CHECK_EQ_INT(unlock_level_extras(&c).lunar_springs, 1);   /* lunar listed: level 2's */
    use("");
    CHECK_EQ_INT(unlock_level_get(&c), 0);   /* the standard edition: its own level as it was */
    CHECK_EQ_INT(unlock_level_extras(&c).lunar_springs, 0);
}

int main(void)
{
    snprintf(base, sizeof base, "%s/tnfs_edition_rules_test_%d", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
    snprintf(pref, sizeof pref, "%s/pref", base);
    make_dirs(pref);
    setenv("TNFS_PREF_DIR", pref, 1);
    test_the_gates();
    test_what_is_stock();
    test_the_unlocks();
    test_lunar_springs_needs_lunar();
    test_settle_to_the_edition();
    test_a_new_edition_is_a_new_session();
    test_the_level_in_an_edition();
    char cmd[400];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", base);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", base);
    return TEST_REPORT();
}
