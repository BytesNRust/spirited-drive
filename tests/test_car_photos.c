/* The showcase photos of a car project (car_photos.h): the list of photos= as names, a safe file name for a picture the person chose (always one mod_ini_photo_name_ok
 * takes, none taken, none of the game's own), the copy into the piece (a PNG of 100x60 to 1280x960, staged then renamed), and the sweep of the files Studio made that
 * the card no longer names. */
#include "car_photos.h"
#include "formats/mod_ini.h"
#include "formats/png.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { N = MOD_INI_PHOTO_NAME_MAX + 1 };

static int write_png(const char *path, uint32_t w, uint32_t h)
{
    uint8_t *rgba = calloc((size_t)w * h, 4);
    uint8_t *png = NULL;
    size_t len = 0;
    int ok = rgba != NULL && png_write_rgba(rgba, w, h, &png, &len) == PNG_OK;
    FILE *f = ok ? fopen(path, "wb") : NULL;
    ok = ok && f != NULL && fwrite(png, 1, len, f) == len;
    if (f != NULL) fclose(f);
    png_free(png);
    free(rgba);
    return ok;
}

static int exists(const char *path) { return access(path, F_OK) == 0; }

static void test_list(void)
{
    char names[MOD_INI_PHOTOS_MAX][N], joined[300];
    CHECK_EQ_INT(car_photos_split("", names, MOD_INI_PHOTOS_MAX), 0);
    CHECK_EQ_INT(car_photos_split("a.png, b.png ,c.png", names, MOD_INI_PHOTOS_MAX), 3);
    CHECK(strcmp(names[0], "a.png") == 0 && strcmp(names[1], "b.png") == 0 && strcmp(names[2], "c.png") == 0);
    car_photos_join(names, 3, joined, sizeof joined);
    CHECK(strcmp(joined, "a.png,b.png,c.png") == 0);
    car_photos_join(names, 0, joined, sizeof joined);
    CHECK(strcmp(joined, "") == 0);
    CHECK_EQ_INT(car_photos_split("1.png,2.png,3.png,4.png,5.png,6.png,7.png,8.png,9.png", names, MOD_INI_PHOTOS_MAX), MOD_INI_PHOTOS_MAX);   /* the card keeps eight */
}

static void test_safe_names(void)
{
    char taken[MOD_INI_PHOTOS_MAX][N] = { "front.png" }, out[N];
    CHECK(car_photos_safe_name("/home/me/My Pictures/Front View (1).PNG", taken, 1, out, sizeof out));
    CHECK(strcmp(out, "front-view-1.png") == 0);   /* lower case, the odd characters one dash, .png */
    CHECK(mod_ini_photo_name_ok(out));
    CHECK(car_photos_safe_name("/x/front.png", taken, 1, out, sizeof out));
    CHECK(strcmp(out, "front-2.png") == 0);   /* taken: the next free one */
    CHECK(car_photos_safe_name("/x/card.png", taken, 1, out, sizeof out) && mod_ini_photo_name_ok(out) && strcmp(out, "card.png") != 0);   /* the game's own names are not photos */
    CHECK(car_photos_safe_name("/x/gallery.png", taken, 1, out, sizeof out) && mod_ini_photo_name_ok(out));
    CHECK(car_photos_safe_name("/x/preview.png", taken, 1, out, sizeof out) && mod_ini_photo_name_ok(out));
    CHECK(car_photos_safe_name("/x/a-very-long-picture-name-indeed-more-than-twenty-four.png", taken, 1, out, sizeof out));
    CHECK(strlen(out) <= MOD_INI_PHOTO_NAME_MAX && mod_ini_photo_name_ok(out));
    CHECK(car_photos_safe_name("/x/ñandú.png", taken, 1, out, sizeof out) && mod_ini_photo_name_ok(out));   /* nothing of the name is usable: still a name */
    CHECK(car_photos_safe_name("/x/.png", taken, 1, out, sizeof out) && mod_ini_photo_name_ok(out));
    for (int i = 0; i < 8; ++i) snprintf(taken[i], N, "p%d.png", i);   /* a name that is taken eight times over still comes out free */
    CHECK(car_photos_safe_name("/x/p1.png", taken, 8, out, sizeof out));
    for (int i = 0; i < 8; ++i) CHECK(strcmp(out, taken[i]) != 0);
}

static void test_import_and_sweep(const char *dir)
{
    char piece[600], src[600], dst[700], why[300] = "";
    snprintf(piece, sizeof piece, "%s/piece", dir);
    mkdir(piece, 0777);
    snprintf(src, sizeof src, "%s/ok.png", dir);
    CHECK(write_png(src, 450, 254));
    CHECK(car_photos_import(src, piece, "side.png", why, sizeof why));
    snprintf(dst, sizeof dst, "%s/side.png", piece);
    CHECK(exists(dst));
    uint32_t sw = 0, sh = 0;
    CHECK(car_photos_size(piece, "side.png", &sw, &sh) && sw == 450 && sh == 254);   /* from the header */
    CHECK(!car_photos_size(piece, "none.png", &sw, &sh));
    snprintf(dst, sizeof dst, "%s/side.png.tmp", piece);
    CHECK(!exists(dst));   /* staged and renamed */
    /* a picture the showcase cannot use: refused with its size, nothing copied */
    snprintf(src, sizeof src, "%s/small.png", dir);
    CHECK(write_png(src, 50, 30));
    CHECK(!car_photos_import(src, piece, "small.png", why, sizeof why) && strstr(why, "50x30") != NULL);
    snprintf(dst, sizeof dst, "%s/small.png", piece);
    CHECK(!exists(dst));
    snprintf(src, sizeof src, "%s/huge.png", dir);
    CHECK(write_png(src, 1300, 700));
    CHECK(!car_photos_import(src, piece, "huge.png", why, sizeof why) && strstr(why, "1300x700") != NULL);
    snprintf(src, sizeof src, "%s/notpng.png", dir);
    FILE *f = fopen(src, "wb");
    if (f != NULL) fputs("not a picture", f), fclose(f);
    CHECK(!car_photos_import(src, piece, "notpng.png", why, sizeof why) && why[0] != '\0');
    CHECK(!car_photos_import("/nonexistent/none.png", piece, "none.png", why, sizeof why) && why[0] != '\0');
    CHECK(!car_photos_import(src, "/nonexistent-piece", "x.png", why, sizeof why));
    /* the sweep: of the files Studio made, those the card no longer names go; the named ones, and any other file, stay */
    snprintf(src, sizeof src, "%s/ok.png", dir);
    CHECK(car_photos_import(src, piece, "second.png", why, sizeof why));
    snprintf(dst, sizeof dst, "%s/mine.png", piece);   /* a picture the author put there himself */
    CHECK(write_png(dst, 300, 200));
    char made[2][N] = { "side.png", "second.png" };
    CHECK_EQ_INT(car_photos_sweep(piece, made, 2, "second.png"), 1);
    snprintf(dst, sizeof dst, "%s/side.png", piece);
    CHECK(!exists(dst));
    snprintf(dst, sizeof dst, "%s/second.png", piece);
    CHECK(exists(dst));
    snprintf(dst, sizeof dst, "%s/mine.png", piece);
    CHECK(exists(dst));   /* not Studio's: never removed */
    CHECK_EQ_INT(car_photos_sweep(piece, made, 2, "second.png"), 0);   /* again: nothing left to remove */
}

int main(void)
{
    char dir[400];
    snprintf(dir, sizeof dir, "%s/car_photos_XXXXXX", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp");
    if (mkdtemp(dir) == NULL) return 2;
    test_list();
    test_safe_names();
    test_import_and_sweep(dir);
    return TEST_REPORT();
}
