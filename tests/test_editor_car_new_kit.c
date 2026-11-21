/* New car (editor/car_new_kit.c): the stem of a new car, the refusals (each leaves nothing behind), and, with the game's files, a kit made from a template.
 * argv[1] is the game's files (original/), argv[2] the port's data/ (car_masks/, dash_masks/); without the CFM of the Viper the made-kit block is skipped. */
#include "car_new_kit.h"
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

static void path_in(char *out, size_t cap, const char *rel)
{
    snprintf(out, cap, "%s/%s", root, rel);
}

static int exists(const char *rel)
{
    char p[800];
    struct stat st;
    path_in(p, sizeof p, rel);
    return lstat(p, &st) == 0;
}

static int file_has_line(const char *rel, const char *line)
{
    char p[800], got[200];
    path_in(p, sizeof p, rel);
    FILE *f = fopen(p, "r");
    int found = 0;
    while (f != NULL && !found && fgets(got, sizeof got, f) != NULL) {
        got[strcspn(got, "\r\n")] = '\0';
        found = strcmp(got, line) == 0;
    }
    if (f != NULL) fclose(f);
    return found;
}

static void put_card(const char *folder, const char *stem)
{
    char dir[800], file[900];
    path_in(dir, sizeof dir, folder);
    mkdir(dir, 0755);
    snprintf(file, sizeof file, "%s/mod.ini", dir);
    FILE *f = fopen(file, "w");
    if (f == NULL) return;
    fprintf(f, "id=%s\nname=x\nversion=1\ntype=car\nstem=%s\n", folder, stem);
    fclose(f);
}

static void stems(void)
{
    char cars[800], stem[16];
    path_in(cars, sizeof cars, "cars");
    mkdir(cars, 0755);
    CHECK(car_new_stem(cars, "my-car", stem, sizeof stem) && strcmp(stem, "MYCAR") == 0);
    CHECK(car_new_stem(cars, "a-very-long-car-name", stem, sizeof stem) && strcmp(stem, "AVERYLON") == 0);
    CHECK(car_new_stem(cars, "dviper", stem, sizeof stem) && strcmp(stem, "XDVIPER") == 0);   // DVIPER is the game's: an X in front
    put_card("cars/mycar", "MYCAR");
    CHECK(car_new_stem(cars, "my-car", stem, sizeof stem) && strcmp(stem, "MYCAR01") == 0);   // taken by another kit: two digits
    put_card("cars/mycar2", "MYCAR01");
    CHECK(car_new_stem(cars, "my-car", stem, sizeof stem) && strcmp(stem, "MYCAR02") == 0);
    CHECK(car_new_stem(cars, "---", stem, sizeof stem) && strcmp(stem, "X") == 0);
}

static void refusals(const char *cd, const char *port)
{
    char cars[800], kit[900], why[300];
    path_in(cars, sizeof cars, "cars");
    CHECK(!car_new_kit(cd, port, cars, "wagon", "", "new-one", kit, sizeof kit, why, sizeof why) && strstr(why, "eight cars") != NULL);
    CHECK(!car_new_kit(cd, port, cars, "viper", "", "Bad_Id", kit, sizeof kit, why, sizeof why) && strstr(why, "a-z") != NULL);
    CHECK(!car_new_kit("", port, cars, "viper", "", "new-one", kit, sizeof kit, why, sizeof why) && strstr(why, "game's files") != NULL);
    CHECK(!car_new_kit("/no/such/folder", port, cars, "viper", "", "new-one", kit, sizeof kit, why, sizeof why) && strstr(why, "game's files") != NULL);
    CHECK(!car_new_kit(cd, port, cars, "viper", "", "mycar", kit, sizeof kit, why, sizeof why) && strstr(why, "already") != NULL);   // cars/mycar is there
    CHECK(!exists("cars/new-one") && !exists("cars/new-one.piece") && !exists("cars/new-one.making"));
}

static void made(const char *cd, const char *port)
{
    char dir[800], cars[800], kit[900], why[300], probe[900];
    snprintf(probe, sizeof probe, "%s/SIMDATA/CARFAMS/DVIPER.CFM", cd);
    if (access(probe, R_OK) != 0) {
        printf("skip: no DVIPER.CFM in %s\n", cd);
        return;
    }
    path_in(cars, sizeof cars, "cars");
    path_in(dir, sizeof dir, "cars/test-car");
    CHECK(car_new_kit(cd, port, cars, "viper", "Test car", "test-car", kit, sizeof kit, why, sizeof why));
    if (why[0] != '\0') fprintf(stderr, "why: %s\n", why);
    CHECK(strcmp(kit, dir) == 0);
    CHECK(file_has_line("cars/test-car/mod.ini", "id=test-car"));
    CHECK(file_has_line("cars/test-car/mod.ini", "name=Test car"));
    CHECK(file_has_line("cars/test-car/mod.ini", "stem=TESTCAR"));
    CHECK(file_has_line("cars/test-car/mod.ini", "sound_from=viper"));
    CHECK(exists("cars/test-car/specs/specs.tsv") && exists("cars/test-car/specs/pdn.tsv"));   // the Viper's PBS and PDN as the kit lays them out
    CHECK(exists("cars/test-car/model/near/bank"));
    CHECK(!exists("cars/test-car.piece") && !exists("cars/test-car.making"));
    CHECK(!car_new_kit(cd, port, cars, "viper", "", "test-car", kit, sizeof kit, why, sizeof why) && strstr(why, "already") != NULL);   // the same id twice
}

int main(int argc, char **argv)
{
    const char *cd = argc > 1 ? argv[1] : "", *port = argc > 2 ? argv[2] : "";
    snprintf(root, sizeof root, "%s/car_new_kit_XXXXXX", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp");
    if (mkdtemp(root) == NULL) return perror("mkdtemp"), 1;
    stems();
    refusals(cd, port);
    made(cd, port);
    nftw(root, rm_entry, 16, FTW_DEPTH | FTW_PHYS);
    return TEST_REPORT();
}
