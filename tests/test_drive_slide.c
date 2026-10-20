/* The loading pictures (drive_slide.h): 004571e8's file name and its "SCRN" shape with the palette after it. */
#include "game/viewer/drive_slide.h"
#include "game/sys/resource.h"
#include "test.h"

#include <string.h>

/* drive_slide_show's screen and files, not used here */
uint8_t *resource_load(const char *path, size_t *size) { (void)path, (void)size; return NULL; }
void resource_free(uint8_t *data) { (void)data; }
const char *resource_dir(GamePath which) { (void)which; return ""; }
void video_set_palette(int first, int count, const uint8_t *rgb) { (void)first, (void)count, (void)rgb; }
void video_blit_at_own_position(const FshEntry *shape) { (void)shape; }
void video_flip(void) {}

static uint8_t file[0x10 + 8 + 16 + 4 * 2 + 16 + 768];

static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24); }

int main(void)
{
    char name[64];
    drive_slide_name(name, sizeof name, "simdata/slides/", 6, 3);
    CHECK(strcmp(name, "simdata/slides/ANSXs3.qfs") == 0);
    drive_slide_name(name, sizeof name, "simdata/gslides/", 0, 0);
    CHECK(strcmp(name, "simdata/gslides/TSupras0.qfs") == 0);
    drive_slide_name(name, sizeof name, "simdata/slides/", 10, 6);   /* 004c5660[10] is MRX7, not the P911T of the models */
    CHECK(strcmp(name, "simdata/slides/MRX7s6.qfs") == 0);
    drive_slide_name(name, sizeof name, "simdata/slides/", 12, 0);   /* past the table: none */
    CHECK(strcmp(name, "") == 0);

    /* SHPI, one entry SCRN: a 4 x 2 bitmap at (0, 0), its block 16 + 8 long, then a 0x24 palette */
    memcpy(file, "SHPI", 4), put32(file + 4, sizeof file), put32(file + 8, 1), memcpy(file + 12, "GIMX", 4);
    memcpy(file + 16, "SCRN", 4), put32(file + 20, 24);
    uint8_t *e = file + 24;
    e[0] = 0x7b, e[1] = 16 + 8, e[4] = 4, e[6] = 2;
    for (int i = 0; i < 8; i++) e[16 + i] = (uint8_t)(i + 1);
    uint8_t *p = e + 24;
    p[0] = 0x24, p[4] = 0, p[5] = 1, p[6] = 1;   /* 256 x 1 */
    for (int i = 0; i < 768; i++) p[16 + i] = (uint8_t)i;
    FshEntry shape;
    const uint8_t *rgb = NULL;
    CHECK(drive_slide_parts(file, sizeof file, &shape, &rgb));
    CHECK_EQ_INT(shape.width, 4);
    CHECK_EQ_INT(shape.payload[7], 8);
    CHECK(rgb == p + 16);
    /* a palette past the file: refused */
    CHECK(!drive_slide_parts(file, sizeof file - 1, &shape, &rgb));
    /* 00476460's 6 to 8 bits */
    uint8_t six[768] = { 63, 1, 32, 0 }, eight[768];
    drive_slide_palette(six, eight);
    CHECK_EQ_INT(eight[0], 255);
    CHECK_EQ_INT(eight[1], 4);
    CHECK_EQ_INT(eight[2], 130);
    CHECK_EQ_INT(eight[3], 0);
    return TEST_REPORT();
}
