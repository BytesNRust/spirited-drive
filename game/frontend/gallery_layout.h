/* port (dep59, PR B, notes/dep59-showcase-redesign-steps.md): an added car's gallery laid out like the Car Showcase pages of the CD: its
 * tiles at the CD pages' own rectangles (Color's place at the top left, General, Performance, Video, History; Mechanical's left empty), each in
 * a dark frame with a drop shadow. The page's still part is made once in RGB and quantised: the configuration screens' clean background (the
 * sepia of the page), the Road & Track band under the title covered by the band's own texture mirrored, the two photos, the translucent
 * panels of General and History, the Color tile's fill and the foot's band. What changes (the words, the arrows, the lit frame) the glue
 * draws over it in the page's palette (added_gallery_glue.c). No drawing and no files here. */
#ifndef TNFS_GALLERY_LAYOUT_H
#define TNFS_GALLERY_LAYOUT_H

#include <stddef.h>
#include <stdint.h>

enum {
    GL_W = 640, GL_H = 480,
    /* the name, big, and "Car Showcase" under it */
    GL_NAME_X = 12, GL_NAME_Y = 8, GL_NAME_NUM = 2, GL_NAME_DEN = 1,
    GL_HEADING_X = 8, GL_HEADING_Y = 62, GL_HEADING_NUM = 3, GL_HEADING_DEN = 2,
    /* the band's patch: the band's texture from BAND_FROM_X, mirrored, over 0 .. BAND_W - 1 */
    GL_BAND_W = 285, GL_BAND_H = 56, GL_BAND_FROM_X = 300, GL_BLUR_R = 6,   /* the strip over the title line is blurred by a box of 2 * GL_BLUR_R + 1 */
    /* FRAME px of black outside every tile; SHADOW px down and right, the depth of every shadow on the page */
    GL_FRAME = 2, GL_SHADOW = 6,
    /* the tiles (the CD pages' rectangles, the picture inside the frame): Performance's big photo, General's figures, Video's small photo,
     * History's text, and the place of Color (the CD's Color tile) */
    GL_PERF_X = 306, GL_PERF_Y = 77, GL_PERF_W = 225, GL_PERF_H = 127,
    GL_GEN_X = 133, GL_GEN_Y = 173, GL_GEN_W = 163, GL_GEN_H = 109,
    GL_MECH_X = 82, GL_MECH_Y = 326, GL_MECH_W = 254, GL_MECH_H = 87,   /* Mechanical's (owner 2026-11-19: power, weight and their ratio over General's panel); the CD's page has its picture there */
    GL_VID_X = 306, GL_VID_Y = 214, GL_VID_W = 139, GL_VID_H = 108,
    GL_HIST_X = 455, GL_HIST_Y = 245, GL_HIST_W = 161, GL_HIST_H = 122,
    GL_COL_X = 57, GL_COL_Y = 100, GL_COL_W = 154, GL_COL_H = 63,
    GL_ARROW_W = 9, GL_ARROW_H = 14, GL_ARROW_IN = 8,
    /* the SETUP panel when it is open, over the tile and General's top (the mock-up: x 57..299, y 100..269); its rows from GL_SETUP_ROW0 down by
     * GL_SETUP_STEP, the value column from GL_SETUP_VALUE_X, GL_SETUP_VALUE_W wide (its arrows at its two ends) */
    GL_SETUP_X = 40, GL_SETUP_Y = 100, GL_SETUP_W = 380, GL_SETUP_H = 170, GL_SETUP_ROW0 = 44, GL_SETUP_STEP = 24,
    GL_SETUP_VALUE_X = 168, GL_SETUP_VALUE_W = 196, GL_SETUP_PREVIEW_W = 20, GL_SETUP_PREVIEW_H = 12,   /* the arrows' zone: Color's and Skin's arrows at its two ends, a switch's value at its right end; the label from x + GL_SETUP_LABEL_X */
    GL_SETUP_TITLE_NUM = 5, GL_SETUP_TITLE_DEN = 4,   /* the panel's title "CAR SETUP", red: the size of the CD page's baked "Car Showcase" (owner 2026-11-18), on the CD's pages and the added one's alike */
    GL_SETUP_CHOICE_X = 80,   /* Color's and Skin's row (owner 2026-11-19): the left arrow at this x in the panel, past the label, and the right arrow's right edge at the panel's width - 16 (a switch's value ends there too) */
    GL_SETUP_CHOICE_GAP = 6,  /* the preview box's own gap to the left arrow, and the name's to the box and to the right arrow */
    GL_SETUP_INK_MID = 10,   /* a row's capitals' middle below the row's y ((5 + 16) / 2 in the main face at 7/8): a click's row band is centred there, on what the player sees (owner 2026-11-19) */
    GL_SETUP_LABEL_X = 14, GL_SETUP_FONT_NUM = 7, GL_SETUP_FONT_DEN = 8,   /* the rows in the main face at 7/8, as Options' */
    GALLERY_SETUP_ROWS = 5,
    GL_ARROW_ZONE = 30,   /* the width of the big photo's left and right zones, the arrows' places for a click */
    /* the bottom row (DONE, the arrows, "Next car") as the CD showcase pages have it, cut from their art (GalleryRow): rows GL_ROW_Y ..
     * + GL_ROW_H - 1, columns 0 .. GL_ROW_W - 1 (past them the 512TR's art has its swirl); their boxes are the showcase's (showcase.h) */
    GL_ROW_Y = 436, GL_ROW_H = 30, GL_ROW_W = 420, GL_ROW_SHADOW = 2, GL_ROW_COLOURS = 64,
    GL_ROW_PLAIN = 0, GL_ROW_DONE = 1, GL_ROW_PREVIOUS = 2, GL_ROW_NEXT = 3, GL_ROW_LOOKS = 4,
    /* the palette: the photos' entries first, then the still part's */
    GL_PICTURE_COLOURS = 160
};

/* the RGB of the panels and boxes */
#define GL_PANEL_RGB 0x0a0704u    /* the translucent panels' tint */
#define GL_PANEL_EDGE_RGB 0x7a654au
#define GL_SKIN_RGB 0x3a2c1eu     /* the Skin box's fill */
enum { GL_SHADOW_ALPHA = 150, GL_FIGURES_ALPHA = 165, GL_HISTORY_ALPHA = 200 };   /* of 255 */
#define GL_HISTORY_RGB 0x2a1c0eu  /* History's panel: the sepia of the page, darker */

/* the still part in RGB (GL_W x GL_H x 3) from the background `bgnd` and the two photos: `big` (GL_PERF_W x GL_PERF_H x 3) and `small`
 * (GL_VID_W x GL_VID_H x 3) */
void gallery_compose(const uint8_t *bgnd, const uint8_t *big, const uint8_t *small, uint8_t *out);

/* `rgb` (the still part) into `pixels` (GL_W x GL_H): the two photos' boxes together quantised to entries 0 .. GL_PICTURE_COLOURS - 1, the rest to
 * GL_PICTURE_COLOURS .. colours - 1; their colours into `palette`. 1 on success, 0 when there was no memory (`pixels` then all 0) */
int gallery_quantise(const uint8_t *rgb, int colours, uint8_t palette[768], uint8_t *pixels);

/* `mask` (w x h) leant right by num/den of a pixel a row up from its foot, into `out` (ow x h, ow >= w + h * num / den + 1); 0 elsewhere */
void gallery_lean(const uint8_t *mask, int w, int h, int num, int den, uint8_t *out, int ow);

/* what a click at (x, y) hits: GALLERY_SKIN / GALLERY_COLOUR (the Color tile's two rows, with *step -1 on a left arrow, 1 on a right one, 0
 * between), GALLERY_PHOTOS (the big photo: step -1 in its left zone, 1 in its right one, 0 between), GALLERY_HISTORY, GALLERY_PREVIOUS,
 * GALLERY_NEXT or GALLERY_DONE (step 0); -1 for nothing */
int gallery_hit(int x, int y, int *step);

/* port (owner 2026-11-19): where a Color or Skin row's parts are on the screen: the arrows' left x (`left`, `right`), the preview box's left x, and the
 * span [name_x0, name_x1) the name is centred in (the gap between the box and the right arrow, not between the two arrows) */
typedef struct {
    int left, right, preview, name_x0, name_x1;
} SetupChoice;
void gallery_setup_choice(SetupChoice *c);
/* what a click at column x on a Color or Skin row means: -1 in the left half of the arrows' zone, 1 in the right half, 0 left of it (the label: the row alone) */
int gallery_setup_choice_step(int x);
/* the panel's row (0 ..) whose band the pointer row `y` is in: each band is GL_SETUP_STEP tall and centred on the row's text; -1 above the first */
int gallery_setup_row_at(int y);

/* what a click at (x, y) hits in the open SETUP panel: the row (SETUP_*) with *step -1 on the left half of the value column, 1 on the right half,
 * 0 on the label (the row alone); -1 outside the panel or between rows */
int gallery_setup_hit(int x, int y, int *step);

/* port (owner 2026-11-17): the bottom row of the CD showcase pages, cut from a car's bank (FRONTEND/SHOW/512TR.QFS: its background with
 * the unlit DONE, arrows and "Next car", and the lit shapes): the letters' and arrows' own pixels ("faces": light, cream or yellow) in
 * four looks, plain and with DONE, Previous or Next lit; the page's own picture shows elsewhere, so the art's red is never drawn */
typedef struct {
    uint8_t face[GL_ROW_LOOKS][GL_ROW_H * GL_ROW_W];   /* 0 none; k: the colour rgb[k - 1] */
    uint8_t rgb[GL_ROW_COLOURS][3];
    int count[GL_ROW_COLOURS];                         /* the pixels of each colour over the four looks */
    int colours;
} GalleryRow;

/* 1 when a colour of the art is a letter's or an arrow's (cream, yellow and their edges, not the red, pink or dark background) */
int gallery_row_is_face(const uint8_t rgb[3]);
void gallery_row_clear(GalleryRow *row);
/* a picture of the bank (w x h, its top left at (x0, y0), entries of the palette `pal` 8 bits a channel) into the look: inside its
 * rectangle on the row, its faces and nothing else; past GL_ROW_COLOURS colours, the nearest of those kept */
void gallery_row_put(GalleryRow *row, int look, const uint8_t *pixels, int w, int h, int x0, int y0, const uint8_t pal[768]);
/* the four looks from a bank's file (QFS or SHPI): its background (DONE, the arrows and "Next car" unlit), and over it in turn the lit
 * DONE, Previous and Next (donl, prel, nexl); 1 when it had them all and the palette */
int gallery_row_cut(GalleryRow *row, const uint8_t *file, size_t size);
/* the look for the page's item (GALLERY_*) */
int gallery_row_look(int item);
/* the look into `to` (tw x th): a shadow in entry `shadow` GL_ROW_SHADOW px down and right of the faces, then the faces, colour k in
 * entry map[k - 1] */
void gallery_row_draw(const GalleryRow *row, int look, uint8_t *to, int tw, int th, const uint8_t map[GL_ROW_COLOURS], int shadow);

#endif
