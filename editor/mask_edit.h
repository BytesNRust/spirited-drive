/* The edit of one car mask (dep59 car editor E3): the mask's texels as classes (core/formats/car_mask_codec.h: 0 paint, 128 glass, 192 body shine, 224 fixed black,
 * 240 brake, 244 side light, 248 headlight, 252 reverse, 255 matte), the texture they belong to (kept to look at and to fill by), the pencil, the fill and the
 * eyedropper, undo and redo, and Save, which writes the mask back as the repo's 8-bit grey PNG: the same size, and every texel outside the edit as it was. A mask is
 * kept at its own size; the game scales a mask that is not its texture's (nearest neighbour, car_mask_codec.h), so the texture is looked up by the same scaling.
 * Pure C, no window. */
#ifndef TNFS_TRACK_EDITOR_MASK_EDIT_H
#define TNFS_TRACK_EDITOR_MASK_EDIT_H

#include <stddef.h>
#include <stdint.h>

enum { ME_PATH = 4096, ME_UNDO_MAX = 256, ME_BRUSH_MIN = 1, ME_BRUSH_MAX = 8, ME_CLASSES = 9 };

typedef enum { ME_FILL_CLASS = 0, ME_FILL_TEXTURE } MaskFillMode;

/* one class of the legend: its value, the name the window gives it, and the colour it is drawn in over the texture */
typedef struct {
    uint8_t value;
    const char *name;
    const char *tag;     /* a short name for a list: "glass", "side light" */
    uint8_t rgb[3];
} MaskClass;

typedef struct {
    uint32_t *idx;     /* the texels this step changed */
    uint8_t *old;      /* what each was */
    size_t count, cap;
    uint8_t cls;       /* what they became */
} MaskStep;

typedef struct {
    char path[ME_PATH];
    int width, height;          /* the mask's own size */
    uint8_t *now, *saved;       /* width x height classes: on screen, and as the file has them */
    int tex_width, tex_height;  /* the texture's size; 0 when there is none to show */
    uint8_t *tex;               /* tex_width x tex_height x 3 (RGB), or NULL */
    MaskStep *undo, *redo;      /* malloc'd stacks of ME_UNDO_MAX; the oldest undo falls off */
    int undo_count, redo_count;
    int stroke;                 /* a stroke is open: pencil calls add to the top step */
    int modified_known, modified;
    unsigned version;           /* counts every change of `now` (an edit, an undo, a redo): what a picture made from the mask compares with */
} MaskEdit;

/* the nine classes of the legend, in the order the window lists them; ME_CLASSES of them */
const MaskClass *mask_edit_class(int index);
/* the index into the legend of a texel value; the unknown non-zero values are matte, as the game reads them */
int mask_edit_class_index(uint8_t value);

/* Reads the mask PNG at `path` as it is (its own size), with the texture it belongs to: `tex_rgb` is tex_width x tex_height x 3, copied (NULL for none).
 * 1, or 0 with `why`. */
int mask_edit_open(MaskEdit *e, const char *path, int tex_width, int tex_height, const uint8_t *tex_rgb, char *why, size_t why_cap);
void mask_edit_free(MaskEdit *e);

/* 1 with a sentence for the person when the mask's size is not the texture's (both sizes, and that the game scales the mask); 0 when they match or there is no texture */
int mask_edit_size_note(const MaskEdit *e, char *out, size_t cap);

/* the texture's colour under mask texel (x, y), scaled as the game scales the mask; 0 when there is no texture or the texel is outside */
int mask_edit_texture_at(const MaskEdit *e, int x, int y, uint8_t rgb[3]);
/* The texel of a width x height mask under a point of the screen: the mask is drawn from (origin_x, origin_y) with `scale` screen pixels a texel. 1 with (*x, *y) when the point is on a texel;
 * 0 with both -1 otherwise, including a point that is not a number or too far to be one (a missing mouse position is -FLT_MAX) or a scale that is not above 0: nothing is cast out of range. */
int mask_edit_texel_under(float px, float py, float origin_x, float origin_y, float scale, int width, int height, int *x, int *y);

/* the class of texel (x, y), or -1 outside */
int mask_edit_class_at(const MaskEdit *e, int x, int y);

/* Pencil: a square of `size` texels (ME_BRUSH_MIN..ME_BRUSH_MAX) centred on (x, y), set to class `cls`. Texels outside the mask are skipped. Calls between
 * mask_edit_stroke_begin and _end are one step to undo; a call outside a stroke is its own step. A line joins two points with the same brush. Return how many texels changed. */
void mask_edit_stroke_begin(MaskEdit *e);
void mask_edit_stroke_end(MaskEdit *e);
int mask_edit_pencil(MaskEdit *e, int x, int y, int size, uint8_t cls);
int mask_edit_line(MaskEdit *e, int x0, int y0, int x1, int y1, int size, uint8_t cls);

/* Fill: the connected texels (4 neighbours) from (x, y) that have the same class (ME_FILL_CLASS) or the same texture colour (ME_FILL_TEXTURE, which needs a
 * texture) become `cls`. One step. Returns how many texels changed. */
int mask_edit_fill(MaskEdit *e, int x, int y, uint8_t cls, MaskFillMode mode);

int mask_edit_undo(MaskEdit *e);
int mask_edit_redo(MaskEdit *e);
int mask_edit_can_undo(const MaskEdit *e);
int mask_edit_can_redo(const MaskEdit *e);

/* 1 when what is on screen is not what the file has (undoing back to it is not modified) */
int mask_edit_modified(MaskEdit *e);

/* The picture of the mask for the screen: width x height RGBA, the texture under it (grey 40 where there is none) and each class that is `on` tinted with its legend colour at
 * `opacity` (0..1) when `show`. `rgba` has room for width x height x 4. */
void mask_edit_compose(const MaskEdit *e, const uint8_t on[ME_CLASSES], int show, float opacity, uint8_t *rgba);

/* Writes the file when something changed: a grey 8-bit PNG of the same size, through a temporary file renamed over it. Nothing changed writes nothing (the file's bytes
 * stay). 1 when written or nothing to write; 0 with `why` and the file as it was. */
int mask_edit_save(MaskEdit *e, char *why, size_t why_cap);

/* The PNG Save would write, for a test: 1 with `*out` (free with free()) and `*len`. */
int mask_edit_render(const MaskEdit *e, uint8_t **out, size_t *len);

#endif
