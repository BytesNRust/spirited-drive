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

static uint8_t bgnd[GL_W * GL_H * 3], picture[GL_PICTURE_W * GL_PICTURE_H * 3], out[GL_W * GL_H * 3];

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

static void test_compose(void)
{
    for (int y = 0; y < GL_H; y++)
        for (int x = 0; x < GL_W; x++) {
            uint8_t *b = bgnd + 3 * (y * GL_W + x);
            b[0] = (uint8_t)(x * 255 / GL_W), b[1] = (uint8_t)(y * 255 / GL_H), b[2] = (uint8_t)((x ^ y) & 255);
        }
    for (int i = 0; i < GL_PICTURE_W * GL_PICTURE_H; i++) picture[3 * i] = 200, picture[3 * i + 1] = 100, picture[3 * i + 2] = 50;
    gallery_compose(bgnd, picture, out);
    int band = 1;   /* the band's patch: the texture beside it, mirrored */
    for (int y = 0; y < GL_BAND_H; y++)
        for (int x = 0; x < GL_BAND_W; x++) band &= memcmp(at(out, x, y), at(bgnd, GL_BAND_FROM_X + GL_BAND_W - 1 - x, y), 3) == 0;
    CHECK(band);
    CHECK(memcmp(at(out, GL_BAND_W, 10), at(bgnd, GL_BAND_W, 10), 3) == 0);   /* past it: the band itself */
    int pic = 1;
    for (int y = GL_PICTURE_Y; y < GL_PICTURE_Y + GL_PICTURE_H; y++)
        for (int x = GL_PICTURE_X; x < GL_PICTURE_X + GL_PICTURE_W; x++) pic &= memcmp(at(out, x, y), picture, 3) == 0;
    CHECK(pic);
    CHECK(mixed(GL_PICTURE_X - 1, GL_PICTURE_Y + 50, 0, 255));                               /* the frame: black */
    CHECK(mixed(GL_PICTURE_X + GL_PICTURE_W + GL_FRAME - 1, GL_PICTURE_Y + 50, 0, 255));
    CHECK(mixed(GL_PICTURE_X + GL_PICTURE_W + GL_FRAME + 1, GL_PICTURE_Y + 50, 0, GL_SHADOW_ALPHA));   /* its shadow, right */
    CHECK(mixed(GL_PICTURE_X + 50, GL_PICTURE_Y + GL_PICTURE_H + GL_FRAME + 1, 0, GL_SHADOW_ALPHA));   /* and down */
    CHECK(mixed(GL_PICTURE_X + 2, GL_PICTURE_Y + GL_PICTURE_H + GL_FRAME + 1, 0, 0));       /* not under its left, as a shadow falls */
    CHECK(mixed(GL_COLUMN_X + 40, GL_FIGURES_Y + 20, GL_PANEL_RGB, GL_FIGURES_ALPHA));       /* the technical data's panel */
    CHECK(mixed(GL_COLUMN_X, GL_FIGURES_Y + 20, GL_PANEL_EDGE_RGB, 255));                   /* its edge */
    CHECK(mixed(GL_COLUMN_X + GL_COLUMN_W + 2, GL_SKIN_Y + GL_BOX_H + 2, 0, GL_SHADOW_ALPHA));   /* Skin's shadow */
    CHECK(mixed(GL_COLUMN_X + GL_COLUMN_W + 2, GL_COLOUR_Y + GL_BOX_H + 2, 0, GL_SHADOW_ALPHA)); /* Color's */
    CHECK(mixed(GL_COLUMN_X + 40, GL_SKIN_Y + 20, GL_SKIN_RGB, 255));                        /* the boxes: filled */
    CHECK(mixed(GL_COLUMN_X + GL_COLUMN_W - 1, GL_COLOUR_Y + GL_BOX_H - 1, GL_SKIN_RGB, 255));
    CHECK(mixed(GL_FOOT_X0 + 100, GL_FOOT_Y0 + 20, GL_PANEL_RGB, GL_FOOT_ALPHA));           /* the foot's band */
    /* one depth for every shadow: the picture's frame and the two boxes cast GL_SHADOW px, no more */
    const int fx1 = GL_PICTURE_X + GL_PICTURE_W - 1 + GL_FRAME, bx1 = GL_COLUMN_X + GL_COLUMN_W - 1;
    CHECK(mixed(fx1 + GL_SHADOW, GL_PICTURE_Y + 50, 0, GL_SHADOW_ALPHA));
    CHECK(mixed(fx1 + GL_SHADOW + 1, GL_PICTURE_Y + 50, 0, 0));
    CHECK(mixed(bx1 + GL_SHADOW, GL_SKIN_Y + 20, 0, GL_SHADOW_ALPHA));
    CHECK(mixed(bx1 + GL_SHADOW + 1, GL_SKIN_Y + 20, 0, 0));
    CHECK(mixed(GL_COLUMN_X + 40, GL_COLOUR_Y + GL_BOX_H - 1 + GL_SHADOW, 0, GL_SHADOW_ALPHA));
    CHECK(mixed(GL_COLUMN_X + 40, GL_COLOUR_Y + GL_BOX_H + GL_SHADOW, 0, 0));
    CHECK(mixed(630, 470, 0, 0));                                                            /* the rest: the background */
    CHECK(mixed(GL_COLUMN_X + 40, GL_FIGURES_Y + GL_FIGURES_H + 4, 0, 0));                  /* the gap under the panel */
    /* the right column ends level with the picture */
    CHECK_EQ_INT(GL_COLOUR_Y + GL_BOX_H - 1, GL_OUTER_Y1);
    CHECK_EQ_INT(GL_SKIN_Y - (GL_FIGURES_Y + GL_FIGURES_H), GL_COLOUR_Y - (GL_SKIN_Y + GL_BOX_H));   /* the gaps even */
}

/* the outer box (the frame's outside) of the element whose pixels at row y / column x differ from the flat ground: first and last x of row y
 * from x_from, first and last y of column x from y_from, stopping at the first pixel that is the ground again */
static const uint8_t GROUND[3] = { 200, 180, 150 };
static int ground(int x, int y) { return memcmp(at(out, x, y), GROUND, 3) == 0; }
static int shade(int x, int y) { return mixed(x, y, 0, GL_SHADOW_ALPHA); }

/* how many shadow pixels run from (x, y) in the direction (dx, dy) */
static int run(int x, int y, int dx, int dy)
{
    int n = 0;
    while (x >= 0 && y >= 0 && x < GL_W && y < GL_H && shade(x, y)) n++, x += dx, y += dy;
    return n;
}

/* the owner's: the picture and the right column line up by the frames' outsides, and every shadow falls the same way, as deep */
static void test_edges_and_shadows(void)
{
    for (int i = 0; i < GL_W * GL_H; i++) memcpy(bgnd + 3 * i, GROUND, 3);
    for (int i = 0; i < GL_PICTURE_W * GL_PICTURE_H; i++) picture[3 * i] = 10, picture[3 * i + 1] = 200, picture[3 * i + 2] = 10;
    gallery_compose(bgnd, picture, out);
    /* the picture's frame, outside: scanned from the middle of the picture outwards */
    const int mx = GL_PICTURE_X + GL_PICTURE_W / 2, my = GL_PICTURE_Y + GL_PICTURE_H / 2;
    int top = my, bottom = my, left = mx, right = mx;
    while (!ground(mx, top - 1) && !shade(mx, top - 1)) top--;
    while (!shade(mx, bottom + 1)) bottom++;
    while (!ground(left - 1, my) && !shade(left - 1, my)) left--;
    while (!shade(right + 1, my)) right++;
    /* the Color box, outside: its fill scanned from inside to its last pixel before the shadow */
    const int cy = GL_COLOUR_Y + GL_BOX_H / 2;
    int c_bottom = cy, c_right = GL_COLUMN_X + GL_COLUMN_W / 2;
    while (!shade(c_right + 1, cy)) c_right++;
    while (!shade(GL_COLUMN_X + 40, c_bottom + 1)) c_bottom++;
    int f_top = GL_FIGURES_Y + 10;   /* the technical data's panel, its top edge */
    while (!ground(GL_COLUMN_X + 40, f_top - 1)) f_top--;
    CHECK_EQ_INT(c_bottom, bottom);   /* the right column's foot level with the picture frame's outside */
    CHECK_EQ_INT(f_top, top);         /* and its top */
    int foot_left = GL_FOOT_X0 + 50, foot_right = GL_FOOT_X0 + 50, fy = GL_FOOT_Y0 + 20;
    while (!ground(foot_left - 1, fy)) foot_left--;
    while (!ground(foot_right + 1, fy)) foot_right++;
    CHECK_EQ_INT(foot_left, left);    /* the foot's band from the picture frame's outside to the column's */
    CHECK_EQ_INT(foot_right, c_right);
    /* the shadows: as deep right and down, starting as far down and right of each one's corner */
    CHECK_EQ_INT(run(right + 1, my, 1, 0), GL_SHADOW);
    CHECK_EQ_INT(run(c_right + 1, cy, 1, 0), GL_SHADOW);
    CHECK_EQ_INT(run(mx, bottom + 1, 0, 1), GL_SHADOW);
    CHECK_EQ_INT(run(GL_COLUMN_X + 40, c_bottom + 1, 0, 1), GL_SHADOW);
    int p_from = top, c_from = GL_COLOUR_Y;   /* the right strips start GL_SHADOW below each top */
    while (!shade(right + 1, p_from)) p_from++;
    while (!shade(c_right + 1, c_from)) c_from++;
    CHECK_EQ_INT(p_from - top, GL_SHADOW);
    CHECK_EQ_INT(c_from - GL_COLOUR_Y, GL_SHADOW);
    int p_x = left, c_x = GL_COLUMN_X;        /* the bottom strips start GL_SHADOW right of each left */
    while (!shade(p_x, bottom + 1)) p_x++;
    while (!shade(c_x, c_bottom + 1)) c_x++;
    CHECK_EQ_INT(p_x - left, GL_SHADOW);
    CHECK_EQ_INT(c_x - GL_COLUMN_X, GL_SHADOW);
    CHECK(memcmp(at(out, right + 2, my), at(out, c_right + 2, cy), 3) == 0);   /* as dark, on the same ground */
}

static void test_quantise(void)
{
    static uint8_t rgb[GL_W * GL_H * 3], pixels[GL_W * GL_H];
    uint8_t palette[768];
    for (int y = 0; y < GL_H; y++)
        for (int x = 0; x < GL_W; x++) {
            uint8_t *p = rgb + 3 * (y * GL_W + x);
            const int in = x >= GL_PICTURE_X && x < GL_PICTURE_X + GL_PICTURE_W && y >= GL_PICTURE_Y && y < GL_PICTURE_Y + GL_PICTURE_H;
            if (in) p[0] = (uint8_t)(x * 7), p[1] = (uint8_t)(y * 5), p[2] = (uint8_t)(x * y);   /* many colours */
            else p[0] = 90, p[1] = 70, p[2] = (uint8_t)(y < 240 ? 40 : 20);                    /* two */
        }
    enum { COLOURS = 219 };
    CHECK(gallery_quantise(rgb, COLOURS, palette, pixels));
    int ok = 1;
    for (int y = 0; y < GL_H; y++)
        for (int x = 0; x < GL_W; x++) {
            const int in = x >= GL_PICTURE_X && x < GL_PICTURE_X + GL_PICTURE_W && y >= GL_PICTURE_Y && y < GL_PICTURE_Y + GL_PICTURE_H;
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

static void test_hit(void)
{
    int step = 9;
    CHECK_EQ_INT(gallery_hit(GL_COLUMN_X + GL_ARROW_IN + 2, GL_SKIN_Y + 20, &step), GALLERY_SKIN);
    CHECK_EQ_INT(step, -1);
    CHECK_EQ_INT(gallery_hit(GL_COLUMN_X + GL_COLUMN_W - GL_ARROW_IN - 2, GL_SKIN_Y + 20, &step), GALLERY_SKIN);
    CHECK_EQ_INT(step, 1);
    CHECK_EQ_INT(gallery_hit(GL_COLUMN_X + GL_COLUMN_W / 2, GL_COLOUR_Y + 10, &step), GALLERY_COLOUR);
    CHECK_EQ_INT(step, 0);
    CHECK_EQ_INT(gallery_hit(GL_COLUMN_X + GL_COLUMN_W - 3, GL_COLOUR_Y + GL_BOX_H - 3, &step), GALLERY_COLOUR);
    CHECK_EQ_INT(step, 1);
    CHECK_EQ_INT(gallery_hit(200, 200, &step), -1);                            /* the picture */
    CHECK_EQ_INT(gallery_hit(GL_COLUMN_X + 20, GL_SKIN_Y - GL_GAP / 2, &step), -1);   /* between the boxes */
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
    test_edges_and_shadows();
    test_quantise();
    test_lean();
    test_row_put_and_draw();
    test_row_from_the_cd();
    test_hit();
    return TEST_REPORT();
}
