/* Spirited Drive's own window icon (port_icon_ours.h): the sizes the repository ships under data/icon, smallest first, a mod's copy before them, and a file
 * that is not the square PNG its name says passed over. */
#include "formats/png.h"
#include "game/sys/port_icon_ours.h"
#include "test.h"

#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char root[64];

static int remove_entry(const char *path, const struct stat *st, int type, struct FTW *ftw)
{
    (void)st, (void)type, (void)ftw;
    return remove(path);
}

static void write_bytes(const char *path, const uint8_t *data, size_t len)
{
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f == NULL) return;
    CHECK(fwrite(data, 1, len, f) == len);
    fclose(f);
}

/* a `side` x `side` PNG of one colour at <root>/icon/<name> */
static void write_flat(const char *name, int side, const uint8_t rgba[4])
{
    uint8_t *pixels = malloc((size_t)side * side * 4);
    for (int i = 0; i < side * side; i++) memcpy(pixels + i * 4, rgba, 4);
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK(png_write_rgba(pixels, (uint32_t)side, (uint32_t)side, &png, &len) == PNG_OK);
    char path[256];
    snprintf(path, sizeof path, "%s/icon/%s", root, name);
    write_bytes(path, png, len);
    png_free(png);
    free(pixels);
}

static int opaque_pixels(const PortIconImage *image)
{
    int n = 0;
    for (int i = 0; i < image->side * image->side; i++) n += image->rgba[i * 4 + 3] != 0;
    return n;
}

static void test_the_shipped_sizes(void)
{
    PortIconImage icons[PORT_ICON_SIZES];
    const int n = port_icon_ours_from(NULL, NULL, TNFS_SOURCE_DIR, icons);
    CHECK_EQ_INT(n, PORT_ICON_SIZES);
    static const int sides[PORT_ICON_SIZES] = { 32, 48, 64, 128, 256 };
    for (int i = 0; i < n; i++) {
        CHECK_EQ_INT(icons[i].side, sides[i]);   /* smallest first: the 32 px key is the icon at 100% scale */
        CHECK(icons[i].rgba != NULL && opaque_pixels(&icons[i]) > icons[i].side * icons[i].side / 4);
    }
    port_icon_ours_free(icons, n);
}

static void test_none_without_the_files(void)
{
    PortIconImage icons[PORT_ICON_SIZES];
    CHECK_EQ_INT(port_icon_ours_from(NULL, NULL, root, icons), 0);   /* a source tree with no data/icon: the caller takes the CD's icon */
}

static void test_a_mods_copy_first_and_bad_files_passed_over(void)
{
    char dir[128];
    snprintf(dir, sizeof dir, "%s/icon", root);
    CHECK(mkdir(dir, 0755) == 0);
    static const uint8_t green[4] = { 0, 200, 0, 255 };
    write_flat("spirited_32.png", 32, green);
    write_flat("spirited_48.png", 40, green);   /* not the size its name says */
    char junk_path[256];
    snprintf(junk_path, sizeof junk_path, "%s/spirited_64.png", dir);
    write_bytes(junk_path, (const uint8_t *)"not a png", 9);
    PortIconImage icons[PORT_ICON_SIZES];
    const int n = port_icon_ours_from(root, NULL, TNFS_SOURCE_DIR, icons);
    CHECK_EQ_INT(n, PORT_ICON_SIZES);
    CHECK(n > 2 && memcmp(icons[0].rgba, green, 4) == 0);                            /* the mod's 32 */
    CHECK(n > 2 && icons[1].side == 48 && memcmp(icons[1].rgba + 24 * 48 * 4 + 24 * 4, green, 4) != 0);   /* the shipped 48, not the mod's wrong one */
    CHECK(n > 2 && icons[2].side == 64);                                             /* the shipped 64, the mod's is no PNG */
    port_icon_ours_free(icons, n);
    CHECK_EQ_INT(port_icon_ours_from(root, NULL, NULL, icons), 1);                   /* the mod alone: only its good file */
    port_icon_ours_free(icons, 1);
}

/* on macOS the window icon replaces the Dock icon while the game runs, and these full-bleed pictures would show bigger than every other app's: the bundle's
 * .icns, on Apple's grid, already gives the Dock icon there */
static void test_the_window_icon_left_to_the_bundle_on_macos(void)
{
#ifdef __APPLE__
    CHECK_EQ_INT(port_icon_sets_window(), 0);
#else
    CHECK_EQ_INT(port_icon_sets_window(), 1);
#endif
}

int main(void)
{
    test_the_window_icon_left_to_the_bundle_on_macos();
    snprintf(root, sizeof root, "%s", "/tmp/tnfs_test_icon.XXXXXX");
    if (mkdtemp(root) == NULL) {
        perror("mkdtemp");
        return 1;
    }
    test_the_shipped_sizes();
    test_none_without_the_files();
    test_a_mods_copy_first_and_bad_files_passed_over();
    nftw(root, remove_entry, 8, FTW_DEPTH | FTW_PHYS);
    return TEST_REPORT();
}
