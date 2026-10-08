/* 004373c4: the front-end state machine (single-player path). */
#include "game/frontend/frontend.h"

#include "game/frontend/config.h"

enum {
    FRONTEND_RACE = 0,
    FRONTEND_QUIT = 1,
    FRONTEND_INTRO = 2,
    FRONTEND_TITLE_INSTANT = 3
};

char frontend_player_name[42];

void frontend_run(void)
{
    int state = FRONTEND_INTRO;
    int first = 1;
    int show_intro = 1;
    do {
        if (show_intro) {
            /* TODO(004644f0, 00464568): stop menu music before the intro. */
            intro_sequence();
            show_intro = 0;
            if (first) {
                /* 0042a010: without a config (defaults in use) the title appears
                 * without its fade (state 3). */
                if (config_load()) state = FRONTEND_TITLE_INSTANT;
                first = 0;
            }
            /* TODO(0045638c, 004640d0): start menu music. */
        }
        if (state == FRONTEND_INTRO || state == FRONTEND_TITLE_INSTANT) title_screen(state);

        const int choice = main_menu();
        if (choice == FRONTEND_INTRO) show_intro = 1;
        state = choice;
        if (choice == FRONTEND_RACE) {
            /* TODO(00466ae4): resets the tournament and session state (0052ad9d..,
             * 0052b2b4.., 0052b2d6.., 0052b310) and backs up the 26 bytes at
             * 0052ad26 to 0052b298; none of that is ported yet. */
            const int result = control_central();
            config_save();  /* 0042a504 */
            if (result == FRONTEND_INTRO) {
                show_intro = 1;
                state = FRONTEND_INTRO;
            }
        }
    } while (state != FRONTEND_QUIT);
}
