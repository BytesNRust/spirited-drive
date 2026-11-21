/* Physics from (editor/car_physics_from.c): the words and stems, and, with the game's files, the physics of one car copied into a kit made from another: the performance rows
 * are the game car's, what is the kit's own (the model's size, the assist words, the PDN's body) stays, copying a car's own physics changes nothing, and a refusal leaves the kit
 * as it was. argv[1] is the game's files (original/), argv[2] the port's data/; without the Viper's CFM the copy blocks are skipped. */
#include "car_new_kit.h"
#include "car_physics_from.h"
#include "test.h"

#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char root[400];

static int rm_entry(const char *path, const struct stat *st, int flag, struct FTW *ftw)
{
    (void)st, (void)flag, (void)ftw;
    return remove(path);
}

/* the line of the tsv `rel` (under the fixture) whose second field is `name`; "" when there is none */
static const char *row(const char *rel, const char *name)
{
    static char line[8][300];
    static int at;
    char path[800], key[80];
    snprintf(path, sizeof path, "%s/%s", root, rel);
    snprintf(key, sizeof key, "\t%s\t", name);
    char *out = line[at++ % 8];
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    char got[300];
    while (f != NULL && fgets(got, sizeof got, f) != NULL)
        if (got[0] != '#' && strstr(got, key) != NULL) {
            got[strcspn(got, "\r\n")] = '\0';
            snprintf(out, sizeof line[0], "%s", got);
            break;
        }
    if (f != NULL) fclose(f);
    return out;
}

static int exists(const char *rel)
{
    char p[800];
    struct stat st;
    snprintf(p, sizeof p, "%s/%s", root, rel);
    return lstat(p, &st) == 0;
}

static void words(void)
{
    const char *w[CAR_PHYSICS_WORDS + 2];
    const int n = car_physics_words(w, CAR_PHYSICS_WORDS + 2);
    CHECK_EQ_INT(n, CAR_PHYSICS_WORDS);
    CHECK(strcmp(w[n - 1], "warrior") == 0);
    for (int i = 0; i < n; ++i) CHECK(car_physics_stem(w[i]) != NULL);
    CHECK(car_physics_stem("warrior") != NULL && strcmp(car_physics_stem("warrior"), "TRAFFC") == 0);   // the Warrior's physics are the traffic car's
    CHECK(car_physics_stem("viper") != NULL && strcmp(car_physics_stem("viper"), "DVIPER") == 0);
    CHECK(car_physics_stem("traffic") == NULL && car_physics_stem("") == NULL && car_physics_stem(NULL) == NULL);
}

static void copies(const char *cd, const char *port)
{
    char probe[900], cars[800], kit[900], why[300];
    snprintf(probe, sizeof probe, "%s/SIMDATA/CARFAMS/DVIPER.CFM", cd);
    if (access(probe, R_OK) != 0) {
        printf("skip: no DVIPER.CFM in %s\n", cd);
        return;
    }
    snprintf(cars, sizeof cars, "%s/cars", root);
    mkdir(cars, 0755);
    CHECK(car_new_kit(cd, port, cars, "viper", "", "from-viper", kit, sizeof kit, why, sizeof why));
    CHECK(car_new_kit(cd, port, cars, "supra", "", "from-supra", kit, sizeof kit, why, sizeof why));
    CHECK(strcmp(row("cars/from-viper/specs/specs.tsv", "power"), row("cars/from-supra/specs/specs.tsv", "power")) != 0);   // two cars, two powers
    char own_length[300], own_assist[300], own_body[300];
    snprintf(own_length, sizeof own_length, "%s", row("cars/from-viper/specs/specs.tsv", "half_length"));
    snprintf(own_assist, sizeof own_assist, "%s", row("cars/from-viper/specs/specs.tsv", "unknown_334"));
    snprintf(own_body, sizeof own_body, "%s", row("cars/from-viper/specs/pdn.tsv", "body_0c"));
    CHECK(own_length[0] != '\0' && own_assist[0] != '\0' && own_body[0] != '\0');

    snprintf(kit, sizeof kit, "%s/cars/from-viper", root);
    int specs_rows = -1, pdn_rows = -1;
    CHECK(car_physics_from(cd, kit, "viper", &specs_rows, &pdn_rows, why, sizeof why));   // its own physics: nothing changes
    CHECK_EQ_INT(specs_rows, 0);
    CHECK_EQ_INT(pdn_rows, 0);
    CHECK(car_physics_from(cd, kit, "supra", &specs_rows, &pdn_rows, why, sizeof why));
    if (why[0] != '\0') fprintf(stderr, "why: %s\n", why);
    CHECK(specs_rows > 10 && pdn_rows >= 1);
    CHECK(strcmp(row("cars/from-viper/specs/specs.tsv", "power"), row("cars/from-supra/specs/specs.tsv", "power")) == 0);   // the Supra's
    CHECK(strcmp(row("cars/from-viper/specs/specs.tsv", "torque_curve"), row("cars/from-supra/specs/specs.tsv", "torque_curve")) == 0);
    CHECK(strcmp(row("cars/from-viper/specs/pdn.tsv", "mass"), row("cars/from-supra/specs/pdn.tsv", "mass")) == 0);
    CHECK(strcmp(row("cars/from-viper/specs/pdn.tsv", "ai_speed"), row("cars/from-supra/specs/pdn.tsv", "ai_speed")) == 0);
    CHECK(strcmp(row("cars/from-viper/specs/specs.tsv", "half_length"), own_length) == 0);   // the model's size stays the kit's
    CHECK(strcmp(row("cars/from-viper/specs/specs.tsv", "unknown_334"), own_assist) == 0);   // so do the assist words
    CHECK(strcmp(row("cars/from-viper/specs/pdn.tsv", "body_0c"), own_body) == 0);           // and the PDN's body
    CHECK(!exists("cars/from-viper/specs/specs.tsv.new") && !exists("cars/from-viper/specs/pdn.tsv.new"));
    CHECK(car_physics_from(cd, kit, "supra", &specs_rows, &pdn_rows, why, sizeof why) && specs_rows == 0 && pdn_rows == 0);   // again: nothing left to change
    CHECK(car_physics_from(cd, kit, "warrior", &specs_rows, &pdn_rows, why, sizeof why) && specs_rows > 0);

    const char *before = row("cars/from-viper/specs/specs.tsv", "power");
    char saved[300];
    snprintf(saved, sizeof saved, "%s", before);
    CHECK(!car_physics_from(cd, kit, "wagon", &specs_rows, &pdn_rows, why, sizeof why) && strstr(why, "Physics from offers") != NULL);
    CHECK(!car_physics_from("", kit, "viper", &specs_rows, &pdn_rows, why, sizeof why) && strstr(why, "game's files") != NULL);
    CHECK(!car_physics_from("/no/such/folder", kit, "viper", &specs_rows, &pdn_rows, why, sizeof why) && strstr(why, "no such file") != NULL);
    snprintf(kit, sizeof kit, "%s/cars/no-kit", root);
    CHECK(!car_physics_from(cd, kit, "viper", &specs_rows, &pdn_rows, why, sizeof why) && strstr(why, "no such file") != NULL);
    CHECK(strcmp(row("cars/from-viper/specs/specs.tsv", "power"), saved) == 0);   // a refusal changed nothing
}

int main(int argc, char **argv)
{
    const char *cd = argc > 1 ? argv[1] : "", *port = argc > 2 ? argv[2] : "";
    snprintf(root, sizeof root, "%s/car_physics_XXXXXX", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp");
    if (mkdtemp(root) == NULL) return perror("mkdtemp"), 1;
    words();
    copies(cd, port);
    nftw(root, rm_entry, 16, FTW_DEPTH | FTW_PHYS);
    return TEST_REPORT();
}
