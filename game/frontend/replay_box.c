#include "game/frontend/replay_box.h"

#include "game/frontend/file_list.h"

#include <stdio.h>
#include <string.h>

/* 0042a9f8 */
void replay_box_show(char *name, int german, const MessageBannerInputs *in, uint8_t *config, const char *dir,
                     const SaveBoxOps *ops)
{
    enum { W = 0x1e0, H = 0x64, X = 0x50, Y = 0xbe, DARK = 0x9000, RED = 0xff0000, LENGTH = 8, SOUND = 0x48,
           CUT = 0xf, REPLAY_NAME = 0x3b, SAVED_SHOWN = 100 };
    /* 004aef38.. */
    static const char *const SYSTEM[] = { "REPLAY", "REPLAY1", "REPLAY2", "REPLAY3" };
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
    d->text_centred(d->ctx, german ? "Wiederholung speichern als" : "Save replay as", 0xf0, 0xa);
    d->text_select(d->ctx, 1);
    d->select(d->ctx, in->screen);
    d->blit(d->ctx, d->picture_shape(d->ctx, box), X, Y);
    d->select_default(d->ctx);
    d->blit_own(d->ctx, d->picture_shape(d->ctx, in->screen));
    d->flip(d->ctx);
    int i = 0;
    while (name[i] != '.' && i < CUT) i++;
    name[i] = 0;
    const int32_t taken = ops->text_field(ops->ctx, name, LENGTH, 0x140, 0xfa, 0);
    d->select(d->ctx, in->screen);
    d->blit(d->ctx, d->picture_shape(d->ctx, under), X, Y);
    d->select_default(d->ctx);
    d->blit_own(d->ctx, d->picture_shape(d->ctx, in->screen));
    d->flip(d->ctx);
    if ((int16_t)taken) {
        char path[REPLAY_BOX_PATH];
        snprintf(path, sizeof path, "%s%s.rpl", dir, name);
        memcpy(config + REPLAY_NAME, name, strlen(name) + 1);
        if (ops->file_exists(ops->ctx, path)) {
            int system = 0;
            for (size_t r = 0; r < sizeof SYSTEM / sizeof SYSTEM[0]; r++)
                if (file_list_same_name((const uint8_t *)name, SYSTEM[r])) system = 1;
            if (system)
                ops->banner_wait(ops->ctx, german ? "Kann system-weiderholung nicht ersetzen"
                                                  : "Cannot replace system replay", SAVED_SHOWN);
            else if (ops->ask(ops->ctx, german ? "Wiederholung ersetzen ?" : "Replace replay ?"))
                saved = ops->save(ops->ctx, path);
        } else {
            saved = ops->save(ops->ctx, path);
            ops->sound(ops->ctx, SOUND);
        }
        if ((int16_t)saved)
            ops->banner_wait(ops->ctx, german ? "Wiederholung gespeichert" : "Replay saved", SAVED_SHOWN);
    } else {
        ops->sound(ops->ctx, SOUND);
    }
    d->picture_free(d->ctx, under);
    d->picture_free(d->ctx, box);
    ops->pointer_redraw(ops->ctx, 0, 1);
}
