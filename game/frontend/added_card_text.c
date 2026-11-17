/* See added_card_text.h. */
#include "game/frontend/added_card_text.h"

#include "game/sys/text.h"

#include <stdlib.h>
#include <string.h>

static int measure(void *ctx, const char *s)
{
    (void)ctx;
    return text_width_scaled(s, ADDED_CARD_NAME_NUM, ADDED_CARD_NAME_DEN);
}

int added_card_name_letters(const char *name, int rival, AddedCardName *out, uint8_t **mask, char *text, int text_size)
{
    *mask = NULL;
    memset(out, 0, sizeof *out);
    if (text_size > 0) text[0] = '\0';
    if (!text_loaded()) return 0;
    /* the card's width less the margin on each side and the shadow's column */
    const int room = (rival ? ADDED_RIVAL_W : ADDED_CARD_W) - 2 * ADDED_CARD_NAME_X - 1;
    if (!added_card_name_fit(name, room, measure, NULL, text, text_size)) return 0;
    const int w = measure(NULL, text), h = text_mask_rows(text, ADDED_CARD_NAME_NUM, ADDED_CARD_NAME_DEN);
    int top = 0, bottom = 0;
    text_ink_scaled("H", ADDED_CARD_NAME_NUM, ADDED_CARD_NAME_DEN, &top, &bottom);   /* a capital's rows, bottom one past its last */
    if (w <= 0 || h <= 0 || bottom <= 0 || (*mask = malloc((size_t)w * h)) == NULL) return 0;
    if (!text_mask_scaled(text, ADDED_CARD_NAME_NUM, ADDED_CARD_NAME_DEN, *mask, w, h)) {
        free(*mask), *mask = NULL;
        return 0;
    }
    *out = (AddedCardName){ *mask, w, h, bottom - 1 };
    return 1;
}
