#include "game/frontend/save_box.h"

#include <stdio.h>
#include <string.h>

/* 0042a680 */
void save_box_show(char *name, int german, const MessageBannerInputs *in, SaveBoxState *s, const SaveBoxOps *ops)
{
    enum { W = 0x190, H = 0x64, X = 0x78, Y = 0xbe, DARK = 0x9000, RED = 0xff0000, LENGTH = 8, SOUND = 0x48,
           NAME = 9, CONTROLS = 0x5f36, CONTROLS_SIZE = 0x1a, COPY = 0xac, SAVED_SHOWN = 100 };
    const MessageBannerOps *d = ops->draw;
    uint8_t shade[MESSAGE_BANNER_SHADE];
    int32_t saved = 0;
    const int32_t under = d->picture_new(d->ctx, W, H, 0);
    const int32_t box = d->picture_new(d->ctx, W, H, 0);
    d->shade_table(d->ctx, DARK, DARK, DARK, in->palette, shade);
    d->select(d->ctx, in->screen);
    d->grab(d->ctx, d->picture_shape(d->ctx, under), X, Y);
    d->select(d->ctx, box);
    d->blit(d->ctx, d->picture_shape(d->ctx, under), 0, 0);
    d->shade_set(d->ctx, shade);
    const int32_t corners[8] = { 0, 0, W - 1, 0, W - 1, H - 1, 0, H - 1 };
    d->shade_polygon(d->ctx, 4, corners);
    d->frame(d->ctx, 2, 2, W - 3, H - 3, d->colour(d->ctx, RED));
    d->frame(d->ctx, 3, 3, W - 4, H - 4, d->colour(d->ctx, RED));
    d->text_styles(d->ctx);
    d->text_select(d->ctx, 2);
    d->text_centred(d->ctx, german ? "Spiel speichern als" : "Save game as", 0xc8, 0xa);
    d->text_select(d->ctx, 1);
    d->select(d->ctx, in->screen);
    d->blit(d->ctx, d->picture_shape(d->ctx, box), X, Y);
    d->select_default(d->ctx);
    d->blit_own(d->ctx, d->picture_shape(d->ctx, in->screen));
    d->flip(d->ctx);
    const int32_t taken = ops->text_field(ops->ctx, name, LENGTH, 0x140, 0xfa, 0);
    d->select_default(d->ctx);
    d->select(d->ctx, in->screen);
    d->blit(d->ctx, d->picture_shape(d->ctx, under), X, Y);
    d->select_default(d->ctx);
    d->blit_own(d->ctx, d->picture_shape(d->ctx, in->screen));
    d->flip(d->ctx);
    if ((int16_t)taken) {
        char path[SAVE_BOX_PATH];
        const size_t n = strlen(name);
        memcpy(s->config + NAME, name, n + 1);
        memcpy(s->game + (0x0052b298 - 0x0052ad44), s->config + CONTROLS, CONTROLS_SIZE);
        memcpy(s->game + (0x0052b1ec - 0x0052ad44), s->config + NAME, COPY);
        s->game[0x0052b2b2 - 0x0052ad44] = (uint8_t)s->lost_vegas;
        s->game[0x0052b2b2 - 0x0052ad44 + 1] = (uint8_t)((uint16_t)s->lost_vegas >> 8);
        snprintf(path, sizeof path, "%s%s.SAV", s->dir, name);
        if (ops->file_exists(ops->ctx, path)) {
            if (ops->ask(ops->ctx, german ? "Spiel ersetzen ?" : "Replace game ?")) {
                ops->remove_file(ops->ctx, path);
                saved = ops->save(ops->ctx, path);
            }
        } else {
            saved = ops->save(ops->ctx, path);
            ops->sound(ops->ctx, SOUND);
        }
        if ((int16_t)saved) ops->banner_wait(ops->ctx, german ? "Spiel gespeichert" : "Game saved", SAVED_SHOWN);
    } else {
        ops->sound(ops->ctx, SOUND);
    }
    d->picture_free(d->ctx, under);
    d->picture_free(d->ctx, box);
    ops->pointer_redraw(ops->ctx, 0, 1);
}
