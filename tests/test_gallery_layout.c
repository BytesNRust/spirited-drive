/* dep59 (the owner's v3b): the added car's gallery laid out like the Car Showcase (game/frontend/gallery_layout.h). The still part in
 * RGB: the band's patch mirrored, the picture framed with its shadow, the panels translucent, the rest the background as it is; quantised
 * with the picture's entries apart from the rest's; the title leant; the bottom row centred; the clicks' places. */
#include "game/frontend/gallery_layout.h"
#include "game/frontend/added_gallery.h"
#include "game/frontend/showcase.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t bgnd[GL_W * GL_H * 3], big[GL_PERF_W * GL_PERF_H * 3], small[GL_VID_W * GL_VID_H * 3], out[GL_W * GL_H * 3];

static const uint8_t *at(const uint8_t *rgb, int x, int y) { return rgb + 3 * (y * GL_W + x); }
static int blend(int under, int over, int alpha) { return (under * (255 - alpha) + over * alpha + 127) / 255; }

/* 1 when the pixel (x, y) of `out` is the background's there mixed alpha/255 towards rgb */
static int mixed(int x, int y, uint32_t rgb, int alpha)
{
    const uint8_t *o = at(out, x, y), *b = at(bgnd, x, y);
    for (int k = 0; k < 3; k++)
        if (o[k] != blend(b[k], (int)(rgb >> (16 - 8 * k) & 255), alpha)) return 0;
    return 1;
}

static int photo_is(int x0, int y0, int w, int h, const uint8_t *photo)
{
    int same = 1;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) same &= memcmp(at(out, x0 + x, y0 + y), photo + 3 * (y * w + x), 3) == 0;
    return same;
}

/* a tile's frame: GL_FRAME px of black all round, and its shadow GL_SHADOW px down and right, no more */
/* `edges`: 1 its top is checked, 2 its bottom, 4 its right side (a neighbour's frame or shadow lies over the others) */
static void check_tile(int x0, int y0, int w, int h, int edges)
{
    CHECK(mixed(x0 - 1, y0 + h / 2, 0, 255));
    CHECK(mixed(x0 - GL_FRAME, y0 + h / 2, 0, 255));
    CHECK(mixed(x0 + w, y0 + h / 2, 0, 255));
    CHECK(mixed(x0 + w + GL_FRAME - 1, y0 + h / 2, 0, 255));
    if (edges & 1) CHECK(mixed(x0 + w / 2, y0 - 1, 0, 255));
    if (edges & 2) CHECK(mixed(x0 + w / 2, y0 + h + GL_FRAME - 1, 0, 255));
    if (edges & 4) CHECK(mixed(x0 + w + GL_FRAME + 1, y0 + h / 2, 0, GL_SHADOW_ALPHA));   /* the shadow, right */
    if (edges & 2) CHECK(mixed(x0 + w / 2, y0 + h + GL_FRAME + 1, 0, GL_SHADOW_ALPHA));   /* and down */
    if (edges & 4) CHECK(mixed(x0 + w + GL_FRAME + GL_SHADOW - 1, y0 + h / 2, 0, GL_SHADOW_ALPHA));
    if (edges & 4) CHECK(mixed(x0 + w + GL_FRAME + GL_SHADOW, y0 + h / 2, 0, 0));   /* one depth: no more */
    if (edges & 2) CHECK(mixed(x0 + 2, y0 + h + GL_FRAME + 1, 0, 0));      /* not under its left, as a shadow falls */
}

static void test_compose(void)
{
    for (int y = 0; y < GL_H; y++)
        for (int x = 0; x < GL_W; x++) {
            uint8_t *b = bgnd + 3 * (y * GL_W + x);
            b[0] = (uint8_t)(x * 255 / GL_W), b[1] = (uint8_t)(y * 255 / GL_H), b[2] = (uint8_t)((x ^ y) & 255);
        }
    for (int i = 0; i < GL_PERF_W * GL_PERF_H; i++) big[3 * i] = 200, big[3 * i + 1] = 100, big[3 * i + 2] = 50;
    for (int i = 0; i < GL_VID_W * GL_VID_H; i++) small[3 * i] = 20, small[3 * i + 1] = 110, small[3 * i + 2] = 220;
    gallery_compose(bgnd, big, small, out);
    /* the strip over the title line is blurred (owner 2026-11-18): its blue channel, a noisy xor in the test's background, varies less from
     * column to column than the background's, and the first row under it is the background as it is */
    int rough_in = 0, rough_out = 0;
    for (int y = 0; y < GL_BAND_H; y++)
        for (int x = 1; x < GL_W; x++) {
            rough_in += abs(at(bgnd, x, y)[2] - at(bgnd, x - 1, y)[2]);
            rough_out += abs(at(out, x, y)[2] - at(out, x - 1, y)[2]);
        }
    CHECK(rough_out * 2 < rough_in);
    CHECK(memcmp(at(out, 100, GL_BAND_H + 1), at(bgnd, 100, GL_BAND_H + 1), 3) == 0);
    CHECK(photo_is(GL_PERF_X, GL_PERF_Y, GL_PERF_W, GL_PERF_H, big));         /* the photos in their tiles, the CD pages' places */
    CHECK(photo_is(GL_VID_X, GL_VID_Y, GL_VID_W, GL_VID_H, small));
    check_tile(GL_PERF_X, GL_PERF_Y, GL_PERF_W, GL_PERF_H, 0);   /* a gap of GL_SHADOW between two frames: every shadow shows whole */
    check_tile(GL_VID_X, GL_VID_Y, GL_VID_W, GL_VID_H, 0);
    check_tile(GL_HIST_X, GL_HIST_Y, GL_HIST_W, GL_HIST_H, 0);
    CHECK(mixed(GL_GEN_X + 40, GL_GEN_Y + 20, GL_PANEL_RGB, GL_FIGURES_ALPHA));   /* General: the translucent panel */
    CHECK(mixed(GL_HIST_X + 40, GL_HIST_Y + 20, GL_HISTORY_RGB, GL_HISTORY_ALPHA));   /* History: the sepia panel */
    CHECK(mixed(GL_COL_X + 40, GL_COL_Y + 20, GL_SKIN_RGB, 255));              /* the Color tile: filled */
    CHECK(mixed(GL_COL_X + GL_COL_W - 1, GL_COL_Y + GL_COL_H - 1, GL_SKIN_RGB, 255));
    CHECK(mixed(GL_COL_X - GL_FRAME, GL_COL_Y + 20, 0, 255));
    /* Mechanical's place (the CD's 82,326 254x87): General's translucent panel in its frame; the rest is the background as it is */
    CHECK(GL_MECH_X == 82 && GL_MECH_Y == 326 && GL_MECH_W == 254 && GL_MECH_H == 87);
    CHECK(mixed(GL_MECH_X + 40, GL_MECH_Y + 20, GL_PANEL_RGB, GL_FIGURES_ALPHA));
    check_tile(GL_MECH_X, GL_MECH_Y, GL_MECH_W, GL_MECH_H, 0);
    CHECK(mixed(30, 360, 0, 0));
    CHECK(mixed(630, 470, 0, 0));
    /* the CD's rectangles */
    CHECK(GL_PERF_X == 306 && GL_PERF_Y == 77 && GL_PERF_W == 225 && GL_PERF_H == 127);
    CHECK(GL_GEN_X == 133 && GL_GEN_Y == 173 && GL_VID_X == 306 && GL_VID_Y == 214 && GL_HIST_X == 455 && GL_HIST_Y == 245);
}

static void test_quantise(void)
{
    static uint8_t rgb[GL_W * GL_H * 3], pixels[GL_W * GL_H];
    uint8_t palette[768];
    const int perf_in = 0;
    (void)perf_in;
    for (int y = 0; y < GL_H; y++)
        for (int x = 0; x < GL_W; x++) {
            uint8_t *p = rgb + 3 * (y * GL_W + x);
            const int in = (x >= GL_PERF_X && x < GL_PERF_X + GL_PERF_W && y >= GL_PERF_Y && y < GL_PERF_Y + GL_PERF_H) ||
                           (x >= GL_VID_X && x < GL_VID_X + GL_VID_W && y >= GL_VID_Y && y < GL_VID_Y + GL_VID_H);
            if (in) p[0] = (uint8_t)(x * 7), p[1] = (uint8_t)(y * 5), p[2] = (uint8_t)(x * y);   /* many colours */
            else p[0] = 90, p[1] = 70, p[2] = (uint8_t)(y < 240 ? 40 : 20);                    /* two */
        }
    enum { COLOURS = 219 };
    CHECK(gallery_quantise(rgb, COLOURS, palette, pixels));
    int ok = 1;
    for (int y = 0; y < GL_H; y++)
        for (int x = 0; x < GL_W; x++) {
            const int in = (x >= GL_PERF_X && x < GL_PERF_X + GL_PERF_W && y >= GL_PERF_Y && y < GL_PERF_Y + GL_PERF_H) ||
                           (x >= GL_VID_X && x < GL_VID_X + GL_VID_W && y >= GL_VID_Y && y < GL_VID_Y + GL_VID_H);
            const int i = pixels[y * GL_W + x];
            ok &= in ? i < GL_PICTURE_COLOURS : i >= GL_PICTURE_COLOURS && i < COLOURS;
        }
    CHECK(ok);
    const uint8_t *two = palette + 3 * pixels[10 * GL_W + 10], *other = palette + 3 * pixels[400 * GL_W + 10];
    CHECK(two[0] == 90 && two[1] == 70 && two[2] == 40);   /* a few colours come back as they are */
    CHECK(other[0] == 90 && other[1] == 70 && other[2] == 20);
}

static void test_lean(void)
{
    enum { W = 4, H = 3, OW = 8 };
    uint8_t mask[W * H] = { 0 }, leant[OW * H];
    mask[0] = 15, mask[2 * W + 1] = 9;   /* the top row's first, the foot row's second */
    gallery_lean(mask, W, H, 1, 1, leant, OW);
    CHECK_EQ_INT(leant[0 * OW + 2], 15);   /* two rows up from the foot: two to the right */
    CHECK_EQ_INT(leant[2 * OW + 1], 9);    /* the foot row stays */
    int sum = 0;
    for (int i = 0; i < OW * H; i++) sum += leant[i];
    CHECK_EQ_INT(sum, 24);   /* nothing else */
}

/* owner 2026-11-17: the bottom row as the CD's showcase pages have it, cut from their art: the letters' and arrows' own pixels (cream,
 * yellow), never the art's red or pink; a shape replaces what is under its rectangle; drawn over its shadow 2 px down and right */
static void test_row_put_and_draw(void)
{
    static GalleryRow row;
    static uint8_t pal[768];
    static const uint8_t CREAM[3] = { 236, 212, 188 }, YELLOW[3] = { 252, 204, 0 }, RED[3] = { 150, 0, 0 }, PINK[3] = { 199, 97, 97 };
    memcpy(pal + 3, CREAM, 3), memcpy(pal + 6, RED, 3), memcpy(pal + 9, YELLOW, 3), memcpy(pal + 12, PINK, 3);
    CHECK(gallery_row_is_face(CREAM) && gallery_row_is_face(YELLOW) && !gallery_row_is_face(RED) && !gallery_row_is_face(PINK));
    gallery_row_clear(&row);
    static const uint8_t PICTURE[2 * 4] = { 1, 2, 3, 4, 2, 1, 1, 0 };   /* 4 x 2 at (100, 440) */
    gallery_row_put(&row, GL_ROW_PLAIN, PICTURE, 4, 2, 100, 440, pal);
    const uint8_t *f = row.face[GL_ROW_PLAIN];
    const int at = (440 - GL_ROW_Y) * GL_ROW_W + 100;
    CHECK(f[at] != 0 && f[at + 1] == 0 && f[at + 2] != 0 && f[at + 3] == 0);
    CHECK(f[at + GL_ROW_W] == 0 && f[at + GL_ROW_W + 1] != 0 && f[at + GL_ROW_W + 3] == 0);
    CHECK(memcmp(row.rgb[f[at] - 1], CREAM, 3) == 0 && memcmp(row.rgb[f[at + 2] - 1], YELLOW, 3) == 0);   /* the art's own colours */
    CHECK_EQ_INT(row.colours, 2);
    static const uint8_t DARK[2] = { 2, 2 };   /* a shape: what is under its rectangle is its own */
    gallery_row_put(&row, GL_ROW_PLAIN, DARK, 2, 1, 100, 440, pal);
    CHECK(f[at] == 0 && f[at + 1] == 0 && f[at + 2] != 0);
    gallery_row_put(&row, GL_ROW_PLAIN, PICTURE, 4, 2, 100, GL_ROW_Y + GL_ROW_H, pal);   /* below the row: nothing */
    gallery_row_put(&row, GL_ROW_PLAIN, PICTURE, 4, 2, GL_ROW_W, 440, pal);              /* past it: nothing */
    int faces = 0;
    for (int i = 0; i < GL_ROW_H * GL_ROW_W; i++) faces += f[i] != 0;
    CHECK_EQ_INT(faces, 3);

    static uint8_t screen[GL_W * GL_H];
    memset(screen, 0, sizeof screen);
    uint8_t map[GL_ROW_COLOURS];
    for (int k = 0; k < GL_ROW_COLOURS; k++) map[k] = (uint8_t)(50 + k);
    gallery_row_draw(&row, GL_ROW_PLAIN, screen, GL_W, GL_H, map, 9);
    CHECK_EQ_INT(screen[440 * GL_W + 102], 50 + f[at + 2] - 1);
    CHECK_EQ_INT(screen[441 * GL_W + 101], 50 + f[at + GL_ROW_W + 1] - 1);
    CHECK_EQ_INT(screen[442 * GL_W + 104], 9);    /* (102, 440)'s shadow */
    CHECK_EQ_INT(screen[443 * GL_W + 103], 9);    /* (101, 441)'s */
    CHECK_EQ_INT(screen[440 * GL_W + 100], 0);    /* not a face: the page's own */
    CHECK_EQ_INT(gallery_row_look(GALLERY_DONE), GL_ROW_DONE);
    CHECK_EQ_INT(gallery_row_look(GALLERY_PREVIOUS), GL_ROW_PREVIOUS);
    CHECK_EQ_INT(gallery_row_look(GALLERY_NEXT), GL_ROW_NEXT);
    CHECK_EQ_INT(gallery_row_look(GALLERY_SKIN), GL_ROW_PLAIN);
}

/* the box (inclusive) of the pixels of look `look` in colour `rgb` within columns x0..x1 */
static int row_box(const GalleryRow *row, int look, const uint8_t rgb[3], int x0, int x1, int box[4])
{
    int n = 0;
    box[0] = box[1] = 9999, box[2] = box[3] = -1;
    for (int y = 0; y < GL_ROW_H; y++)
        for (int x = x0; x <= x1; x++) {
            const int k = row->face[look][y * GL_ROW_W + x];
            if (k == 0 || memcmp(row->rgb[k - 1], rgb, 3) != 0) continue;
            n++;
            if (x < box[0]) box[0] = x;
            if (x > box[2]) box[2] = x;
            if (GL_ROW_Y + y < box[1]) box[1] = GL_ROW_Y + y;
            if (GL_ROW_Y + y > box[3]) box[3] = GL_ROW_Y + y;
        }
    return n;
}

/* with the CD: cut from the 512TR's showcase art, the letters and arrows where the CD's page has them (measured on
 * notes/captures/showcase-cd-512tr-640x480.png: DONE 11..72 x 445..455, "Next car" 256..358 x 445..455, the arrows 225..233 and
 * 380..388 x 442..458, cream 236, 212, 188), lit in yellow 252, 204, 0 */
static void test_row_from_the_cd(void)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/original/FRONTEND/SHOW/512TR.QFS", TNFS_SOURCE_DIR);
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        printf("skip: no CD at %s\n", path);
        return;
    }
    static uint8_t file[1 << 20];
    const size_t size = fread(file, 1, sizeof file, f);
    fclose(f);
    static GalleryRow row;
    CHECK_EQ_INT(gallery_row_cut(&row, file, size), 1);
    static const uint8_t CREAM[3] = { 236, 212, 188 }, YELLOW[3] = { 252, 204, 0 };
    static const struct { int look, x0, x1, want[4]; const uint8_t *rgb; } AT[] = {
        { GL_ROW_PLAIN, 0, 140, { 11, 445, 72, 455 }, CREAM }, { GL_ROW_PLAIN, 245, 370, { 256, 445, 358, 455 }, CREAM },
        { GL_ROW_PLAIN, 200, 244, { 225, 442, 233, 458 }, CREAM }, { GL_ROW_PLAIN, 372, 419, { 380, 442, 388, 458 }, CREAM },
        { GL_ROW_NEXT, 372, 419, { 380, 442, 388, 458 }, YELLOW }, { GL_ROW_PREVIOUS, 200, 244, { 225, 442, 233, 458 }, YELLOW },
        { GL_ROW_DONE, 0, 140, { 11, 445, 72, 455 }, YELLOW },
    };
    for (size_t i = 0; i < sizeof AT / sizeof AT[0]; i++) {
        int box[4];
        CHECK(row_box(&row, AT[i].look, AT[i].rgb, AT[i].x0, AT[i].x1, box) > 0);
        if (memcmp(box, AT[i].want, sizeof box) != 0) printf("look %d at %d..%d: %d,%d..%d,%d\n", AT[i].look, AT[i].x0, AT[i].x1, box[0], box[1], box[2], box[3]);
        CHECK(memcmp(box, AT[i].want, sizeof box) == 0);
    }
    int box[4];
    CHECK_EQ_INT(row_box(&row, GL_ROW_PLAIN, YELLOW, 0, GL_ROW_W - 1, box), 0);   /* nothing lit when nothing is */
    CHECK_EQ_INT(row_box(&row, GL_ROW_NEXT, YELLOW, 0, 371, box), 0);             /* Next's look: only its arrow */
}

/* the open SETUP panel: a row by its y; on Color and Skin the value column's halves step and the label's side takes the row alone; a switch
 * (Transmission, ABS, Traction control) changes with a click anywhere on its row, as in Options; outside is nothing */
static void test_setup_hit(void)
{
    int step = 9;
    for (int row = 0; row < GALLERY_SETUP_ROWS; row++) {
        const int y = GL_SETUP_Y + GL_SETUP_ROW0 + row * GL_SETUP_STEP + 5, sw = row >= SETUP_TRANSMISSION;
        CHECK_EQ_INT(gallery_setup_hit(GL_SETUP_X + GL_SETUP_VALUE_X + 5, y, &step), row);
        CHECK_EQ_INT(step, sw ? 1 : -1);
        CHECK_EQ_INT(gallery_setup_hit(GL_SETUP_X + GL_SETUP_VALUE_X + GL_SETUP_VALUE_W - 5, y, &step), row);
        CHECK_EQ_INT(step, 1);
        CHECK_EQ_INT(gallery_setup_hit(GL_SETUP_X + 20, y, &step), row);
        CHECK_EQ_INT(step, sw ? 1 : 0);
    }
    CHECK_EQ_INT(gallery_setup_hit(GL_SETUP_X + 20, GL_SETUP_Y + 10, &step), -1);             /* the title */
    CHECK_EQ_INT(gallery_setup_hit(GL_SETUP_X + 20, GL_SETUP_Y + GL_SETUP_H + 3, &step), -1);  /* under the panel */
    CHECK_EQ_INT(gallery_setup_hit(GL_SETUP_X + GL_SETUP_W + 3, GL_SETUP_Y + 60, &step), -1);  /* beside it */
    CHECK_EQ_INT(gallery_setup_hit(GL_SETUP_X + 20, GL_SETUP_Y + GL_SETUP_ROW0 + 5 * GL_SETUP_STEP + 8, &step), -1);   /* under the rows */
}

static void test_hit(void)
{
    int step = 9;
    /* the SETUP tile (the Color tile's place): one item, no step */
    CHECK_EQ_INT(gallery_hit(GL_COL_X + 10, GL_COL_Y + 10, &step), GALLERY_SETUP);
    CHECK_EQ_INT(step, 0);
    CHECK_EQ_INT(gallery_hit(GL_COL_X + GL_COL_W - 3, GL_COL_Y + GL_COL_H - 3, &step), GALLERY_SETUP);
    CHECK_EQ_INT(step, 0);
    /* the big photo: its left and right zones step, the middle takes a click without a step */
    CHECK_EQ_INT(gallery_hit(GL_PERF_X + 5, GL_PERF_Y + 60, &step), GALLERY_PHOTOS);
    CHECK_EQ_INT(step, -1);
    CHECK_EQ_INT(gallery_hit(GL_PERF_X + GL_PERF_W - 5, GL_PERF_Y + 60, &step), GALLERY_PHOTOS);
    CHECK_EQ_INT(step, 1);
    CHECK_EQ_INT(gallery_hit(GL_PERF_X + GL_PERF_W / 2, GL_PERF_Y + 60, &step), GALLERY_PHOTOS);
    CHECK_EQ_INT(step, 0);
    CHECK_EQ_INT(gallery_hit(GL_HIST_X + 20, GL_HIST_Y + 20, &step), GALLERY_HISTORY);
    CHECK_EQ_INT(gallery_hit(200, 200, &step), -1);                      /* General: figures, no item */
    CHECK_EQ_INT(gallery_hit(GL_VID_X + 20, GL_VID_Y + 20, &step), GALLERY_RECORDS);  /* Video's tile is RECORDS */
    CHECK_EQ_INT(gallery_hit(150, 360, &step), -1);                      /* Mechanical's place: empty */
    CHECK_EQ_INT(gallery_hit(GL_COL_X + 20, GL_COL_Y - 3, &step), -1);   /* above the Color tile */
    /* the bottom row: the CD showcase's own boxes (showcase.c BOX_*, ends included), the words between the arrows in none */
    static const struct { int x, y, item; } AT[] = {
        { 200, 430, GALLERY_PREVIOUS }, { 260, 480, GALLERY_PREVIOUS }, { 229, 450, GALLERY_PREVIOUS },
        { 360, 430, GALLERY_NEXT }, { 420, 480, GALLERY_NEXT }, { 384, 450, GALLERY_NEXT },
        { 0, 430, GALLERY_DONE }, { 130, 480, GALLERY_DONE }, { 40, 450, GALLERY_DONE },
        { 199, 450, -1 }, { 261, 450, -1 }, { 300, 450, -1 }, { 359, 450, -1 }, { 421, 450, -1 }, { 131, 450, -1 }, { 230, 429, -1 },
    };
    for (size_t i = 0; i < sizeof AT / sizeof AT[0]; i++) {
        step = 9;
        const int got = gallery_hit(AT[i].x, AT[i].y, &step);
        if (got != AT[i].item) printf("gallery_hit(%d, %d) = %d, want %d\n", AT[i].x, AT[i].y, got, AT[i].item);
        CHECK_EQ_INT(got, AT[i].item);
        if (got >= 0) CHECK_EQ_INT(step, 0);
    }
    CHECK(SHOWCASE_PREVIOUS_LEFT == 200 && SHOWCASE_PREVIOUS_RIGHT == 260 && SHOWCASE_NEXT_LEFT == 360 && SHOWCASE_NEXT_RIGHT == 420);
    CHECK(SHOWCASE_DONE_LEFT == 0 && SHOWCASE_DONE_RIGHT == 130 && SHOWCASE_BOTTOM_TOP == 430 && SHOWCASE_BOTTOM_BOTTOM == 480);
}

int main(void)
{
    test_compose();
    test_quantise();
    test_lean();
    test_row_put_and_draw();
    test_row_from_the_cd();
    test_hit();
    test_setup_hit();
    return TEST_REPORT();
}
