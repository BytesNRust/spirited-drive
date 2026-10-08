/* 004373c4: the front-end state machine (single-player path). */
#include "game/frontend/frontend.h"

#include <stdio.h>

enum {
    FRONTEND_RACE = 0,
    FRONTEND_QUIT = 1,
    FRONTEND_INTRO = 2,
    FRONTEND_TITLE_INSTANT = 3
};

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
                /* TODO(0042a010): load gamedata/config/config.dat; a saved
                 * config makes the title appear without its fade (state 3). */
                first = 0;
            }
            /* TODO(0045638c, 004640d0): start menu music. */
        }
        if (state == FRONTEND_INTRO || state == FRONTEND_TITLE_INSTANT) title_screen(state);

        const int choice = main_menu();
        if (choice == FRONTEND_INTRO) show_intro = 1;
        state = choice;
        if (choice == FRONTEND_RACE) {
            /* TODO(00466ae4, 00432ff4, 0042a504): race setup, race, save config. */
            fprintf(stderr, "frontend: race setup (00432ff4) not ported yet, back to the menu\n");
        }
    } while (state != FRONTEND_QUIT);
}
