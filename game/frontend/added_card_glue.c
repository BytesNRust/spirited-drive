/* port (dep59 step 3): Control Central's load_added_card (central.h): the piece's picture read from its folder (CarPiece.dir) and made a
 * bank (added_card.h). Made at each change of car and freed with the screen's art: nothing is kept that a change of mods could leave
 * stale. */
#include "game/frontend/added_card.h"
#include "game/frontend/added_card_text.h"
#include "game/frontend/central_glue_private.h"
#include "game/frontend/ea_lib.h"
#include "game/platform/platform.h"
#include "game/race/car_registry.h"
#include "game/sys/port_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { POINTER_COLOUR = 0xfe, SEE_THROUGH = 0xff };

/* `name` in the piece's folder, read as a picture; 0 when there is none or it cannot be read */
static int picture_of(const CarPiece *p, const char *name, PngImage *img)
{
    char path[CAR_PIECE_DIR_MAX + 16];
    snprintf(path, sizeof path, "%s/%s", p->dir, name);
    size_t size = 0;
    uint8_t *data = platform_read_file(path, &size);
    if (data == NULL) return 0;
    const int read = png_read(data, size, img) == PNG_OK;
    platform_free_file(data);
    if (!read) port_log("car piece: %s: %s is not a picture the game can read\n", p->id, name);
    return read;
}

int32_t cg_load_added_card(void *ctx, int car, int rival, const uint8_t palette[768])
{
    (void)ctx;
    const CarPiece *p = car_registry_piece(car);
    if (p == NULL || p->dir[0] == '\0') return 0;
    PngImage img;
    int from_card = 0;
    if (!rival || !picture_of(p, "rival.png", &img)) {
        if (!picture_of(p, "card.png", &img)) {
            port_log("car piece: %s has no card.png: no card is drawn\n", p->id);
            return 0;
        }
        from_card = rival;
    }
    uint8_t usable[256];
    memset(usable, 1, sizeof usable);
    usable[POINTER_COLOUR] = usable[SEE_THROUGH] = 0;   /* the screen's own */
    char why[160], text[MOD_INI_NAME_MAX + 4];
    size_t size = 0;
    AddedCardName name;   /* its name over the picture, as the CD's cards carry theirs, unless the picture shows its own */
    uint8_t *mask = NULL;
    const int named = !p->card_name_off && added_card_name_letters(p->name, rival, &name, &mask, text, (int)sizeof text);
    uint8_t *bank = added_card_bank_named(&img, rival, from_card, named ? &name : NULL, palette, usable, &size, why, sizeof why);
    png_free(img.pixels);
    free(mask);
    if (bank == NULL) {
        port_log("car piece: %s: %s\n", p->id, why);
        return 0;
    }
    return ea_bank_put(bank, size, rival ? "an added car's rival card" : "an added car's card");
}
