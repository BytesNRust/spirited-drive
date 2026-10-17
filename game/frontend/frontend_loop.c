#include "game/frontend/frontend_loop.h"

#include <stdio.h>

enum { STATE_RACE = 0, STATE_QUIT = 1, STATE_INTRO = 2, STATE_DEFAULTS = 3 };

/* 004373c4's network branch */
static void network_loop(const FrontendLoopState *s, const FrontendLoopOps *ops)
{
    ops->title(ops->ctx, ops->config_load(ops->ctx) ? STATE_DEFAULTS : STATE_INTRO);
    if (!ops->network_found(ops->ctx))
        ops->fatal(ops->ctx, s->german ? "Keinen Netzwerk-Service gefunden." : "No network services found.");
    while (!ops->name_entry(ops->ctx)) {
        char text[0x20];
        if (s->german) snprintf(text, sizeof text, "Zur%cck zum system ?", 0x81);
        else snprintf(text, sizeof text, "Exit to system ?");
        if (ops->ask(ops->ctx, text)) return;
    }
    int32_t state;
    do {
        state = ops->network_game(ops->ctx);
        if (state == STATE_RACE) {
            *s->network_raced = 1;
            ops->game_reset(ops->ctx);
            state = ops->control_central(ops->ctx);
            ops->config_save(ops->ctx);
            if (*s->link) ops->link_close(ops->ctx);
        }
    } while (state != STATE_QUIT);
}

/* 004373c4 */
void frontend_loop(const FrontendLoopState *s, const FrontendLoopOps *ops)
{
    if (s->network) {
        network_loop(s, ops);
        return;
    }
    int32_t state = STATE_INTRO;
    int intro = 1, config_unread = 1;
    do {
        if (intro) {
            ops->music_fade_out(ops->ctx);
            ops->music_end(ops->ctx);
            ops->intro(ops->ctx);
            intro = 0;
            if (config_unread) {
                config_unread = 0;
                if (ops->config_load(ops->ctx)) state = STATE_DEFAULTS;
            }
            ops->after_intro(ops->ctx);
            ops->music_feed(ops->ctx);
        }
        if (state == STATE_INTRO || state == STATE_DEFAULTS) ops->title(ops->ctx, state);
        state = ops->main_menu(ops->ctx);
        if (state == STATE_INTRO) intro = 1;
        if (state == STATE_RACE) {
            ops->game_reset(ops->ctx);
            state = ops->control_central(ops->ctx);
            ops->config_save(ops->ctx);
        }
        if (*s->link) ops->link_close(ops->ctx);
    } while (state != STATE_QUIT);
}
