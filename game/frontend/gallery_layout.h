/* port (dep59, the owner's v3b, notes/dep59-added-gallery-showcase-plan.md sections 5-7): an added car's gallery laid out like the Car
 * Showcase. The page's still part is made once in RGB and quantised: the configuration screens' clean background, the Road & Track band
 * under the title covered by the band's own texture mirrored, the picture in a dark frame with a drop shadow, the technical data's
 * translucent panel, the Skin and Color boxes' shadows and the foot's translucent band. What changes (the words, the boxes' fills and
 * outlines, the arrows) the glue draws over it in the page's palette (added_gallery_glue.c). No drawing and no files here. */
#ifndef TNFS_GALLERY_LAYOUT_H
#define TNFS_GALLERY_LAYOUT_H

#include <stdint.h>

enum {
    GL_W = 640, GL_H = 480,
    /* the name, big, and "Car Showcase" under it */
    GL_NAME_X = 12, GL_NAME_Y = 8, GL_NAME_NUM = 2, GL_NAME_DEN = 1,
    GL_HEADING_X = 8, GL_HEADING_Y = 62, GL_HEADING_NUM = 3, GL_HEADING_DEN = 2,
    /* the band's patch: the band's texture from BAND_FROM_X, mirrored, over 0 .. BAND_W - 1 */
    GL_BAND_W = 285, GL_BAND_H = 56, GL_BAND_FROM_X = 300,
    /* the picture (320x200) and its frame (FRAME px outside it); SHADOW px down and right, the depth of every shadow on the page */
    GL_PICTURE_X = 24, GL_PICTURE_Y = 100, GL_PICTURE_W = 320, GL_PICTURE_H = 200, GL_FRAME = 2, GL_SHADOW = 6,
    /* the picture frame's outside (the owner's: what the other elements line up with), ends included */
    GL_OUTER_X0 = GL_PICTURE_X - GL_FRAME, GL_OUTER_Y0 = GL_PICTURE_Y - GL_FRAME,                    /* 22, 98 */
    GL_OUTER_Y1 = GL_PICTURE_Y + GL_PICTURE_H - 1 + GL_FRAME,                                      /* 301 */
    /* the right column, its top and foot level with the picture frame's outside: the technical data, then Skin and Color */
    GL_COLUMN_X = 364, GL_COLUMN_W = 252,
    GL_FIGURES_Y = GL_OUTER_Y0, GL_FIGURES_ROWS = 3, GL_FIGURES_ROW = 18, GL_FIGURES_PAD = 7,
    GL_FIGURES_H = 2 * GL_FIGURES_PAD + GL_FIGURES_ROWS * GL_FIGURES_ROW,   /* 68: 98 .. 165 */
    GL_GAP = 16,
    GL_BOX_H = (GL_OUTER_Y1 + 1 - (GL_FIGURES_Y + GL_FIGURES_H) - 2 * GL_GAP) / 2,           /* 52 */
    GL_SKIN_Y = GL_FIGURES_Y + GL_FIGURES_H + GL_GAP,                                       /* 182 */
    GL_COLOUR_Y = GL_SKIN_Y + GL_BOX_H + GL_GAP,                                            /* 250: ends at 301 */
    GL_ARROW_W = 9, GL_ARROW_H = 14, GL_ARROW_IN = 8,
    /* the foot band: the description, the author at its left and the version at its right */
    GL_FOOT_X0 = GL_OUTER_X0, GL_FOOT_X1 = GL_COLUMN_X + GL_COLUMN_W - 1, GL_FOOT_Y0 = 326, GL_FOOT_Y1 = 420, GL_FOOT_PAD = 10, GL_FOOT_LINES = 3,
    /* the bottom row: DONE at the left, "< NEXT CAR >" in the middle */
    GL_DONE_X = 8, GL_BOTTOM_Y = 442, GL_BOTTOM_H = 30, GL_NEXT_GAP = 8,
    /* the palette: the picture's entries first, then the still part's */
    GL_PICTURE_COLOURS = 160
};

/* the RGB of the panels and boxes */
#define GL_PANEL_RGB 0x0a0704u    /* the translucent panels' tint */
#define GL_PANEL_EDGE_RGB 0x7a654au
#define GL_SKIN_RGB 0x3a2c1eu     /* the Skin box's fill */
enum { GL_SHADOW_ALPHA = 150, GL_FIGURES_ALPHA = 165, GL_FOOT_ALPHA = 120 };   /* of 255 */

/* the still part in RGB (GL_W x GL_H x 3) from the background `bgnd` and the picture `picture` (GL_PICTURE_W x GL_PICTURE_H x 3) */
void gallery_compose(const uint8_t *bgnd, const uint8_t *picture, uint8_t *out);

/* `rgb` (the still part) into `pixels` (GL_W x GL_H): the picture's box quantised to entries 0 .. GL_PICTURE_COLOURS - 1, the rest to
 * GL_PICTURE_COLOURS .. colours - 1; their colours into `palette`. 1 on success, 0 when there was no memory (`pixels` then all 0) */
int gallery_quantise(const uint8_t *rgb, int colours, uint8_t palette[768], uint8_t *pixels);

/* `mask` (w x h) leant right by num/den of a pixel a row up from its foot, into `out` (ow x h, ow >= w + h * num / den + 1); 0 elsewhere */
void gallery_lean(const uint8_t *mask, int w, int h, int num, int den, uint8_t *out, int ow);

/* where the bottom row's "< NEXT CAR >" goes for a text `text_w` wide: the left arrow's x, the text's x and the right arrow's x, the whole
 * centred on the screen */
void gallery_next_place(int text_w, int *left_x, int *text_x, int *right_x);

/* what a click at (x, y) hits: GALLERY_SKIN / GALLERY_COLOUR (with *step -1 on its left arrow, 1 on its right one, 0 between),
 * GALLERY_PREVIOUS, GALLERY_NEXT or GALLERY_DONE (step 0); -1 for nothing. `next_text_w` as for gallery_next_place, `done_w` DONE's width */
int gallery_hit(int x, int y, int next_text_w, int done_w, int *step);

#endif
