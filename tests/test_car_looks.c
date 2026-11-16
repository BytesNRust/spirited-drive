/* dep59 step 3: the added car picked and its look, by id in restomod.cfg (game/sys/car_looks.h): a round trip of car=, opponent= and
 * look.<id>=; an original takes its line out; an id not loaded gives CAR_LOOKS_NOT_LOADED and its lines stay; Stock and Restored read and write nothing;
 * a save of the mode's settings keeps the lines (port_mode_write). */
#include "game/sys/car_looks.h"
#include "game/race/car_registry.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include "test.h"

#include "game/platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the player's folder in memory: port.cfg (the mode) and the mode files */
enum { FILES = 4, SIZE = 4096 };
static struct { char name[32]; char text[SIZE]; size_t size; int present; } files[FILES];

static int slot(const char *name, int make)
{
    for (int i = 0; i < FILES; i++)
        if (files[i].present && strcmp(files[i].name, name) == 0) return i;
    if (!make) return -1;
    for (int i = 0; i < FILES; i++)
        if (!files[i].present) return snprintf(files[i].name, sizeof files[i].name, "%s", name), i;
    return -1;
}

uint8_t *platform_read_user_file(const char *name, size_t *size)
{
    const int i = slot(name, 0);
    if (i < 0) return NULL;
    uint8_t *copy = malloc(files[i].size + 1);
    memcpy(copy, files[i].text, files[i].size);
    *size = files[i].size;
    return copy;
}

bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    const int i = slot(name, 1);
    if (i < 0 || size >= SIZE) return false;
    memcpy(files[i].text, data, size);
    files[i].text[size] = '\0';
    files[i].size = size, files[i].present = 1;
    return true;
}

void platform_free_file(uint8_t *data) { free(data); }

static void fresh(const char *mode)
{
    memset(files, 0, sizeof files);
    char line[64];
    snprintf(line, sizeof line, "profile=%s\n", mode);
    platform_write_user_file("port.cfg", (const uint8_t *)line, strlen(line));
    port_settings_forget();
}

static const char *file(const char *name)
{
    const int i = slot(name, 0);
    return i < 0 ? NULL : files[i].text;
}

static void add(const char *id, const char *stem)
{
    CarPiece p;
    memset(&p, 0, sizeof p);
    snprintf(p.id, sizeof p.id, "%s", id);
    snprintf(p.stem, sizeof p.stem, "%s", stem);
    p.dash_model = -1;
    CHECK(car_registry_add(&p, NULL, 0) >= CAR_ADDED_FIRST);
}

static void test_cars(void)
{
    fresh("restomod");
    CHECK_EQ_INT(car_looks_saved_car(0), -1);   /* no line */
    CHECK(car_looks_save_car(0, 16));
    CHECK(car_looks_save_car(1, 17));
    CHECK(strstr(file("restomod.cfg"), "car=test-car\n") != NULL);
    CHECK(strstr(file("restomod.cfg"), "opponent=other\n") != NULL);
    CHECK_EQ_INT(car_looks_saved_car(0), 16);
    CHECK_EQ_INT(car_looks_saved_car(1), 17);
    CHECK(car_looks_save_car(0, 17));   /* replaced, never twice */
    CHECK_EQ_INT(car_looks_saved_car(0), 17);
    CHECK(strstr(file("restomod.cfg"), "test-car") == NULL);
    CHECK(car_looks_save_car(0, 3));    /* an original: config.dat keeps it */
    CHECK_EQ_INT(car_looks_saved_car(0), -1);
    CHECK(strstr(file("restomod.cfg"), "\ncar=") == NULL && strncmp(file("restomod.cfg"), "car=", 4) != 0);
    CHECK_EQ_INT(car_looks_saved_car(1), 17);
    /* Review Focus 2: an id that is not loaded is told apart from no line, and its lines stay */
    CHECK(car_looks_save_car(0, 16));
    CHECK(car_registry_remove("test-car"));
    CHECK_EQ_INT(car_looks_saved_car(0), CAR_LOOKS_NOT_LOADED);
    CHECK(strstr(file("restomod.cfg"), "car=test-car\n") != NULL);
    add("test-car", "TESTCAR");
}

static void test_looks(void)
{
    fresh("restomod");
    CarLook l = car_looks_get("test-car");
    CHECK(l.skin[0] == '\0' && l.paint[0] == '\0');
    CHECK(car_looks_set("test-car", &(CarLook){ "race", "Giallo" }));
    CHECK(strstr(file("restomod.cfg"), "look.test-car=race,Giallo\n") != NULL);
    l = car_looks_get("test-car");
    CHECK(strcmp(l.skin, "race") == 0 && strcmp(l.paint, "Giallo") == 0);
    CHECK(car_looks_set("test-car", &(CarLook){ "", "Rosso Corsa" }));
    CHECK(strstr(file("restomod.cfg"), "look.test-car=-,Rosso Corsa\n") != NULL);
    l = car_looks_get("test-car");
    CHECK(l.skin[0] == '\0' && strcmp(l.paint, "Rosso Corsa") == 0);
    CHECK(car_looks_set("other", &(CarLook){ "stripes", "" }));
    l = car_looks_get("other");
    CHECK(strcmp(l.skin, "stripes") == 0 && l.paint[0] == '\0');
    CHECK(car_looks_set("test-car", &(CarLook){ "", "" }));   /* as it comes: no line */
    CHECK(strstr(file("restomod.cfg"), "look.test-car") == NULL);
    CHECK(strstr(file("restomod.cfg"), "look.other=stripes,-\n") != NULL);
    CHECK(!car_looks_set("../x", &(CarLook){ "race", "" }));        /* a bad id */
    CHECK(!car_looks_set("test-car", &(CarLook){ "Race", "" }));    /* a bad skin name */
    CHECK(!car_looks_set("test-car", &(CarLook){ "race", "a,b" })); /* a paint name the card refuses */
    CHECK(!car_looks_set("test-car", &(CarLook){ "race", "a\nb" }));
    platform_write_user_file("restomod.cfg", (const uint8_t *)"look.test-car=RACE,Giallo\n", 26);   /* a hand-made bad line: as it comes */
    l = car_looks_get("test-car");
    CHECK(l.skin[0] == '\0' && strcmp(l.paint, "Giallo") == 0);
}

/* a save of the mode's settings (any of its writers goes through port_mode_write) keeps the lines */
static void test_a_settings_save_keeps_them(void)
{
    fresh("restomod");
    CHECK(car_looks_save_car(0, 16));
    CHECK(car_looks_set("test-car", &(CarLook){ "race", "Giallo" }));
    PortExtras e = port_extras_load();
    e.nitro = 1;
    CHECK(port_extras_save(&e));
    CHECK(strstr(file("restomod.cfg"), "nitro=1\n") != NULL);
    CHECK(strstr(file("restomod.cfg"), "look.test-car=race,Giallo\n") != NULL);
    CHECK(strstr(file("restomod.cfg"), "car=test-car\n") != NULL);
    CHECK_EQ_INT(car_looks_saved_car(0), 16);
    CHECK(car_looks_save_car(0, 2));   /* and these writes keep the settings */
    CHECK(strstr(file("restomod.cfg"), "nitro=1\n") != NULL);
}

static void test_stock_and_restored(void)
{
    static const char *const MODES[] = { "stock", "restored" };
    for (int m = 0; m < 2; m++) {
        fresh(MODES[m]);
        CHECK(!car_looks_save_car(0, 16));
        CHECK(!car_looks_set("test-car", &(CarLook){ "race", "" }));
        CHECK(file("restomod.cfg") == NULL && file("stock.cfg") == NULL && file("restored.cfg") == NULL);
        platform_write_user_file("restomod.cfg", (const uint8_t *)"car=test-car\n", 13);
        CHECK_EQ_INT(car_looks_saved_car(0), -1);   /* Restomod's file is not this mode's */
        CHECK(car_looks_get("test-car").skin[0] == '\0');
    }
}

int main(void)
{
    car_registry_clear();
    add("test-car", "TESTCAR"), add("other", "OTHER");   /* 16, 17 */
    test_cars();
    test_looks();
    test_a_settings_save_keeps_them();
    test_stock_and_restored();
    return TEST_REPORT();
}
