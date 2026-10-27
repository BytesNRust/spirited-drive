/* port: a car's colours changed texel by texel, only in its body textures. A palette-level recolour (every entry of the car's
 * palette that has some colour) reaches whatever else shares the entries: the wheels, the trim and the lights share greys with a
 * grey body, and a body texel whose entry the map does not name keeps its old colour (a speckle). Here the unit is the body
 * texture: its texels' entries are the ones recoloured. An entry that only the body textures use is recoloured where it is; an entry
 * that other textures use too keeps its colour, and the body's texels that showed it are rewritten to a spare entry (one no
 * texture of the bank uses) that has the new colour, equal colours sharing one. When no spare is left the texel takes the nearest
 * colour already given. Only the body's paint family is touched: the entries the main body textures use heavily (not the glass: a
 * neutral entry darker than `floor`, 8-bit lightness) set the family's hue, saturation and lightness range, and any other body texel is
 * recoloured only when its colour is in that family (a coloured body: hue within 30 degrees and a comparable spread; a grey one: low
 * saturation inside the ramp). The lamps, the plate and the badge are not, and are left as they were. The second and later palettes (the CFM's '!xxx') are written the same. */
#ifndef TNFS_CAR_REPAINT_H
#define TNFS_CAR_REPAINT_H

#include "game/race/car_livery.h"

#include <stddef.h>
#include <stdint.h>

enum { REPAINT_NONE = 0, REPAINT_MAIN = 1, REPAINT_END = 2, REPAINT_KEEPS = 4 };

/* the draft's classes of a texel (car_repaint_draft): painted, left as it is without a shine (plate, trim, badge, vents), left as it is
 * with one (glass, lamps) */
enum { REPAINT_DRAFT_PAINT = 0, REPAINT_DRAFT_MATTE = 1, REPAINT_DRAFT_SHINE = 2 };

typedef struct {
    uint8_t *pixels;     /* the texture's texels, palette indices */
    size_t count;
    int body;            /* REPAINT_MAIN for topv, rsid, bott, wing; REPAINT_END for frnt and rear (the lamps, the plate and the badge are
                          * there too); 0 for any other */
    int width;           /* the texture's width in texels, for `keep` */
    int keep[REPAINT_KEEPS][4];   /* x, y, w, h: rectangles of texels left as they were (the plate, the black trim); w 0: unused */
    const uint8_t *mask;  /* NULL, or `count` bytes made by hand (car_mask.h): non-zero is a texel left as it is, 0 is one painted. It replaces the
                           * family test, `keep` and the lamp ring for this texture: every texel it marks as paint is recoloured, the dark ones
                           * and the lamp slots too. Ignored for a texture that is not a body texture. */
} RepaintTexture;

typedef struct {
    RepaintTexture *textures;
    int texture_count;
    uint8_t *palette[2];  /* 768 bytes each: [0] ('!PAL') says which entries are the body's; [1] (or NULL: the CFM's '!xxx', the car's
                           * own colours) has the same entries recoloured by the same rule from its own colours */
    int palettes;
    int type, type2;      /* the palettes' shape type: 0x24, 8 bits a channel, or 0x22, 6 bits; any other type2 leaves [1] alone */
} RepaintBank;

/* The body class of a texture (REPAINT_MAIN, REPAINT_END or REPAINT_NONE) by its 4-letter name and the detail level of its bank (0 high,
 * 1 low). The high bank's body textures: topv, rsid, bott, wing, J000, J001 (main) and frnt, rear (end). The low bank's side (main) and
 * back (end) are body textures too; the high bank has no texture of those names, so they are none at level 0. */
int car_repaint_body_class(const char name[4], int level);

/* The body's colours towards the paint `rgb` (car_paint.h's recolour). Returns the number of body texels changed; -1 for a
 * palette type that is not 0x24 or 0x22 (the bank is left as it is). */
int car_repaint_paint(const RepaintBank *bank, const uint8_t rgb[3], int floor);

/* The texels the rules above would leave as they are, for the drafts of the hand-made masks: `keep`[t] is `count` bytes for the bank's
 * texture t, set to REPAINT_DRAFT_PAINT where the texel is repainted, and where it is not to REPAINT_DRAFT_SHINE for a lamp slot and its ring, a
 * dithered glass texel, a glass colour (a neutral under `floor`) and a very saturated colour (a lamp), else to REPAINT_DRAFT_MATTE (a texture
 * that is not a body texture is all matte). Nothing in the bank changes. Returns the number of texels marked paint; -1 for a palette type that is not 0x24 or 0x22. */
int car_repaint_draft(const RepaintBank *bank, int floor, uint8_t *const keep[]);

/* The PS1 livery's map: a colour (5 bits a channel) in `pairs` becomes the pair's `to`; a paint colour no pair names takes the
 * `to` of the pair whose `from` is nearest in lightness. Returns as above. */
int car_repaint_pairs(const RepaintBank *bank, const CarLiveryPair *pairs, int count, int floor);

#endif
