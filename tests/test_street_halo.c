/* The street lamps' halo end to end (street_halo.h): a lamp blob inside a noted board rectangle is lit, the same blob outside one (a sign the reflectors light) is not, and nothing is at day. */
#include "game/viewer/street_halo.h"
#include "game/platform/platform.h"
#include "tests/test.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* (port_settings reads the switch's file: no file is read or written here) */
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}
void platform_free_file(uint8_t *data) { free(data); }

enum { W = 96, H = 48, BACK = 100, LAMP_A = 10, LAMP_B = 11, LAMP_C = 12, WINDOW = 20 };

static uint8_t pixels[W * H];
static uint8_t palette[768];
static FshEntry board_a, board_b;

static void blob(int cx, int cy, int entry)
{
    for (int y = cy - 2; y < cy + 2; y++)
        for (int x = cx - 2; x < cx + 2; x++) pixels[y * W + x] = (uint8_t)entry;
}

static void scene(void)
{
    memset(pixels, BACK, sizeof pixels);
    blob(20, 24, LAMP_A);   /* a lamp's sprite */
    blob(70, 24, LAMP_A);   /* a sign painted in the same entries, in no rectangle */
    for (int c = 0; c < 256; c++) palette[3 * c] = palette[3 * c + 1] = palette[3 * c + 2] = (uint8_t)(c / 2);
    palette[3 * LAMP_A] = 255, palette[3 * LAMP_A + 1] = 150, palette[3 * LAMP_A + 2] = 40;
}

static int changed_near(const uint8_t *before, int cx, int cy)
{
    int n = 0;
    for (int y = cy - 14; y <= cy + 14; y++)
        for (int x = cx - 14; x <= cx + 14; x++)
            if (x >= 0 && x < W && y >= 0 && y < H && pixels[y * W + x] != before[y * W + x]) n++;
    return n;
}

static void run(int night, int *lamp_changed, int *sign_changed, int covering, int lit_sign)
{
    static uint8_t before[W * H];
    const FshEntry *const boards[3] = { &board_a, &board_b, NULL };
    const uint8_t entry[3] = { LAMP_A, LAMP_B, LAMP_C };
    const QuadTarget t = { pixels, W, 0, 0, W, H };
    scene();
    memcpy(before, pixels, sizeof before);
    street_halo_boards(boards, 2, entry, WINDOW);
    street_halo_frame_begin();
    const int32_t quad[8] = { 14, 18, 26, 18, 26, 30, 14, 30 };   /* the lamp's board, drawn */
    street_halo_note(&board_a, quad);
    const int32_t other[8] = { 60, 18, 80, 18, 80, 30, 60, 30 };
    street_halo_note(NULL, other);                                    /* not a lamp board: noted nowhere */
    const int32_t whole[8] = { -100000, -100000, 100000, -100000, 100000, 100000, -100000, 100000 };
    if (covering) street_halo_note(&board_b, whole);                  /* a board the near plane clips: it covers the view */
    if (lit_sign) street_halo_note_lit(NULL, other);                  /* the reflectors lit the sign's polygon */
    street_halo_draw(&t, palette, night);
    *lamp_changed = changed_near(before, 20, 24), *sign_changed = changed_near(before, 70, 24);
}

int main(void)
{
    setenv("TNFS_LAMP_HALOS", "1", 1);
    int lamp, sign;
    run(1, &lamp, &sign, 0, 0);
    CHECK(lamp > 0);                 /* the lamp is lit */
    CHECK_EQ_INT(sign, 0);           /* the sign is not */
    run(0, &lamp, &sign, 0, 0);
    CHECK_EQ_INT(lamp, 0);           /* day: no halo */
    setenv("TNFS_LAMP_HALOS", "0", 1);
    run(1, &lamp, &sign, 0, 0);
    CHECK_EQ_INT(lamp, 0);           /* switched off: none */
    setenv("TNFS_LAMP_HALOS", "1", 1);
    run(1, &lamp, &sign, 1, 0);
    CHECK(sign > 0);                 /* a covering board takes the sign's blob for a lamp's ... */
    run(0, &lamp, &sign, 0, 0);       /* (a day clears the halos the frame before still fades) */
    run(1, &lamp, &sign, 1, 1);
    CHECK(lamp > 0);                 /* ... unless the reflectors lit the sign's polygon: the lamp keeps its halo */
    CHECK_EQ_INT(sign, 0);           /* and the sign has none */
    street_halo_boards(NULL, 0, NULL, 0);
    street_halo_frame_begin();
    scene();
    const QuadTarget t = { pixels, W, 0, 0, W, H };
    static uint8_t before[W * H];
    memcpy(before, pixels, sizeof before);
    street_halo_draw(&t, palette, 1);
    CHECK(memcmp(before, pixels, sizeof before) == 0);   /* no lamp boards on this track */
    return TEST_REPORT();
}
