/* dep59 step 2: the plan of an install (formats/mod_install.h): a new mod, an update with its backup, the same version again, a folder put
 * there by hand, a reserved id; which backups go; the list kept in step. */
#include "formats/mod_install.h"
#include "test.h"

#include <string.h>

static ModCheck check_of(const char *id, const char *version)
{
    ModCheck c;
    memset(&c, 0, sizeof c);
    snprintf(c.info.id, sizeof c.info.id, "%s", id);
    snprintf(c.info.version, sizeof c.info.version, "%s", version);
    return c;
}

static ModInfo card_of(const char *id, const char *version)
{
    ModInfo i;
    memset(&i, 0, sizeof i);
    snprintf(i.id, sizeof i.id, "%s", id);
    snprintf(i.version, sizeof i.version, "%s", version);
    return i;
}

static void test_the_plans(void)
{
    ModInstallPlan p;
    const ModCheck f40 = check_of("f40", "1.2");
    mod_install_plan(&f40, NULL, 0, "20261115-101500", &p);
    CHECK_EQ_INT(p.kind, MOD_INSTALL_NEW);
    CHECK(strcmp(p.target, "mods/f40") == 0 && strcmp(p.staging, "mods/.staging/f40") == 0 && p.backup[0] == '\0');
    const ModInfo old = card_of("f40", "1.0");
    mod_install_plan(&f40, &old, 0, "20261115-101500", &p);
    CHECK_EQ_INT(p.kind, MOD_INSTALL_UPDATE);
    CHECK(strcmp(p.backup, "mods_backup/f40-1.0-20261115-101500") == 0);
    CHECK(strcmp(p.old_version, "1.0") == 0);
    const ModInfo same = card_of("f40", "1.2");
    mod_install_plan(&f40, &same, 0, "20261115-101501", &p);
    CHECK_EQ_INT(p.kind, MOD_INSTALL_SAME);
    CHECK(strcmp(p.backup, "mods_backup/f40-1.2-20261115-101501") == 0);   /* again, with its backup */
    const ModInfo odd = card_of("f40", "1.0 beta/2-rc");
    mod_install_plan(&f40, &odd, 0, "20261115-101500", &p);   /* the version as a folder's name: one level, no '-' in it */
    CHECK(strcmp(p.backup, "mods_backup/f40-1.0_beta_2_rc-20261115-101500") == 0);
    CHECK(strcmp(p.old_version, "1.0 beta/2-rc") == 0);
    mod_install_plan(&f40, NULL, 1, "20261115-101500", &p);   /* a folder put there by hand */
    CHECK_EQ_INT(p.kind, MOD_INSTALL_REFUSED);
    CHECK(strstr(p.why, "mods/f40") != NULL && strstr(p.why, "not installed by the game") != NULL);
    const ModCheck legacy = check_of("legacy", "1");
    mod_install_plan(&legacy, NULL, 0, "20261115-101500", &p);
    CHECK_EQ_INT(p.kind, MOD_INSTALL_REFUSED);
    CHECK(strstr(p.why, "legacy") != NULL);
    const ModCheck bad = check_of("../x", "1");
    mod_install_plan(&bad, NULL, 0, "20261115-101500", &p);
    CHECK_EQ_INT(p.kind, MOD_INSTALL_REFUSED);
    mod_install_plan(&f40, NULL, 0, "2026-11-15", &p);   /* a date not of the form */
    CHECK_EQ_INT(p.kind, MOD_INSTALL_REFUSED);
}

static void test_the_backups_kept(void)
{
    const char *const names[] = { "f40-1.1-20260201-000000", "f40-1.0-20260101-000000", "f40-1.3-20260401-000000",
                                  "f40-1.2-20260301-000000", "f400-1.0-20260101-000000", "f40-junk", ".DS_Store" };   /* f400's is another mod's */
    int drop[8];
    CHECK_EQ_INT(mod_backups_to_drop("f40", names, 7, 3, drop, 8), 1);
    CHECK_EQ_INT(drop[0], 1);   /* the 1st of January's; f400's is another mod's, and the junk no backup */
    CHECK_EQ_INT(mod_backups_to_drop("f40", names, 7, 1, drop, 8), 3);
    CHECK(drop[0] == 1 && drop[1] == 0 && drop[2] == 3);   /* the oldest first */
    CHECK_EQ_INT(mod_backups_to_drop("f40", names, 7, 1, drop, 2), 2);   /* no more than asked for */
    CHECK_EQ_INT(mod_backups_to_drop("f40", names, 7, 9, drop, 8), 0);
    CHECK_EQ_INT(mod_backups_to_drop("f400", names, 7, 0, drop, 8), 1);
    CHECK_EQ_INT(drop[0], 4);
    const char *const dashed[] = { "f40-x-1.0-20260101-000000", "f40-x-1.1-20260201-000000" };   /* the mod f40-x's, not f40's */
    CHECK_EQ_INT(mod_backups_to_drop("f40", dashed, 2, 0, drop, 8), 0);
    CHECK_EQ_INT(mod_backups_to_drop("f40-x", dashed, 2, 1, drop, 8), 1);
    CHECK_EQ_INT(drop[0], 0);
}

int main(void)
{
    test_the_plans();
    test_the_backups_kept();
    return TEST_REPORT();
}
