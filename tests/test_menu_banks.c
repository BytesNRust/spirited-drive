/* The menus' banks (ea_lib.c): a mod's art bank the menus cannot read within its bytes is refused as a missing one, and a menu
 * whose bank is missing, refused or without a '!pal' gets a grey ramp for its palette, where it read the palette from no shape
 * (every menu read its '!pal' unchecked: the game crashed) */
#include "tests/test.h"

#include "formats/fsh.h"
#include "game/frontend/ea_lib.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* the video and pointer calls ea_lib makes: none needed here */
void video_select_target(uint8_t *pixels, int w, int h) { (void)pixels, (void)w, (void)h; }
uint8_t *video_target(int *w, int *h) { static uint8_t px[1]; *w = *h = 0; return px; }
void video_get_palette(uint8_t rgb[768]) { memset(rgb, 0, 768); }
void video_read_rect(int x, int y, int w, int h, uint8_t *out) { (void)x, (void)y, (void)w, (void)h, (void)out; }
void video_blit(const FshEntry *shape, int x, int y) { (void)shape, (void)x, (void)y; }
void video_blit_at_own_position(const FshEntry *shape) { (void)shape; }
void video_blit_transparent(const FshEntry *shape, int x, int y) { (void)shape, (void)x, (void)y; }
void video_blit_transparent_at_own_position(const FshEntry *shape) { (void)shape; }
void pointer_range(int x0, int y0, int x1, int y1) { (void)x0, (void)y0, (void)x1, (void)y1; }

/* the one file the platform serves: `file`, `file_size` bytes */
static const uint8_t *file;
static size_t file_size;
uint8_t *platform_read_file(const char *path, size_t *size)
{
    (void)path;
    if (file == NULL) return NULL;
    uint8_t *copy = malloc(file_size);
    if (copy == NULL) return NULL;
    memcpy(copy, file, file_size);
    *size = file_size;
    return copy;
}
void platform_free_file(uint8_t *data) { free(data); }
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size) { (void)name, (void)data, (void)size; return false; }

static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24); }

/* a bank whose directory says 1000 shapes in 32 bytes: refused (0), not searched past its end */
static void test_bank_refused(void)
{
    static uint8_t bad[32];
    memcpy(bad, "SHPI", 4), put32(bad + 4, sizeof bad), put32(bad + 8, 1000), memcpy(bad + 12, "GIMX", 4);
    file = bad, file_size = sizeof bad;
    const int32_t bank = ea_bank_load("option/options.qfs");
    CHECK_EQ_INT(bank, 0);
    CHECK(ea_shape(bank, "bgnd") == NULL);
    ea_bank_free(bank);
    file = NULL;
}

/* a bank that fits is kept, and its shapes found */
static void test_bank_kept(void)
{
    static uint8_t good[0x30];
    memcpy(good, "SHPI", 4), put32(good + 4, sizeof good), put32(good + 8, 1), memcpy(good + 12, "GIMX", 4);
    memcpy(good + 16, "bgnd", 4), put32(good + 20, 0x18);
    good[0x18] = 0x7b, good[0x1c] = 2, good[0x1e] = 2;
    file = good, file_size = sizeof good;
    const int32_t bank = ea_bank_load("option/options.qfs");
    CHECK(bank != 0);
    CHECK(ea_shape(bank, "bgnd") != NULL);
    const uint8_t *pal = ea_shape(bank, "!pal");   /* it has none: the grey ramp */
    CHECK(pal != NULL && pal[0] == 0x22);
    ea_bank_free(bank);
    file = NULL;
}

/* no bank: the grey ramp, 6 bits a channel as the CD's palettes; any other shape none, as before */
static void test_palette_without_bank(void)
{
    const uint8_t *pal = ea_shape(0, "!pal");
    CHECK(pal != NULL);
    if (pal == NULL) return;
    CHECK_EQ_INT(pal[0], 0x22);
    CHECK_EQ_INT(pal[0x10 + 3 * 255], 63);
    CHECK_EQ_INT(pal[0x10 + 3 * 128 + 1], 32);
    CHECK(ea_shape(0, "bgnd") == NULL);
}

int main(void)
{
    test_bank_refused();
    test_bank_kept();
    test_palette_without_bank();
    return TEST_REPORT();
}
