/* port: the pass over a car file's two banks that the paint and the fixed black make (the game's livery code and the car editor share it): for each bank, its textures as the repaint code
 * (formats/car_repaint.h) reads them, the number plates kept as they were, each texture given its hand-made mask by the caller's `mask` function (car_mask.h: where a mask is read from is the
 * caller's: the game's data folders, a project's, an edit in memory), and the paint, or the fixed black, run on the bank. The game's data_path and log stay out of the core. */
#ifndef TNFS_CAR_LIVERY_PASS_H
#define TNFS_CAR_LIVERY_PASS_H

#include "formats/car_bank_walk.h"
#include "formats/car_repaint.h"

#include <stddef.h>
#include <stdint.h>

/* The mask of the texture `name` (4 letters) of a bank (`level` 0 high, 1 low detail) that is width x height texels: malloc'd (the pass frees it), CAR_MASK_* a texel, nearest-scaled to the
 * texture; NULL for a texture with none. */
typedef uint8_t *(*CarLiveryMaskFn)(void *user, const char name[4], int level, int width, int height);
/* Called with each bank that has a palette: returns the number of changes, added to the pass's total (a negative one counts for none) */
typedef int (*CarLiveryBankFn)(const RepaintBank *bank, const CarBankMeta *meta, void *ctx);

typedef struct {
    const char *upper;       /* the car's stem in upper case ("P911", "TSUPRA"): the plates and the glass floor are by it */
    int skin;                /* an added car's own masks: every texture is given its mask, and one the race does not paint by its name is the body where its mask paints a texel; an original car's masks keep their names' classes */
    int every_texture;       /* every texture is given its mask, not only the body's (the lamps' walk) */
    CarLiveryMaskFn mask;    /* where a texture's mask is read from; NULL: no masks */
    void *mask_user;
} CarLiveryPass;

/* Runs `bank` on each bank of the CFM `cfm` (`size` bytes, edited in place by it), textures made and given their masks as above. Returns the sum of the results; 0 for a file that is not a CFM. */
int car_livery_walk(uint8_t *cfm, size_t size, const CarLiveryPass *pass, CarLiveryBankFn bank, void *ctx);

/* The lightness (8 bits) under which a neutral colour of the car (stem in upper case) is glass or shadow, not paint */
int car_livery_glass_floor(const char *upper);

/* The pass with the fixed black (car_repaint_fixed_black) and with a catalogue paint (car_repaint_paint): the texels changed */
int car_livery_black(uint8_t *cfm, size_t size, const CarLiveryPass *pass, const uint8_t rgb[3]);
int car_livery_paint(uint8_t *cfm, size_t size, const CarLiveryPass *pass, const uint8_t rgb[3]);

#endif
