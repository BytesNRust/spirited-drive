/* The painted preview of a car's textures (dep59 car editor E1): each texture of the car file as the race has it after the car's masks and paint went through it: the
 * fixed black, then the catalogue paint, by the game's own code (formats/car_livery_pass.h, the pass the game's livery code runs) on a copy of the car file, with the project's masks in front of the game's. The bank's palette is taken to 6 bits a channel as the race loads it. The track's light and the screen palette's nearest-colour
 * snapping are not applied: the preview counts the body's distinct colours and says how many the car's entries in the race palette cannot hold. Pure C, no window. */
#ifndef TNFS_TRACK_EDITOR_CAR_PREVIEW_H
#define TNFS_TRACK_EDITOR_CAR_PREVIEW_H

#include "formats/car_livery_pass.h"
#include "mask_edit.h"
#include "mask_tex.h"

#include <stddef.h>
#include <stdint.h>

enum {
    PREVIEW_OWN_ENTRIES = 0x17,     /* the palette entries a car owns in the race (race_palette.c SLOT_COUNT) */
    PREVIEW_SHARED_ENTRIES = 0x48,  /* the shared car palette of a race with several cars */
    PREVIEW_NOTE = 400
};

typedef struct {
    MaskTexInfo info;
    uint8_t *rgba;      /* info.width x info.height x 4 */
    uint8_t *mask;      /* info.width x info.height CAR_MASK_* classes of this texture as the race reads them (the unsaved edit in place of its file); NULL when it has no mask */
    uint8_t *index;     /* info.width x info.height palette indices, as the paint left them (what a night or light view maps itself) */
    int body;           /* the texture is one of the race's body textures (the paint reaches it) */
} PreviewTex;

typedef struct {
    PreviewTex tex[MASK_TEX_LIST];
    int count;
    uint8_t palette[2][768];   /* each bank's '!PAL' as the race takes it, 8 bits a channel (6 bits scaled): [0] the high detail bank, [1] the low one; zero where the car has no such bank */
    int body_colours;   /* the distinct colours (6 bits a channel) the body textures show after the paint */
    int painted;        /* the texels the paint changed (0 with no paint) */
    int blacked;        /* the texels the fixed black changed */
    int special;        /* texels that name a screen entry of the race rather than a colour and are drawn as a pattern (0xfa..0xfd, 0xff; not the brake lamp's 0xfe) */
    char note[PREVIEW_NOTE];   /* what the person is told about the colours: fits, or how many are shown as their nearest */
    int lamp_row;       /* the brake lamp's colours (formats/car_brake_lamp.h): the original's own row, the standard one for an added car; entry 0xfe of `palette` is its unlit colour */
} CarPreview;

typedef struct {
    const char *cfm;        /* the car's file */
    const char *stem;       /* the stem the game looks the car's masks and black up by ("P911", "Wagon": the masks folder's name) */
    const char *masks_dir;  /* the project's car_masks/<folder>/, NULL for none */
    const char *fallback_dir;   /* the game's own masks of the car (data/car_masks/<folder>/), read for a texture the project has none for: the race's lookup of an original car; NULL for none (an added car has none) */
    const MaskEdit *edit;   /* the mask being edited, with changes not saved: it is used in place of its file; NULL for none */
    const uint8_t *paint;   /* the paint's RGB (8 bits), NULL for the car as it is */
    int added;              /* a car of the player's own (not one of the game's): its masks are read as a skin's folder, its black is the catalogue's usual one (the race's added-car path) */
} PreviewIn;

/* The car file as the race has it after the game's own fixed black and paint: a malloc'd copy of the CFM (free() it), the same layout as the file, high and low banks painted,
 * the '!PAL' of each bank recoloured in place (its own type, 0x24 8 bits or 0x22 6 bits: not taken to 6 bits). `blacked` and `painted` (each may be NULL) get the texels changed.
 * 1, or 0 with `why`.  */
int car_preview_painted_cfm(const PreviewIn *in, uint8_t **cfm, size_t *len, int *blacked, int *painted, char *why, size_t why_cap);

/* The draft of the car's masks (formats/car_livery_pass.h car_livery_draft) with the masks the paint is made with (the project's, the edit, the game's own):
 * `fn` gets each body texture's REPAINT_DRAFT_* classes, from the file as it is (the race drafts before the black and the paint). The texels marked
 * paint, or -1 with `why` when the file cannot be read. The Model window shines a car by it (raster/shine_class.h). */
int car_preview_draft(const PreviewIn *in, CarLiveryDraftFn fn, void *user, char *why, size_t why_cap);

/* Makes the preview. 1, or 0 with `why`.  */
int car_preview_make(const PreviewIn *in, CarPreview *out, char *why, size_t why_cap);
void car_preview_free(CarPreview *p);

/* Writes entry 0xfe of the 8-bit palette `pal` (one of p->palette's form) with the brake lamp's colour, lit or not: the colour the race paints those texels in by day */
void car_preview_lamp_entry(const CarPreview *p, int lit, uint8_t pal[768]);

#endif
