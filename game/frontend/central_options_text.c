/* port: the Options rows' text over the screen's own picture, moved out of central_glue.c */
#include "game/frontend/central_split.h"

#include "formats/fsh.h"
#include "game/frontend/config_bgnd.h"
#include "game/frontend/ea_lib.h"
#include "game/sys/text.h"

#include <stdlib.h>

/* port: the Options rows' text (Customize game) soft like the art: the screen's own picture put back under it
 * first, since the row is redrawn over itself when the selection moves */
void central_options_plus_text(void *ctx, int32_t style_id, const char *text, int32_t x, int32_t y)
{
    enum { MARGIN = 2, SHADOW_ROOM = 6 };
    static int32_t bank;
    (void)ctx;
    if (bank == 0) bank = ea_bank_load("option/options.qfs");
    const uint8_t *const bgnd = bank != 0 ? ea_shape(bank, "bgnd") : NULL;
    const int left = x - MARGIN, top = y - MARGIN;
    const int w = text_width(text) + MARGIN + SHADOW_ROOM, h = text_line_height() + MARGIN + SHADOW_ROOM;
    uint8_t *const cover = malloc(CONFIG_BGND_SHAPE_HEADER + (size_t)w * h);
    if (bgnd != NULL && cover != NULL && config_bgnd_shape(bgnd + CONFIG_BGND_SHAPE_HEADER, left, top, w, h, cover) != 0)
        ea_blit_own(cover);
    free(cover);
    /* the styles from the bank's palette: the first draw comes before the fade puts it on screen */
    const uint8_t *const pal = bank != 0 ? ea_shape(bank, "!pal") : NULL;
    uint8_t rgb[768];
    const FshEntry e = { .type = pal != NULL ? pal[0] : FSH_TYPE_BITMAP, .payload = pal != NULL ? pal + 16 : NULL,
                         .payload_size = sizeof rgb };
    if (pal != NULL && fsh_palette_rgb(&e, rgb) == FSH_OK) text_build_styles_from(rgb);
    text_select((TextStyle)style_id);
    text_draw_smooth(text, x, y, 1, 1);
}
